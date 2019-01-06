#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h> // to use the class_create and device_create
#include <linux/fs.h> // to import the file operationes stuctures
#include <linux/uaccess.h> // to use the copy_to_use function

#define DRIVER_NAME "readWriteDriverName"
#define DEVICE_CLASS "readWriteDeviceClass"
#define DEVICE_NAME "readWriteDeviceName"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Habib Jomaa");
MODULE_DESCRIPTION("create a simple driver to read and write in device");
MODULE_VERSION("1.0");


int majorNumber;
struct class *deviceClass_p;
struct device *device_p;

char message[256]={0};
int messageLength = 0;

// impliment the file operations function we will use.
ssize_t driverRead (struct file *pFile, char __user *buffer, size_t length, loff_t *offset)
{
	printk(KERN_INFO "we are in %s function of the readWriteDriver module", __FUNCTION__);
	if(*offset >= messageLength)
	{
		printk(KERN_INFO "file empty, nothing to send to use");
		return 0;
	}
	
	if(*offset + length > messageLength)
	{
		length = messageLength - *offset;
	}

	if(copy_to_user(buffer, message + *offset, messageLength) != 0)
	{
		printk(KERN_WARNING "sending message to use failed!!");
		return -1;	
	}
	printk(KERN_INFO "message %s send to user successfully", message);
	*offset += length;
	return length;
}
ssize_t driverWrite (struct file *pFile, const char __user *buffer, size_t length, loff_t *offset)
{
	printk(KERN_INFO "we are in %s function of the readWriteDriver module", __FUNCTION__);
	sprintf(message, "%s", buffer);
	messageLength = strlen(message);
	return length;
}
int driverOpen (struct inode *pInode, struct file *pFile)
{
	printk(KERN_INFO "we are in %s function of the readWriteDriver module", __FUNCTION__);
	return 0;
}
int driverClose (struct inode *pInode, struct file *pFile)
{
	printk(KERN_INFO "we are in %s function of the readWriteDriver module", __FUNCTION__);
	return 0;
}

struct file_operations driverOperations =
{
	.owner = THIS_MODULE,
	.open = driverOpen,
	.read = driverRead,
	.write = driverWrite,
	.release = driverClose
};

__init int driverInit(void)
{
	printk(KERN_INFO "registering a character driver to the kernel");
	// first of all we need to register this driver with kernel and specify its caracteristics ( charater or block driver and its major number and the driver name). it will just allocat a major number of the driver, check "/proc/devices" file to see if your driver name exist or not after the register function.
	// manual way to find a proper major number is to pick a number doesn't exist in the list of drivers are still running in system. (cat /proc/devices will display list of drivers. it return 0 in success or -ve if failed
	//majorNumber = 240;
	//int ret = register_chrdev(majorNumber, DRIVER_NAME, &driverOperations);
	// the right way to register a major number is to use a dynamic allocation. it's just the same function "register_devchar()" but we pass 0 instead of number in range [1 255] to tell the function to try to find unused major number for our driver and then return it. in case of error it will return -ve.
	majorNumber = register_chrdev(0, DRIVER_NAME, &driverOperations);
	
	// if we used the manual way we test if res is -ve or 1, if we use the dynamic way, we check if the return number is between [0 255] or not (-ve)
	if(majorNumber<0)
	{
		printk(KERN_ALERT "failed to register a major number" );
		return 0;
	}
	printk(KERN_INFO "Successfully registered a major number = %d", majorNumber);
	
	// new we create a device that use this driver.
	// to do this we need to create the device class first to pass it later to the creation function of the device. this class will be created in /sys/class folder under the className you give to it.
	// class_create is a function that create that class name directory in "/sys/class", so the udev can later automatically create the device file inside by using the device_create function.
	printk(KERN_INFO "Creating device class");
	deviceClass_p = class_create(THIS_MODULE, DEVICE_CLASS);
	if(IS_ERR(deviceClass_p))
	{
		printk(KERN_ALERT "Couldn't create device class");
		printk(KERN_ALERT "unregistering major number");
		unregister_chrdev(majorNumber, DRIVER_NAME);
		return 0;
	}

	printk(KERN_INFO "create device");
	device_p = device_create(deviceClass_p, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if(IS_ERR(device_p))
	{
		printk(KERN_INFO "unregistering the driver");
		unregister_chrdev(majorNumber, DRIVER_NAME);
		printk(KERN_INFO "destroying device class");
		class_destroy(deviceClass_p);
		return 0;
	}
	
	printk(KERN_INFO "device driver successfully created");
	return 0;
}

void driverExit(void)
{
	// before we exit our module we need to unregister our driver.
	printk(KERN_INFO "destroy the device");
	device_destroy(deviceClass_p, MKDEV(majorNumber, 0));
	printk(KERN_INFO "unregistering the device class");
	class_unregister(deviceClass_p);
	printk(KERN_INFO "destroying device class");
	class_destroy(deviceClass_p);
	printk(KERN_INFO "unregistering the driver");
	unregister_chrdev(majorNumber, DRIVER_NAME);
	printk(KERN_INFO "exit module");
}

module_init(driverInit);
module_exit(driverExit);
