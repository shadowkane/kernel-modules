#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Habib JOMAA");
MODULE_DESCRIPTION("joystick driver");
MODULE_VERSION("1.0");

// this function will be call when a device matches one of the id in the usb_device_id table are plugged in
static int joystickProbe(struct usb_interface *intf, const struct usb_device_id *id){
	printk(KERN_INFO "Joystick driver plugged in with vendor:product id = %04X:%04X \n",id->idVendor, id->idProduct);
	return 0;
}

// this function will be called when that device removed
static void joystickDisconnect(struct usb_interface *intf){
	printk(KERN_INFO "Joystick removed \n");
	
}

// list of devices id, vendor id and product id
static struct usb_device_id joystick_table[] = {
	{USB_DEVICE(0x0079,0x0006)}, // from my joystick gamepad (using lsusb)
	{}
};

// this macro enable the linux-hotplug system to load the driver automatically when a device plugged in.
MODULE_DEVICE_TABLE(usb, joystick_table);

static struct usb_driver joystick_driver = {
	.name = "joystick_driver",
	.id_table = joystick_table,
	.probe = joystickProbe,
	.disconnect = joystickDisconnect,
};

static int __init joystickInit(void){
	printk(KERN_INFO "Start joystick driver \n");
	if(usb_register(&joystick_driver)<0){
		printk(KERN_ALERT "can't register joystick driver");
		return -1;
	}
	return 0;
}

static void __exit joystickExit(void){
	printk(KERN_INFO "Exit joystick driver \n");
	usb_deregister(&joystick_driver);
}
module_init(joystickInit);
module_exit(joystickExit);
