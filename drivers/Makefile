CURRENT_PATH := $(shell pwd)
ARM_LINUX_KERNEL :=/work/collector2014/linux-3.8.5

obj-m +=lcd1602.o
modules:
	$(MAKE) -C $(ARM_LINUX_KERNEL) M=$(CURRENT_PATH) modules

clean:
	rm -rf .*.cmd *.o *.mod.c *.ko .tmp_versions Module.symvers .Makefile.swp modules.order

.PHONY: modules modules_install clean
