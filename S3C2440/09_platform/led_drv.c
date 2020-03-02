#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>

static int major;
static volatile unsigned long *gpfcon;
static volatile unsigned long *gpfdat;
static int pin;

static struct class *platform_led_class;

static int platform_led_open(struct inode *inode, struct file *file)
{
    /* 配置为输出 */
    *gpfcon &= ~(0x3<<(pin*2));
    *gpfcon |= (0x1<<(pin*2));
    
    return 0;
}

static ssize_t platform_led_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
    int val, num;
    copy_from_user(&val, buf, count);

    num = (val & 0x3) - 1;
    if(val & 0x10)
    {
        *gpfdat &= ~(1<<(pin + num));
    }
    else
    {
        *gpfdat |= (1<<(pin + num));
    }
    return 0;
}

static struct file_operations platform_led_fops = {
    .owner = THIS_MODULE,
    .open  = platform_led_open,
    .write = platform_led_write,
};

static int led_drv_probe(struct platform_device *pdev)
{
    struct resource *res;
    /* 根据plaform_device的资源进行ioremap */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    
    gpfcon = ioremap(res->start, res->end - res->start + 1);
    gpfdat = gpfcon + 1;

    res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
    pin = res->start;

    /* 注册字符设备驱动  */
    printk("led_probe, found led\n");
    major = register_chrdev(major, "platform_led", &platform_led_fops);

    platform_led_class = class_create(THIS_MODULE, "platformled");
    class_device_create(platform_led_class, NULL, MKDEV(major, 0), NULL, "led");

    return 0;
    
}

static int led_drv_remove(struct platform_device *pdev)
{
    /* 卸载字符设备驱动程序 */
    /*iounmap*/
    printk("led_remove, not found led\n");
    class_device_destroy(platform_led_class, MKDEV(major, 0));
    class_destroy(platform_led_class);
    unregister_chrdev(major, "platform_led");
    iounmap(gpfcon);

    return 0;
}


static struct platform_driver led_drv = {
    .probe		= led_drv_probe,
    .remove		= led_drv_remove,
    .driver		= {
    .name	= "led",
    }   
};

static int led_drv_init(void)
{
    platform_driver_register(&led_drv);
    return 0;
}

static void led_drv_exit(void)
{
    platform_driver_unregister(&led_drv);
}

module_init(led_drv_init);
module_exit(led_drv_exit);
MODULE_LICENSE("GPL");



