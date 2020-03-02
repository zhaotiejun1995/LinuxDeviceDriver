#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

static struct class *irqdrv_class;
static struct class_device *irqdrv_class_dev;

// gpf0,2
volatile unsigned long *gpfcon;
volatile unsigned long *gpfdat;
// gpg3,11
volatile unsigned long *gpgcon;
volatile unsigned long *gpgdat;

static DECLARE_WAIT_QUEUE_HEAD(irq_waitq);
static volatile int ev_press = 0;

struct pin_desc{
    unsigned int pin;
    unsigned int key_val;
};

struct pin_desc pins_desc[3] = {
    {S3C2410_GPF0,  0x01},
    {S3C2410_GPF2,  0x02},
    {S3C2410_GPG3,  0x03}, 
//    {S3C2410_GPG11, 0x04},    
};
static unsigned char key_val;


static irqreturn_t keys_irq(int irq, void *dev_id)
{
    struct pin_desc *pindesc = (struct pin_desc *)dev_id;
    unsigned int pinval;

    pinval = s3c2410_gpio_getpin(pindesc->pin);
    if(pinval)
    {
        key_val = 0x80 | pindesc->key_val;
    }
    else
    {
        key_val = pindesc->key_val;
    }

    ev_press = 1;
    wake_up_interruptible(&irq_waitq);
    
    return IRQ_RETVAL(IRQ_HANDLED);
}

static int irq_drv_open(struct inode *inode, struct file *file)
{
    request_irq(IRQ_EINT0,  keys_irq, IRQT_BOTHEDGE, "s2", &pins_desc[0]);
    request_irq(IRQ_EINT2,  keys_irq, IRQT_BOTHEDGE, "s3", &pins_desc[1]);
    request_irq(IRQ_EINT11, keys_irq, IRQT_BOTHEDGE, "s4", &pins_desc[2]);
//    request_irq(IRQ_EINT19, keys_irq, IRQT_BOTHEDGE, "s5", &pins_desc[3]);
    
    return 0;
}

ssize_t irq_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    if(size != 1)
        return -EINVAL;

    /* 判断是否有按键按下并进行休眠选择 */
    wait_event_interruptible(irq_waitq, ev_press);

    copy_to_user(buf, &key_val, 1);
    ev_press = 0;
    
    return 1;
}

int irq_drv_release(struct inode *inode, struct file *file)
{
    free_irq(IRQ_EINT0,  &pins_desc[0]);
    free_irq(IRQ_EINT2,  &pins_desc[1]);
    free_irq(IRQ_EINT11, &pins_desc[2]);
//    free_irq(IRQ_EINT19, &pins_desc[3]);    
    return 0;
}


static struct file_operations irq_drv_fops = 
{
    .owner   = THIS_MODULE,
    .open    = irq_drv_open,
    .read    = irq_drv_read,
    .release = irq_drv_release,
};


int major;
static int irq_drv_init(void)
{
    major = register_chrdev(major, "irq_drv", &irq_drv_fops);
    
    irqdrv_class = class_create(THIS_MODULE, "irqdrv");
    irqdrv_class_dev = class_device_create(irqdrv_class, NULL, MKDEV(major, 0), NULL, "irq");

    gpfcon = (volatile unsigned long *)ioremap(0x56000050, 16);
    gpfdat = gpfcon + 1;

    gpgcon = (volatile unsigned long *)ioremap(0x56000060, 16);
    gpgdat = gpgcon + 1;
    
    return 0;
}

static void irq_drv_exit(void)
{
    class_device_unregister(irqdrv_class_dev);
    class_destroy(irqdrv_class);
    
    unregister_chrdev(major, "irq_drv");

    iounmap(gpfcon);
    iounmap(gpgcon);
}

module_init(irq_drv_init);
module_exit(irq_drv_exit);

MODULE_LICENSE("GPL");


