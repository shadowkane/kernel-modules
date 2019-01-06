#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("GPL"); // the kernel module license
MODULE_AUTHOR("Habib JOMAA"); // the author of this module
MODULE_DESCRIPTION("this is just to test the basices of kernel programming!!"); // description of this module
MODULE_VERSION("1.0"); // the version of this module

// declaring arguments to be pass later
static char *argString = "this is a string argument";
// module_param tell the kernel that this variable (argString) is an argument, and charp its type( char pointer), with a permission S_IRUGO)
// permissions is in 4 digits(xxxx) the first is 0 and the other 3 are similar to the file or directory permission.
// or we can use the pre-defined varaible like S_IRUGO, S_IRUSR, S_RWXU ...
// 0100 or S_IXUSR (user have only execute permission, groupe and other has nothing)
// 0400 or S_IRUSR (user have the permission to read only)
// 0070 or S_RWXG (group have the permission to read, write and execute)
// 0506 or S_IRXU | S_IRXO (user have the permission to read and execute, other have the permission to read and write)
// for user  ,S_IRUSR: read, S_IWUSR: write, S_IXUSR: execute, S_IRWXU: read and write and execute
// for group ,S_IRGRP: read, S_IWGRP: write, S_IXGRP: execute, S_IRWXG: read and write and execute
// for other ,S_IROTH: read, S_IWOTH: write, S_IXOTH: execute, S_IRWXO: read and write and execute
// to make all(user, group and other) read : S_IRUGO
// to make all(user, group and other) write : S_IWUGO
// to make all(user, group and other) execute : S_IXUGO
module_param(argString, charp, S_IRUGO);
MODULE_PARM_DESC(argString, "this is a string you need to display"); // the argument discreption

static int argInt = 0;
module_param(argInt, int, S_IRUGO);
MODULE_PARM_DESC(argInt, "this is an integer you need to add");

// add the symbol "__initdata" to a variable will tell the kernel to delete this varaible from memory just after executing the init fonction
__initdata int tmpVariableInit = 5;

// add the symbol "__init" to a function will tell the kernel to delete this function from memery just after executing the init function
// the function myInitFunction(void) it's our init function so it's safe to delete it from our memory
// in a general case, before use the __init symbol make sure the function is used only in init function.
__init int myInitFunction(void){
	printk(KERN_ALERT "This is %s function", __FUNCTION__);
	printk(KERN_ALERT "Temporary variable tmpVariableInit = %d will be deleted from memory as soon as the initialization of this module ends!", tmpVariableInit);
	printk(KERN_INFO "The string argument is %s", argString);
	return 0;
}

void myExitFunction(void){
	printk(KERN_ALERT "This is %s function", __FUNCTION__);
	printk(KERN_INFO "The number argument is %d", argInt);
	// if i add this line the kernel will try to print the variable tmpVariableInit but it's not existe anymore because it's an initdata variable
	// which mean it deleted after init function ended
	// don't uncomment it or your module will not be removable because the exit function will not work and then the module will be busy until the next reboot.
	// to remove a busy module like this(doesn't start at boot and has no exit function or has a problem with its exit function) you need to reboot the system.
	//printk(KERN_ALERT "Show me if tmpVariableInit still exist, it = %d", tmpVariableInit);
}

// set the init function here using module_init(<functionName>)
module_init(myInitFunction);
// set the exit function here using module_exit(<functionName>)
module_exit(myExitFunction);
