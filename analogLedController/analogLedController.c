#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
#include <linux/sched/signal.h> // for signals
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Habib JOMAA");
MODULE_DESCRIPTION("controlle led by button interruption and give user space some access");
MODULE_VERSION("1.0");

// in raspberry pi use the cmd "gpio readall" to display the gpio table.
// or the "pinout" cmd to display all information about the raspberry board, SoC info, ports info and GPIO table.
// use the BCM number.

static int ledPin = 18;
module_param(ledPin, int, S_IRUGO);
MODULE_PARM_DESC(ledPin, "gpio pin number for the led");

// led turns on
static int analogUp_Pin = 26;
module_param(analogUp_Pin, int, S_IRUGO);
MODULE_PARM_DESC(analogUp_Pin, "gpio pin number for the analog up");

// led turns off
static int analogDown_Pin = 19;
module_param(analogDown_Pin, int, S_IRUGO);
MODULE_PARM_DESC(analogDown_Pin, "gpio pin number for the analog down");

// increase frequenc
static int analogRight_Pin = 6;
module_param(analogRight_Pin, int, S_IRUGO);
MODULE_PARM_DESC(analogRight_Pin, "gpio pin number for the analog right");

// decrease frequency
static int analogLeft_Pin = 5;
module_param(analogLeft_Pin, int, S_IRUGO);
MODULE_PARM_DESC(analogLeft_Pin, "gpio pin number for the analog left");

// activate/desactivate blink mode
static int analogMid_Pin = 13;
module_param(analogMid_Pin, int, S_IRUGO);
MODULE_PARM_DESC(analogMid_Pin, "gpio pin number for the analog mid");

// analog array button
static struct gpio analog[] = {
	{.gpio = 26, .flags = GPIOF_DIR_IN, .label = "analogUp_Pin"},
	{.gpio = 19, .flags = GPIOF_DIR_IN, .label = "analogDown_Pin"},
	{.gpio = 6, .flags = GPIOF_DIR_IN, .label = "analogRight_Pin"},
	{ .gpio = 5, .flags = GPIOF_DIR_IN, .label = "analogLeft_Pin"},
	{.gpio = 13, .flags = GPIOF_DIR_IN, .label = "analogMid"}
};

static bool ledState = 0; // led on(1) or off(0)
module_param(ledState, bool, S_IRUGO);
MODULE_PARM_DESC(ledState, "ledState = 0 (default) mean if you press botton, led will turn on, if ledState = 1 mean the led will turn off if you press botton");

static int frequency = 1000;
module_param(frequency, int, S_IRUGO);
MODULE_PARM_DESC(frequency, "set the blink frequency");

// we will use a timer to measure the time between interruptions for debouncing, because this kernel doesn't implement the gpio_set_debounce() function (function return -524)
// time when the program start running. we will substruct this from the actual time in millis to avoid using a larg numebers
static unsigned int startTime = 0;
// store the last timer an interruption was handled.(we will not use an oldInterruptionTime for each interruption, because in real life a user will not be so fast that change analog direction less then 200 ms.)
static unsigned int oldInterruptionTime = 0;

// function return the run time in millis
static int runTimeMillis(void)
{
	struct timeval timeValue;
	uint64_t nowMillis;
	do_gettimeofday(&timeValue);
	nowMillis = (uint64_t)timeValue.tv_sec * (uint64_t)1000 + (uint64_t)(timeValue.tv_usec / 1000);
	return (uint32_t)nowMillis-startTime;
}

// thread to run the blink mode
static bool blinkMode = false;
static struct task_struct *blinkThread_p;

