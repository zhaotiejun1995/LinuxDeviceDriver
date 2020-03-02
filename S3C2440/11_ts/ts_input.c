#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/plat-s3c24xx/ts.h>

#include <asm/arch/regs-adc.h>
#include <asm/arch/regs-gpio.h>

struct s3c_ts_regs {
    unsigned long adccon;
    unsigned long adctsc;
    unsigned long adcdly;
    unsigned long adcdat0;
    unsigned long adcdat1;
    unsigned long adcdat2;
    unsigned long adcupdn;
};

static struct input_dev *s3c_ts_dev;
static volatile struct s3c_ts_regs *s3c_ts_regs;

static struct timer_list ts_timer;

static void enter_wait_down_mode(void)
{
    s3c_ts_regs->adctsc = 0xd3;
}

static void enter_wait_up_mode(void)
{
    s3c_ts_regs->adctsc = 0x1d3;
}

static void enter_measure_xy_mode(void)
{
    s3c_ts_regs->adctsc = (1<<3)|(1<<2);
}

static void start_adc(void)
{
    s3c_ts_regs->adccon |= (1<<0);
}

static int s3c_filter_ts(int x[], int y[])
{
#define ERR_LIMIT 10

	int avr_x, avr_y;
	int det_x, det_y;

	avr_x = (x[0] + x[1])/2;
	avr_y = (y[0] + y[1])/2;

	det_x = (x[2] > avr_x) ? (x[2] - avr_x) : (avr_x - x[2]);
	det_y = (y[2] > avr_y) ? (y[2] - avr_y) : (avr_y - y[2]);

	if ((det_x > ERR_LIMIT) || (det_y > ERR_LIMIT))
		return 0;

	avr_x = (x[1] + x[2])/2;
	avr_y = (y[1] + y[2])/2;

	det_x = (x[3] > avr_x) ? (x[3] - avr_x) : (avr_x - x[3]);
	det_y = (y[3] > avr_y) ? (y[3] - avr_y) : (avr_y - y[3]);

	if ((det_x > ERR_LIMIT) || (det_y > ERR_LIMIT))
		return 0;
	
	return 1;   
}

static irqreturn_t enter_ts_irq(int irq, void *dev_id)
{
    if(s3c_ts_regs->adcdat0 & (1<<15))
    {
//        printk("pen up\n");
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		input_report_key(s3c_ts_dev, BTN_TOUCH, 0);
		input_sync(s3c_ts_dev);
        enter_wait_down_mode();
    }
    else
    {
//        printk("pen down\n");
//        enter_wait_up_mode();
        enter_measure_xy_mode();
        start_adc();
    }
    return IRQ_HANDLED;
}

static irqreturn_t adc_ts_irq(int irq, void *dev_id)
{
	static int cnt = 0;
	static int adc_x[4], adc_y[4];
	int adcdat0, adcdat1;
	
	/* 优化2: 如果ADC完成时，发现触摸笔已经松开，测丢弃本次结果 */
    adcdat0 = s3c_ts_regs->adcdat0;
    adcdat1 = s3c_ts_regs->adcdat1;

    if(s3c_ts_regs->adcdat0 & (1<<15))
    {
        /*松开*/
        cnt = 0;
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		input_report_key(s3c_ts_dev, BTN_TOUCH, 0);
		input_sync(s3c_ts_dev);
        enter_wait_down_mode();
    }
    else
    {
//	    printk("adc_irq cnt = %d, x = %d, y = %d\n", ++cnt, adcdat0 & 0x3ff, adcdat1 & 0x3ff);
        /* 优化3: 多次测量求平均值 */
        adc_x[cnt] = adcdat0 & 0x3ff;
        adc_y[cnt] = adcdat1 & 0x3ff;
        ++cnt;
        if(cnt == 4)
        {
            /* 优化4: 软件过滤*/
            if(s3c_filter_ts(adc_x, adc_y))
            {
//                printk("x = %d, y = %d\n", (adc_x[0]+adc_x[1]+adc_x[2]+adc_x[3])/4, \
//                    (adc_y[0]+adc_y[1]+adc_y[2]+adc_y[3])/4);
     			input_report_abs(s3c_ts_dev, ABS_X, (adc_x[0]+adc_x[1]+adc_x[2]+adc_x[3])/4);
     			input_report_abs(s3c_ts_dev, ABS_Y, (adc_y[0]+adc_y[1]+adc_y[2]+adc_y[3])/4);
     			input_report_abs(s3c_ts_dev, ABS_PRESSURE, 1);
     			input_report_key(s3c_ts_dev, BTN_TOUCH, 1);
				input_sync(s3c_ts_dev);
            }    
            cnt = 0;
	        enter_wait_up_mode();

	        /* 启动定时器 */
	        mod_timer(&ts_timer, jiffies + HZ/100);
        }
        else
        {
            enter_measure_xy_mode();
            start_adc();
        }
    }  
	return IRQ_HANDLED;
}

