/*  参考:drivers\input\keyboard\Gpio_keys.c  */

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
#include <linux/gpio_keys.h>

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>

struct pin_desc{
    int irq;
    char *name;
    unsigned int pin;
    unsigned int key_val;
};

struct pin_desc pins_desc[3] = {
    {IRQ_EINT0,  "s2", S3C2410_GPF0, KEY_L},
    {IRQ_EINT2,  "s3", S3C2410_GPF2, KEY_S},
    {IRQ_EINT11, "s4", S3C2410_GPG3, KEY_ENTER},   
};
static struct input_dev *key_dev;
static struct pin_desc *irq_pd;
static struct timer_list key_timer;

static irqreturn_t keys_irq(int irq, void *dev_id)
{
    irq_pd = (struct pin_desc *)dev_id;
    mod_timer(&key_timer, jiffies+HZ/100);// jiffies + 10ms
    return IRQ_RETVAL(IRQ_HANDLED);
}

static void key_timer_fun(unsigned long time)
{   
    struct pin_desc *pindesc = irq_pd;
    unsigned int pinval;

    if(!pindesc)
        return;

    pinval = s3c2410_gpio_getpin(pindesc->pin);
    if(pinval)
    {
        input_event(key_dev, EV_KEY, pindesc->key_val, 0);
        input_sync(key_dev);
    }
    else
    {
        input_event(key_dev, EV_KEY, pindesc->key_val, 1);
        input_sync(key_dev);        
    }   
}

static int key_input_init(void)
{
    int i;
    /*1.分配一个input_dev结构体 */
    key_dev = input_allocate_device();
    
    /*2.设置*/
    /*2.1 设置发生事件类型 :按键类型*/
    set_bit(EV_KEY, key_dev->evbit);
    set_bit(EV_REP, key_dev->evbit);
    /*2.2 设置按键类型里的事件*/
    set_bit(KEY_L,     key_dev->keybit);
    set_bit(KEY_S,     key_dev->keybit);
    set_bit(KEY_ENTER, key_dev->keybit);
    
    /*3.注册*/
    input_register_device(key_dev);
    
    /*4.硬件相关的操作*/
    init_timer(&key_timer);
    key_timer.function = key_timer_fun;
    add_timer(&key_timer);
    for(i = 0; i< 4; i++)
    {
        request_irq(pins_desc[i].irq, keys_irq, IRQT_BOTHEDGE, pins_desc[i].name, &pins_desc[i]);
    }
    return 0;
}

static void key_input_exit(void)
{
    int i;
    for(i = 0; i< 4; i++)
    {
        free_irq(pins_desc[i].irq, &pins_desc[i]);
    }
    del_timer(&key_timer);
    input_unregister_device(key_dev);
    input_free_device(key_dev);
}

module_init(key_input_init);
module_exit(key_input_exit);
MODULE_LICENSE("GPL");


