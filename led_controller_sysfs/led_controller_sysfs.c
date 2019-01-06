#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h> // to use the gpio driver
#include <linux/interrupt.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
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

static int buttonPin = 13;
module_param(buttonPin, int, S_IRUGO);
MODULE_PARM_DESC(buttonPin, "gpio pin number for the button");

static bool ledState = 0; // led on(1) or off(0)
module_param(ledState, bool, S_IRUGO);
MODULE_PARM_DESC(ledState, "ledState = 0 (default) mean if you press botton, led will turn on, if ledState = 1 mean the led will turn off if you press botton");

static unsigned int irqNumber;
static int pressNumber = 0;
// we will use a timer to measure the time between interruptions for debouncing, because this kernel doesn't implement the gpio_set_debounce() function (function return -524)
static unsigned int startTime = 0;
static unsigned int oldInterruptionTime = 0;

static int timeNowMillis(void)
{
	struct timeval timeValue;
	uint64_t nowMillis;
	do_gettimeofday(&timeValue);
	nowMillis = (uint64_t)timeValue.tv_sec * (uint64_t)1000 + (uint64_t)(timeValue.tv_usec / 1000);
	return (uint32_t)nowMillis-startTime;
}

// each attribute in sysfs calls _show function when user from user space wants to read from attribute or _store function if user wants to write in the attribute

/* LED STATE */
// show function to show if led on or off
static ssize_t ledState_show(struct kobject *kobj, struct kobj_attribute *attr,char *buff)
{
	return sprintf(buff, "%d", ledState);
}
// store function to ture led on or off
static ssize_t ledState_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buff, size_t len)
{
	sscanf(buff, "%du", &ledState);
	gpio_set_value(ledPin, ledState);
	return len;
}

/* PRESS NUMBER */
// only show function, because we don't need to change it, so it's read only
static ssize_t pressNumber_show(struct kobject *kobj, struct kobj_attribute *attr, char *buff)
{
	return sprintf(buff, "%d", pressNumber);
}

/* define the kobject attributes */
// to declare or define an kobject attribute we need to use one of the 3 helper macros provided by the sysfs.h header.
// __ATTR(variableName, permission, showFunction, storeFunction) // this to define an attribute with read/write function.
// __ATTR_RO(variableName) to define an attribute with only read function ability and the function name must be variableName_show.
// __ATTR_WO(variableName) to define an attribute with only write function ability and the function name must be variableName_store.
// __ATTR_RW(variableName) to define an attribute with both read and write abilities and those functions name must be variableName_show and variableName_store.
static struct kobj_attribute ledState_Attr = __ATTR(ledState, 0660, ledState_show, ledState_store);
static struct kobj_attribute pressNumber_Attr = __ATTR_RO(pressNumber);

// define the attributes array that store all attributes we will use for the attrbut group that we will create later
static struct attribute *attr_array[] = {
	&ledState_Attr.attr,
	&pressNumber_Attr.attr,
	NULL, // to terminate the list of attributes
};

// define the attribute group, this variable contains the array of attributes and its name
static struct attribute_group attr_group = {
	.name = "led_controller",
	.attrs = attr_array,
};

// define an kobject varaible
static struct kobject *myKobject = NULL;

static irq_handler_t button_irq_handler(unsigned int irq_number, void *dev_id, struct pt_regs *regs)
{
	unsigned int now = timeNowMillis();
	// after the first button press, we may have a multi bounces effect, to avoud those bounces we make sure that after the first press, the irq handler will not be available for 200 ms.
	// to make sure it's a rising edge, the gpio pin should be high.
	if((now - oldInterruptionTime > 200) && (gpio_get_value(buttonPin) == 1))
	{
		//  after we release the button aftet 200 ms, we may face some bounces. those bounces can't be avoided so to check if this is a valid (real) intturption we need to wait for sometime to make sure that this interruption was called because we pressed a button and not just some bounces.
		// so the minimum time for a button press should be 5 ms at least. if after 5 ms we still have a high input level, that mean we are  still pressing the button and it('s not just a bounce.
		while(timeNowMillis() - now < 10);
		if(gpio_get_value(buttonPin) == 1)
		{
			printk(KERN_INFO "time now %d", now);
			pressNumber++;
			ledState = !ledState;
			gpio_set_value(ledPin, ledState);
			printk(ledState ? KERN_INFO "led is on": KERN_INFO "led is off"); 
			oldInterruptionTime = now;
		}
	}
	return (irq_handler_t) IRQ_HANDLED; // mean the irq handled correctly
}

