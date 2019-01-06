#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h> // to use the gpio driver
#include <linux/delay.h> // to use the msleep() function

// in raspberry pi use the cmd "gpio readall" to display the gpio table.
// or the "pinout" cmd to display all information about the raspberry board, SoC info, ports info and GPIO table.
// use the BCM number.
#define LEDPIN 18

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Habib JOMAA");
MODULE_DESCRIPTION("make led blink for a given timer in ms");
MODULE_VERSION("1.0");

static int frequency = 2000; // in ms

static int __init blinkLedInit(void)
{
	printk(KERN_INFO "Init module ...");
	// check if the given gpio pin is a valide pin or not, before do anything.
	if(!gpio_is_valid(LEDPIN))
	{
		printk(KERN_ALERT "wrong gpio pin numer");
		return -ENODEV;
	}
	// allocate the GPIO number and set its label so it appear in the sysfs.
	gpio_request(LEDPIN, "led pin");
	// export the gpio pin (create a GPIO<pin number> to the sysfs (/sys/class/gpio). exporting the gpio pin mean we can controlle it from user space.
	// if you try to export the same pin number using a cmd shell, you won't be able to do that because the device or resource will be busy.
	// so the right way is to use this function (gpio_export(<gpio pin>, <direction changable>)) it will create the gpio directory for us and from in a user (from user space) can controlle that gpio pin. and if the <direction changable> set to true it mean the user can change pin direction, false mean, he can't.
	// the cmd to change pin from terminal is to "cd /sys/class/gpio" then "echo <gpio number> > export" then "echo <out/in> > direction" then if out "echo <1/0> > value" to set value.
	gpio_export(LEDPIN, false);
	// set the GPIO pin as output and pass the init value (high or low). (of input use gpio_direction_intput(unsigned int <gpioName>)).
	gpio_direction_output(LEDPIN, 1);
	msleep(frequency/2);
	gpio_set_value(LEDPIN, 0);
	msleep(frequency/2);
	gpio_set_value(LEDPIN, 1);
	printk(KERN_INFO "Init module done.");
	return 0;
}

static void __exit blinkLedExit(void)
{
	printk(KERN_INFO "Prepare module to exit ...");
	// unexport the gpio<pin number>. this will delete the gpio<pin number> from sysfs
	gpio_unexport(LEDPIN);
	// release the allocated (claimed) gpio pin.
	gpio_free(LEDPIN);
	printk(KERN_INFO "Exit module.");
}

module_init(blinkLedInit);
module_exit(blinkLedExit);
