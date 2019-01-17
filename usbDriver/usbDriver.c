#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Habib JOMAA");
MODULE_DESCRIPTION("joystick driver");
MODULE_VERSION("1.0");

// this function will be call when a device matches one of the id in the usb_device_id table are plugged in
static int usbProbe(struct usb_interface *intf, const struct usb_device_id *id){
	printk(KERN_INFO "usb device plugged in with vendor:product id = %04X:%04X \n",id->idVendor, id->idProduct);
	return 0;
}

// this function will be called when that device removed
static void usbDisconnect(struct usb_interface *intf){
	printk(KERN_INFO "usb device removed \n");
	
}

// list of devices id, vendor id and product id
static struct usb_device_id usbTable[] = {
	{USB_DEVICE(0x0079,0x0006)}, // vendor/product id of my usb (using lsusb)
	{}
};

// this macro enable the linux-hotplug system to load the driver automatically when a device plugged in.
MODULE_DEVICE_TABLE(usb, usbTable);

static struct usb_driver usbDriver = {
	.name = "usb_driver",
	.id_table = usbTable,
	.probe = usbProbe,
	.disconnect = usbDisconnect,
};

static int __init usbDriverInit(void){
	printk(KERN_INFO "Start usb driver \n");
	if(usb_register(&usbDriver)<0){
		printk(KERN_ALERT "can't register usb driver");
		return -1;
	}
	return 0;
}

static void __exit usbDriverExit(void){
	printk(KERN_INFO "Exit usb driver \n");
	usb_deregister(&usbDriver);
}
module_init(usbDriverInit);
module_exit(usbDriverExit);