static int __init blinkLedInit(void)
{
	int result = 0;
	startTime = timeNowMillis();
	
	printk(KERN_INFO "Init module ...");
	/* create the kobject variable */
	myKobject = kobject_create_and_add("led_controller_kobject", kernel_kobj->parent); // kernel_kobj is the /sys/kernel/ directory
	if(!myKobject)
	{
		printk(KERN_ALERT "couldn't create a kobject");
		return -ENOMEM;
	}
	
	result = sysfs_create_group(myKobject, &attr_group);
	if(result)
	{
		printk(KERN_ALERT "cound't create sysfs group");
		kobject_put(myKobject);
		return result;
	}
	/* LED */
	// check if the given gpio pin is a valide pin or not, before do anything.
	if(!gpio_is_valid(ledPin))
	{
		printk(KERN_ALERT "wrong gpio pin numer for led");
		return -ENODEV;
	}
	// allocate the GPIO number and set its label so it appear in the sysfs.
	gpio_request(ledPin, "led pin");
	// export the gpio pin (create a GPIO<pin number> to the sysfs (/sys/class/gpio). exporting the gpio pin mean we can controlle it from user space.
	// if you try to export the same pin number using a cmd shell, you won't be able to do that because the device or resource will be busy.
	// so the right way is to use this function (gpio_export(<gpio pin>, <direction changable>)) it will create the gpio directory for us and from in a user (from user space) can controlle that gpio pin. and if the <direction changable> set to true it mean the user can change pin direction, false mean, he can't.
	// the cmd to change pin from terminal is to "cd /sys/class/gpio" then "echo <gpio number> > export" then "echo <out/in> > direction" then if out "echo <1/0> > value" to set value.
	gpio_export(ledPin, false);
	// set the GPIO pin as output and pass the init value (high or low). (of input use gpio_direction_intput(unsigned int <gpioName>)).
	gpio_direction_output(ledPin, ledState);
	
	/* BUTTON */
	if(!gpio_is_valid(buttonPin))
	{
		printk(KERN_ALERT "invalid gpio pin number for button");
		return -ENODEV;
	}
	gpio_request(buttonPin, "button pin");
	gpio_export(buttonPin, false);
	gpio_direction_input(buttonPin);
	int debounceResult = gpio_set_debounce(buttonPin, 1000); // usualy a press button when i touch the switch contact it's natural value will bounce. mean it will turn on and off multi times in short periode of time (in 10 or 100 ms). to avoid this multi reading we use a debounce solution to read only the raising eadge and then the next read will be after sort of time, in our example use 200 ms to avoid that bouncing.
	printk(KERN_INFO "debounce number is %d", debounceResult);
	// get the irq number for a specific gpio pin number. this mapping is predefined in the gpio driver. this function will give us the right number of irq that want to lister to based on the gpio pin number we will use.
	irqNumber = gpio_to_irq(buttonPin);
	// reqest for and irq by passing the irq number and the function that will handler the interruption and set the interruption mode (in rising or falling edge),set the name to write in /proc/interrupts to identify the owner. 
	result = request_irq(irqNumber, // the irq number
				(irq_handler_t) button_irq_handler, // the function handler
				IRQF_TRIGGER_RISING | IRQF_ONESHOT, // the irq trigger mode, (rising edge)
				"button_irq_handler", // the name interruption name int he /proc/interruption
				NULL);
	printk(KERN_INFO "requst irq result = %d", result);
	printk(KERN_INFO "Init module done.");
	return result;
}

static void __exit blinkLedExit(void)
{
	printk(KERN_INFO "Prepare module to exit ...");
	/* kobject */
	kobject_put(myKobject);
	/* LED */
	// unexport the gpio<pin number>. this will delete the gpio<pin number> from sysfs
	gpio_unexport(ledPin);
	// release the allocated (claimed) gpio pin.
	gpio_free(ledPin);
	/* BUTTON */
	free_irq(irqNumber, NULL);
	gpio_unexport(buttonPin);
	gpio_free(buttonPin);
	printk(KERN_INFO "Exit module.");
}

module_init(blinkLedInit);
module_exit(blinkLedExit);
