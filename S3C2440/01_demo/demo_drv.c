#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

static struct class *demodrv_class;
static struct class_device *demodrv_class_devs;

volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;

static int demo_drv_open(struct inode *inode, struct file *file)
{
//    printk("demo_drv_open\n");

    *gpfcon &= ~((0x3<<(4*2)) | (0x3<<(5*2)) | (0x3<<(6*2)));
    *gpfcon |= ((0x1<<(4*2)) | (0x1<<(5*2)) | (0x1<<(6*2)));
    
    return 0;
}

static ssize_t demo_drv_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    int val;    
//    printk("demo_drv_write\n");

    copy_from_user(&val, buf, count);

    if(val == 1)
    {
        *gpfdat &= ~((1<<4) | (1<<5) | (1<<6));
    }
    else
    {     
        *gpfdat |= ((1<<4) | (1<<5) | (1<<6));
    }

    return 0;
}

static struct file_operations demo_drv_fops = {
    .owner = THIS_MODULE,
    .open  = demo_drv_open,
    .write = demo_drv_write,
};
int major;
int demo_drv_init(void)
{
    major = register_chrdev(0, "demo_drv", &demo_drv_fops);

    demodrv_class = class_create(THIS_MODULE, "demodrv");
//    if(IS_ERR(demodrv_class))
//        return PTR_ERR(demodrv_class);
    demodrv_class_devs = class_device_create(demodrv_class,NULL, MKDEV(major, 0), NULL, "demo");
//    if(unlikely(IS_ERR(demodrv_class_devs)))  
//        return PTR_ERR(demodrv_class_devs);

    gpfcon = (unsigned long *)ioremap(0x56000050, 16);
    gpfdat = gpfcon + 1;
        
    return 0;
}

void demo_drv_exit(void)
{  
    unregister_chrdev(major, "demo_drv");
    
    class_device_unregister(demodrv_class_devs);
    class_destroy(demodrv_class);
    
    iounmap(gpfcon);
}

module_init(demo_drv_init);
module_exit(demo_drv_exit);


MODULE_LICENSE("GPL");