static int blinkThread(void *arg)
{
	unsigned int newTime, oldTime;
	bool stopMe = false;
	allow_signal(SIGKILL);
	while(!kthread_should_stop()){
		set_current_state(TASK_RUNNING);
		if(blinkMode == false){
			oldTime = runTimeMillis();
			ledState = !ledState;
			gpio_set_value(ledPin, ledState);
			do{
				msleep(10);
				newTime = runTimeMillis();
				// check for incomming signals, here too because the frequency may be in seconds, so we don't want to miss an signal come from user space (kill -9) or from kernel space. (kthread_stop)
				if(signal_pending(blinkThread_p) || kthread_should_stop()){
					stopMe = true;
				}
			}while((newTime - oldTime < frequency/2) && (stopMe == false));
		
		}
		set_current_state(TASK_INTERRUPTIBLE);
		
		// either blink mode was set or not we need to check for signals.
		if(signal_pending(blinkThread_p)){
			break;
		}	
	}
	do_exit(0);
	return 0;
}

static int simpleBlinkThread(void *arg)
{
	allow_signal(SIGKILL);
	while(!kthread_should_stop()){	
		if(blinkMode == true){
			set_current_state(TASK_RUNNING);
			ledState = !ledState;
			gpio_set_value(ledPin, ledState);
			set_current_state(TASK_INTERRUPTIBLE);
			msleep(frequency/2);	
		}
		// either blink mode was set or not we need to check for signals.
		if(signal_pending(blinkThread_p)){
			break;
		}	
	}
	do_exit(0);
	return 0;
}

/* interruption number for each analog direction */
static unsigned int irqUp;
static unsigned int irqDown;
static unsigned int irqRight;
static unsigned int irqLeft;
static unsigned int irqMid;

// the interruption handler turn led on when analog is up
static irq_handler_t irq_up_handler(unsigned int irq_number, void *dev_id, struct pt_regs *regs)
{
	unsigned int now = runTimeMillis();
	// after the first button press, we may have a multi bounces effect, to avoud those bounces we make sure that after the first press, the irq handler will not be available for 200 ms.
	// to make sure it's a rising edge, the gpio pin should be high.
	if((now - oldInterruptionTime > 200) && (gpio_get_value(analogUp_Pin) == 1))
	{
		//  after we release the button aftet 200 ms, we may face some bounces. those bounces can't be avoided so to check if this is a valid (real) intturption we need to wait for sometime to make sure that this interruption was called because we pressed a button and not just some bounces.
		// so the minimum time for a button press should be 5 ms at least. if after 5 ms we still have a high input level, that mean we are  still pressing the button and it('s not just a bounce.
		while(runTimeMillis() - now < 10);
		if(gpio_get_value(analogUp_Pin) == 1)
		{
			ledState = 1;
			gpio_set_value(ledPin, ledState);
			oldInterruptionTime = now;
		}
	}
	return (irq_handler_t) IRQ_HANDLED; // mean the irq handled correctly
}

// the interruption handler turn led off when analog is down
static irq_handler_t irq_down_handler(unsigned int irq_number, void *dev_id, struct pt_regs *regs)
{
	unsigned int now = runTimeMillis();
	// after the first button press, we may have a multi bounces effect, to avoud those bounces we make sure that after the first press, the irq handler will not be available for 200 ms.
	// to make sure it's a rising edge, the gpio pin should be high.
	if((now - oldInterruptionTime > 200) && (gpio_get_value(analogDown_Pin) == 1))
	{
		//  after we release the button aftet 200 ms, we may face some bounces. those bounces can't be avoided so to check if this is a valid (real) intturption we need to wait for sometime to make sure that this interruption was called because we pressed a button and not just some bounces.
		// so the minimum time for a button press should be 5 ms at least. if after 5 ms we still have a high input level, that mean we are  still pressing the button and it('s not just a bounce.
		while(runTimeMillis() - now < 10);
		if(gpio_get_value(analogDown_Pin) == 1)
		{
			ledState = 0;
			gpio_set_value(ledPin, ledState);
			oldInterruptionTime = now;
		}
	}
	return (irq_handler_t) IRQ_HANDLED; // mean the irq handled correctly
}

