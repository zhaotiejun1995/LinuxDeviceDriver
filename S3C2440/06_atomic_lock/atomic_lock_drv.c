#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

static struct class *atomic_lockdrv_class;
static struct class_device *atomic_lockdrv_class_dev;

// gpf0,2
volatile unsigned long *gpfcon;
volatile unsigned long *gpfdat;
// gpg3,11
volatile unsigned long *gpgcon;
volatile unsigned long *gpgdat;

static DECLARE_WAIT_QUEUE_HEAD(irq_waitq);
static volatile int ev_press = 0;

static struct fasync_struct *signal_fasync;

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

#if 0   //定义原子操作
static atomic_t canopen = ATOMIC_INIT(1);
#else   //定义互斥锁
static DECLARE_MUTEX(key_lock);
#endif

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

    kill_fasync(&signal_fasync, SIGIO, POLL_IN);
    
    return IRQ_RETVAL(IRQ_HANDLED);
}

static int atomic_lock_drv_open(struct inode *inode, struct file *file)
{
//申请资源
#if 0
    if(!atomic_dec_and_test(&canopen))
    {
        atomic_inc(&canopen);
        return -EBUSY;
    }
#else
    if(file->f_flags & O_NONBLOCK)// 非阻塞，获取不到资源则立即返回
    {
        if(down_trylock(&key_lock))
            return -EBUSY;
    }
    else
    {
        down(&key_lock);    
    }
#endif
   
    request_irq(IRQ_EINT0,  keys_irq, IRQT_BOTHEDGE, "s2", &pins_desc[0]);
    request_irq(IRQ_EINT2,  keys_irq, IRQT_BOTHEDGE, "s3", &pins_desc[1]);
    request_irq(IRQ_EINT11, keys_irq, IRQT_BOTHEDGE, "s4", &pins_desc[2]);
//    request_irq(IRQ_EINT19, keys_irq, IRQT_BOTHEDGE, "s5", &pins_desc[3]);
    
    return 0;
}

ssize_t atomic_lock_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    if(size != 1)
        return -EINVAL;

    if(file->f_flags & O_NONBLOCK)    
    {
        if(!ev_press)
            return -EAGAIN;
    }
    else
    {
        /* 判断是否有按键按下并进行休眠选择 */
        wait_event_interruptible(irq_waitq, ev_press);
    }

    copy_to_user(buf, &key_val, 1);
    ev_press = 0;
    
    return 1;
}

int atomic_lock_drv_release(struct inode *inode, struct file *file)
{
//释放资源
#if 0
    atomic_inc(&canopen);
#else
    up(&key_lock);
#endif
    free_irq(IRQ_EINT0,  &pins_desc[0]);
    free_irq(IRQ_EINT2,  &pins_desc[1]);
    free_irq(IRQ_EINT11, &pins_desc[2]);
//    free_irq(IRQ_EINT19, &pins_desc[3]);    
    return 0;
}

static unsigned int atomic_lock_drv_poll(struct file *file, struct poll_table_struct *wait)
{   //sys_poll
    unsigned int mask = 0;

    poll_wait(file, &irq_waitq, wait);
    
    if(ev_press)
        mask |= POLLIN | POLLRDNORM;

    return mask;    
}

static int atomic_lock_drv_fasync(int fd, struct file *filp, int on)
{
    printk("driver: atomic_lock_drv_fasync\n");
    return fasync_helper (fd, filp, on, &signal_fasync);
}

static struct file_operations atomic_lock_drv_fops = 
{
    .owner   = THIS_MODULE,
    .open    = atomic_lock_drv_open,
    .read    = atomic_lock_drv_read,
    .release = atomic_lock_drv_release,
    .poll    = atomic_lock_drv_poll,
    .fasync  = atomic_lock_drv_fasync,
};


int major;
static int atomic_lock_drv_init(void)
{
    major = register_chrdev(major, "atomic_lock_drv", &atomic_lock_drv_fops);
    
    atomic_lockdrv_class = class_create(THIS_MODULE, "atomic_lockdrv");
    atomic_lockdrv_class_dev = class_device_create(atomic_lockdrv_class, NULL, MKDEV(major, 0), NULL, "atomic_lock");

    gpfcon = (volatile unsigned long *)ioremap(0x56000050, 16);
    gpfdat = gpfcon + 1;

    gpgcon = (volatile unsigned long *)ioremap(0x56000060, 16);
    gpgdat = gpgcon + 1;
    
    return 0;
}

static void atomic_lock_drv_exit(void)
{
    class_device_unregister(atomic_lockdrv_class_dev);
    class_destroy(atomic_lockdrv_class);
    
    unregister_chrdev(major, "atomic_lock_drv");

    iounmap(gpfcon);
    iounmap(gpgcon);
}

module_init(atomic_lock_drv_init);
module_exit(atomic_lock_drv_exit);

MODULE_LICENSE("GPL");


