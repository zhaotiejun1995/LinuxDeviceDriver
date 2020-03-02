#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

static struct input_dev *usb_kbd_dev;         //input_dev
static unsigned char *usb_kbd_buf;            //虚拟地址缓存区
static dma_addr_t usb_kbd_phyc;               //DMA缓存区;
static int usb_kbd_len;                       //数据包长度
static struct urb *usb_kbd_urb;              //urb

static const unsigned char usb_kbd_keycode[252] = {
     0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
    50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
     4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
    27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
    65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
    105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
    72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
    191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
    115,114,  0,  0,  0,121,  0, 89, 93,124, 92, 94, 95,  0,  0,  0,
    122,123, 90, 91, 85,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
    150,158,159,128,136,177,178,176,142,152,173,140
};  //键盘码表共有252个数据

 
static void myusb_kbd_irq(struct urb *urb)               //键盘中断函数
{
    static unsigned char buf1[8]={0,0,0,0,0,0,0,0};
    int i;

    /*上传左右crtl、shift、atl、windows按键*/
    for (i = 0; i < 8; i++)
    {
        if(((usb_kbd_buf[0]>>i)&1)!=((buf1[0]>>i)&1))
        {    
            input_report_key(usb_kbd_dev, usb_kbd_keycode[i + 224], (usb_kbd_buf[0]>> i) & 1);
            input_sync(usb_kbd_dev);             //上传同步事件
        }     
    }

     /*上传普通按键*/
    for(i=2;i<8;i++)
    {
        if(usb_kbd_buf[i]!=buf1[i])
        {
            if(usb_kbd_buf[i] )      //按下事件
            {
                input_report_key(usb_kbd_dev,usb_kbd_keycode[usb_kbd_buf[i]], 1); 
                input_sync(usb_kbd_dev);             //上传同步事件                
            }
            else if(buf1[i])         //松开事件
            {
                input_report_key(usb_kbd_dev,usb_kbd_keycode[buf1[i]], 0);
                input_sync(usb_kbd_dev);             //上传同步事件                
            }
        }        
    }
    memcpy(buf1, usb_kbd_buf, 8);    //更新数据    
    usb_submit_urb(usb_kbd_urb, GFP_KERNEL);
}

static int usb_kbd_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    volatile unsigned char  i;
    struct usb_device *dev = interface_to_usbdev(intf);                           
    struct usb_endpoint_descriptor *endpoint;                            
    struct usb_host_interface *interface;                                           
    int pipe;                                                                        
    interface=intf->cur_altsetting;                                                                   
    endpoint = &interface->endpoint[0].desc;                                        

	/* a. 分配一个input_dev */
    usb_kbd_dev=input_allocate_device();

	/* b. 设置 */
	/* b.1 能产生哪类事件 */
	set_bit(EV_KEY, usb_kbd_dev->evbit);
    set_bit(EV_REP, usb_kbd_dev->evbit);  
    
	/* b.2 能产生哪些事件 */
    for (i = 0; i < 252; i++)
    set_bit(usb_kbd_keycode[i], usb_kbd_dev->keybit);     //添加所有键
    clear_bit(0, usb_kbd_dev->keybit);

	/* c. 注册 */
    input_register_device(usb_kbd_dev);

	/* d. 硬件相关操作 */
	/* 数据传输3要素: 源,目的,长度 */
	
	/* 源: USB设备的某个端点 */
    pipe=usb_rcvintpipe(dev,endpoint->bEndpointAddress); 

	/* 长度: */
    usb_kbd_len=endpoint->wMaxPacketSize;
    
	/* 目的: */
    usb_kbd_buf=usb_buffer_alloc(dev,usb_kbd_len,GFP_ATOMIC,&usb_kbd_phyc);


	/* 使用"3要素" */
	/* 分配usb request block */
	usb_kbd_urb=usb_alloc_urb(0,GFP_KERNEL);
	
	/* 使用"3要素设置urb" */	
    usb_fill_int_urb (usb_kbd_urb, dev, pipe, usb_kbd_buf, usb_kbd_len, myusb_kbd_irq, 0, endpoint->bInterval);             
    usb_kbd_urb->transfer_dma = usb_kbd_phyc;                  //设置DMA地址
    usb_kbd_urb->transfer_flags = URB_NO_TRANSFER_DMA_MAP;     //设置使用DMA地址

	/* 使用URB */
    usb_submit_urb(usb_kbd_urb, GFP_KERNEL);   
    return 0;
}

static void usb_kbd_disconnect(struct usb_interface *intf)
{
    struct usb_device *dev = interface_to_usbdev(intf);      
    usb_kill_urb(usb_kbd_urb);
    usb_free_urb(usb_kbd_urb);
    usb_buffer_free(dev, usb_kbd_len, usb_kbd_buf,usb_kbd_phyc);
    input_unregister_device(usb_kbd_dev);                  //注销内核中的input_dev
    input_free_device(usb_kbd_dev);                        //释放input_dev
}

static struct usb_device_id usb_kbd_id_table [] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_KEYBOARD) },
	{ }						/* Terminating entry */
};

static struct usb_driver usb_kbd_drv = {
    .name        = "usbkbd_drv",
    .probe       = usb_kbd_probe,                        
    .disconnect  = usb_kbd_disconnect,
    .id_table    = usb_kbd_id_table,
};

/*入口函数*/
static int usb_kbd_init(void)
{ 
    usb_register(&usb_kbd_drv);
    return 0;
}
 
/*出口函数*/
static void usb_kbd_exit(void)
{
    usb_deregister(&usb_kbd_drv);
}

module_init(usb_kbd_init);
module_exit(usb_kbd_exit);
MODULE_LICENSE("GPL");