static void ts_timer_fun(unsigned long data)
{
    if(s3c_ts_regs->adcdat0 & (1<<15))
    {
        /* 已经松开 */
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		input_report_key(s3c_ts_dev, BTN_TOUCH, 0);
		input_sync(s3c_ts_dev);
        enter_wait_down_mode();
    }
    else
    {
        enter_measure_xy_mode();
        start_adc();
    }
}

static int s3c_ts_init(void)
{
    struct clk *clk;
    /*1.分配一个input_dev结构体 */
    s3c_ts_dev = input_allocate_device();
    
    /*2.设置*/
    /*2.1 设置发生事件类型 :按键类型*/
    set_bit(EV_KEY, s3c_ts_dev->evbit); 
    set_bit(EV_ABS, s3c_ts_dev->evbit); // 绝对位移事件  
    
    /*2.2 设置按键类型里的事件*/
    set_bit(BTN_TOUCH, s3c_ts_dev->keybit);
	input_set_abs_params(s3c_ts_dev, ABS_X, 0, 0x3FF, 0, 0); // 3FF : 10为AD转换
	input_set_abs_params(s3c_ts_dev, ABS_Y, 0, 0x3FF, 0, 0);
	input_set_abs_params(s3c_ts_dev, ABS_PRESSURE, 0, 1, 0, 0);// 压力

    /*3.注册*/
    input_register_device(s3c_ts_dev);
    
    /*4.硬件相关的操作*/  
    /*4.1使能时钟*/
    clk = clk_get(NULL, "adc");
    clk_enable(clk);
    /*4.2配置寄存器*/
    s3c_ts_regs = ioremap(0x58000000, sizeof(struct s3c_ts_regs));
	/* bit[14]  : 1-A/D converter prescaler enable
	 * bit[13:6]: A/D converter prescaler value,
	 *            49, ADCCLK=PCLK/(49+1)=50MHz/(49+1)=1MHz
	 * bit[0]: A/D conversion starts by enable. 先设为0
	 */
    s3c_ts_regs->adccon = (1<<14) | (49<<6);
    
    request_irq(IRQ_TC, enter_ts_irq, IRQF_SAMPLE_RANDOM, "ts_pen", NULL);
    request_irq(IRQ_ADC, adc_ts_irq, IRQF_SAMPLE_RANDOM, "adc", NULL);

    /*优化1: 设置ADCDLY为最大值，使得电压稳定后再发出IRQ_TC中断 */
    s3c_ts_regs->adcdly = 0xffff;
    
    /* 使用定时器处理长按滑动问题 */
    init_timer(&ts_timer);
    ts_timer.function = ts_timer_fun;
    add_timer(&ts_timer);
    
    enter_wait_down_mode();

    return 0;
    
}

static void s3c_ts_exit(void)
{
    free_irq(IRQ_ADC, NULL);
    free_irq(IRQ_TC, NULL);
    iounmap(s3c_ts_regs);
    input_unregister_device(s3c_ts_dev);
    input_free_device(s3c_ts_dev);
    del_timer(&ts_timer);
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);
MODULE_LICENSE("GPL");


