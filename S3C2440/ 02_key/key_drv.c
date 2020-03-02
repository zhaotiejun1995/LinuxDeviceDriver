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

static struct class *keydrv_class;
static struct class_device *keydrv_class_dev;

volatile unsigned long *gpfcon;
volatile unsigned long *gpfdat;

volatile unsigned long *gpgcon;
volatile unsigned long *gpgdat;

static int key_drv_open(struct inode *inode, struct file *file)
{
    //配置gpf0,2为输入引脚
    *gpfcon &= ~((0x3<<(0*2)) | (0x3<<(2*2)));
    
    //配置gpg3,11为输入引脚
    *gpgcon &= ~((0x3<<(3*2)) | (0x3<<(11*2)));
    
    return 0;
}

static ssize_t key_drv_read(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    //返回四个引脚的电平
    unsigned char key_vals[4];
    int regval;

    if(size != sizeof(key_vals))
        return -EINVAL;
        
    regval = *gpfdat;
    key_vals[0] = (regval & (1<<0)) ? 1 : 0;
    key_vals[1] = (regval & (1<<2)) ? 1 : 0;

    regval = *gpgdat;
    key_vals[2] = (regval & (1<<3)) ? 1 : 0;
    key_vals[3] = (regval & (1<<11)) ? 1 : 0; 

    copy_to_user(buf, key_vals, sizeof(key_vals));

    return sizeof(key_vals);
}

static struct file_operations key_drv_fops = {
    .owner = THIS_MODULE,
    .open  = key_drv_open,
    .read  = key_drv_read,
};

int major;
static int key_drv_init(void)
{
    major = register_chrdev(0, "key_drv", &key_drv_fops);

    keydrv_class = class_create(THIS_MODULE, "keydrv");
    keydrv_class_dev = class_device_create(keydrv_class,NULL, MKDEV(major, 0), NULL, "key");

    gpfcon = (volatile unsigned long *)ioremap(0x56000050, 16);
    gpfdat = gpfcon + 1;

    gpgcon = (volatile unsigned long *)ioremap(0x56000060, 16);
    gpgdat = gpgcon + 1;
    
    return 0;
}

static void key_drv_exit(void)
{  
    unregister_chrdev(major, "key_drv");
    
    class_device_unregister(keydrv_class_dev);
    class_destroy(keydrv_class);

    iounmap(gpfcon);
    iounmap(gpgcon);
}

module_init(key_drv_init);
module_exit(key_drv_exit);


MODULE_LICENSE("GPL");