// the interruption handler increase frequency when analog is rigt
static irq_handler_t irq_right_handler(unsigned int irq_number, void *dev_id, struct pt_regs *regs)
{
	unsigned int now = runTimeMillis();
	// after the first button press, we may have a multi bounces effect, to avoud those bounces we make sure that after the first press, the irq handler will not be available for 200 ms.
	// to make sure it's a rising edge, the gpio pin should be high.
	if((now - oldInterruptionTime > 200) && (gpio_get_value(analogRight_Pin) == 1))
	{
		//  after we release the button aftet 200 ms, we may face some bounces. those bounces can't be avoided so to check if this is a valid (real) intturption we need to wait for sometime to make sure that this interruption was called because we pressed a button and not just some bounces.
		// so the minimum time for a button press should be 5 ms at least. if after 5 ms we still have a high input level, that mean we are  still pressing the button and it('s not just a bounce.
		while(runTimeMillis() - now < 10);
		if(gpio_get_value(analogRight_Pin) == 1)
		{
			frequency += 100;
			printk(KERN_INFO "frequency = %d", frequency);
			oldInterruptionTime = now;
		}
	}
	return (irq_handler_t) IRQ_HANDLED; // mean the irq handled correctly
}

// the interruption handler decrease frequency when analog is left
static irq_handler_t irq_left_handler(unsigned int irq_number, void *dev_id, struct pt_regs *regs)
{
	unsigned int now = runTimeMillis();
	// after the first button press, we may have a multi bounces effect, to avoud those bounces we make sure that after the first press, the irq handler will not be available for 200 ms.
	// to make sure it's a rising edge, the gpio pin should be high.
	if((now - oldInterruptionTime > 200) && (gpio_get_value(analogLeft_Pin) == 1))
	{
		//  after we release the button aftet 200 ms, we may face some bounces. those bounces can't be avoided so to check if this is a valid (real) intturption we need to wait for sometime to make sure that this interruption was called because we pressed a button and not just some bounces.
		// so the minimum time for a button press should be 5 ms at least. if after 5 ms we still have a high input level, that mean we are  still pressing the button and it('s not just a bounce.
		while(runTimeMillis() - now < 10);
		if(gpio_get_value(analogLeft_Pin) == 1)
		{
			frequency -= 100;
			printk(KERN_INFO "frequency = %d", frequency);
			oldInterruptionTime = now;
		}
	}
	return (irq_handler_t) IRQ_HANDLED; // mean the irq handled correctly
}

// the interruption handler activate/desactivate blink mode
static irq_handler_t irq_mid_handler(unsigned int irq_number, void *dev_id, struct pt_regs *regs)
{
	unsigned int now = runTimeMillis();
	// after the first button press, we may have a multi bounces effect, to avoud those bounces we make sure that after the first press, the irq handler will not be available for 200 ms.
	// to make sure it's a rising edge, the gpio pin should be high.
	if((now - oldInterruptionTime > 200) && (gpio_get_value(analogMid_Pin) == 1))
	{
		//  after we release the button aftet 200 ms, we may face some bounces. those bounces can't be avoided so to check if this is a valid (real) intturption we need to wait for sometime to make sure that this interruption was called because we pressed a button and not just some bounces.
		// so the minimum time for a button press should be 5 ms at least. if after 5 ms we still have a high input level, that mean we are  still pressing the button and it('s not just a bounce.
		while(runTimeMillis() - now < 10);
		if(gpio_get_value(analogMid_Pin) == 1)
		{
			blinkMode = !blinkMode;
			oldInterruptionTime = now;
		}
	}
	return (irq_handler_t) IRQ_HANDLED;
}



