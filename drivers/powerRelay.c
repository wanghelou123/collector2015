#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <mach/regs-gpio.h>
#include <asm/gpio.h>//add by whl 2013-5-20
//#include <mach/hardware.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/delay.h> 
#include <linux/moduleparam.h> 
#include <linux/slab.h> 
#include <linux/errno.h> 
#include <linux/ioctl.h> 
#include <linux/cdev.h> 
#include <linux/string.h> 
#include <linux/list.h> 
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/unistd.h>
//#include <asm-arm/arch-s3c2410/regs-gpio.h>
#define DEVICE_NAME "powerRelay"//继电器

#define S3C2410_GPD11			S3C2410_GPD(11)
#define S3C2410_GPD12			S3C2410_GPD(12)
#define S3C2410_GPL13			S3C2410_GPL(13)
#define S3C2410_GPD11_OUTP		S3C2410_GPIO_OUTPUT
#define S3C2410_GPD12_OUTP		S3C2410_GPIO_OUTPUT
#define S3C2410_GPL13_OUTP		S3C2410_GPIO_OUTPUT
#define s3c2410_gpio_cfgpin		s3c_gpio_cfgpin
#define s3c2410_gpio_setpin		gpio_set_value
static unsigned long powerRelay[] = {
	S3C2410_GPD11, //M2M
	S3C2410_GPD12, //zigbee
	S3C2410_GPL13, //USB 3G
};


static int s3c2416_powerRelay_ioctl(
		struct file *file,
		unsigned int cmd,
		unsigned long arg){
	switch(cmd){
		case 0:	
			s3c2410_gpio_setpin(powerRelay[arg], 0);	
			return 0;
			
		case 1:
			s3c2410_gpio_setpin(powerRelay[arg], 1);
			return 0;
		default:
			return -EINVAL;			
	} 
}

static struct file_operations dev_fops = {
		.owner =     THIS_MODULE,
		.unlocked_ioctl =     s3c2416_powerRelay_ioctl,
}; 

static struct miscdevice misc = {
		.minor = MISC_DYNAMIC_MINOR,
		.name  = DEVICE_NAME,
		.fops  = &dev_fops,
};

static int __init dev_init(void){
	int ret;
	int i;
	
	for (i = 0; i < 3; i++){
		s3c2410_gpio_cfgpin(powerRelay[i], S3C2410_GPIO_OUTPUT);
		s3c2410_gpio_setpin(powerRelay[i], 0);
	}
		
	ret = misc_register(&misc);
		
	printk(DEVICE_NAME"\tinitialized\n");
	
	return ret;
}

static void __exit dev_exit(void){
	misc_deregister(&misc);
}

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("WHL");
