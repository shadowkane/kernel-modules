KERNEL_BUILD=/lib/modules/$(shell uname -r)/build

obj-m:=usbDriver.o

all:
	make -C $(KERNEL_BUILD) M=$(PWD) modules
clean:
	make -C $(KERNEL_BUILD) M=$(PWD) clean