static int __init blinkLedInit(void)
{
	int result = 0;
	startTime = runTimeMillis();
	printk(KERN_INFO "Init module ...");
	/* LED */
	// check if the given gpio pin is a valide pin or not, before do anything.
	printk(KERN_INFO "prepare led ...");
	if(!gpio_is_valid(ledPin))
	{
		printk(KERN_ALERT "wrong gpio pin numer for led");
		return -ENODEV;
	}
	gpio_request(ledPin, "led pin");
	gpio_export(ledPin, false);
	gpio_direction_output(ledPin, ledState);
	
	/* ANALOG */
	printk(KERN_INFO "prepare analog ...");
	if(!gpio_is_valid(analogUp_Pin) || !gpio_is_valid(analogDown_Pin) || !gpio_is_valid(analogRight_Pin) || !gpio_is_valid(analogLeft_Pin) || !gpio_is_valid(analogMid_Pin))
	{
		printk(KERN_ALERT "invalid gpio pin numbers for analog");
		return -ENODEV;
	}
	
	gpio_export(analogUp_Pin, false);
	gpio_export(analogDown_Pin, false);
	gpio_export(analogRight_Pin, false);
	gpio_export(analogLeft_Pin, false);
	gpio_export(analogMid_Pin, false);
	
	gpio_request_array(analog, ARRAY_SIZE(analog));
	
	// configure interruptions
	printk(KERN_INFO "prepare interruptions ...");
	irqUp = gpio_to_irq(analogUp_Pin);
	result = request_irq(irqUp, (irq_handler_t) irq_up_handler, IRQF_TRIGGER_RISING, "up_irq_handler", NULL);
	
	irqDown = gpio_to_irq(analogDown_Pin);
	result = request_irq(irqDown, (irq_handler_t) irq_down_handler, IRQF_TRIGGER_RISING, "down_irq_handler", NULL);
	
	irqRight = gpio_to_irq(analogRight_Pin);
	result = request_irq(irqRight, (irq_handler_t) irq_right_handler, IRQF_TRIGGER_RISING, "right_irq_handler", NULL);
	
	irqLeft = gpio_to_irq(analogLeft_Pin);
	result = request_irq(irqLeft, (irq_handler_t) irq_left_handler, IRQF_TRIGGER_RISING, "left_irq_handler", NULL);
	
	irqMid = gpio_to_irq(analogMid_Pin);
	result = request_irq(irqMid, (irq_handler_t) irq_mid_handler, IRQF_TRIGGER_RISING, "mid_irq_handler", NULL);
	
	// launche the blink thread
	printk(KERN_INFO "prepare thread ...");
	blinkThread_p = kthread_run(simpleBlinkThread, NULL, "blink_led");
	if(IS_ERR(blinkThread_p)){
		printk(KERN_ALERT "failed to start blink led thead");
		result = PTR_ERR(blinkThread_p);
	}
	
	printk(KERN_INFO "Init module done.");
	return result;
}

static void __exit blinkLedExit(void)
{
	printk(KERN_INFO "Prepare module to exit ...");
	/* Thread */
	printk(KERN_INFO "stop thread!");
	if(!IS_ERR(blinkThread_p)){
		kthread_stop(blinkThread_p);
	}
	/* LED */
	// unexport the gpio<pin number>. this will delete the gpio<pin number> from sysfs
	printk(KERN_INFO "stop led!");
	gpio_unexport(ledPin);
	// release the allocated (claimed) gpio pin.
	gpio_free(ledPin);
	/* BUTTON */
	printk(KERN_INFO "stop button!");
	free_irq(irqUp, NULL);
	free_irq(irqDown, NULL);
	free_irq(irqRight, NULL);
	free_irq(irqLeft, NULL);
	free_irq(irqMid, NULL);
	gpio_unexport(analogUp_Pin);
	gpio_unexport(analogDown_Pin);
	gpio_unexport(analogRight_Pin);
	gpio_unexport(analogLeft_Pin);
	gpio_unexport(analogMid_Pin);
	gpio_free_array(analog, ARRAY_SIZE(analog));
	printk(KERN_INFO "Exit module.");
}

module_init(blinkLedInit);
module_exit(blinkLedExit);
