#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <mach/hardware.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include "lcd1602.h"

#define DEVICE_NAME "LCD1602"
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

#define	LCDRS			GPIO_TO_PIN(2, 24)
#define	LCDRW			GPIO_TO_PIN(2, 17)
#define	LCDE			GPIO_TO_PIN(0, 8)
#define DB0				GPIO_TO_PIN(0, 9)
#define DB1				GPIO_TO_PIN(0, 10)
#define DB2				GPIO_TO_PIN(0, 11)
#define DB3				GPIO_TO_PIN(2, 11)
#define DB4				GPIO_TO_PIN(2, 12)
#define DB5				GPIO_TO_PIN(2, 13)
#define DB6				GPIO_TO_PIN(2, 14)
#define DB7				GPIO_TO_PIN(2, 15)

void delay_us(unsigned long us) // 延时微秒
{  
	udelay(us);
} 

void delay_ms(unsigned long ms) // 延时毫秒
{     
	mdelay(ms);
}


// 循环检测LCD忙标志(BF)，直到其值为0，方可执行下一指令   
void lcd_wait_until_finish(void)  
{  
	lcd1602_rs_out(CMD);		// RS置0，读状态时RS需置低电平      
	lcd1602_rw_out(READ);		// RW置1，状态为读           
	lcd1602_en_out(HIGH);		// E置1，读取信息  
	delay_us(20);  

	int count = 0;
	while(lcd1602_bf_in()){
		if(count++ > 10)
			break;;		
	}

	// 循环直至BF=0  
	lcd1602_en_out(LOW);		// E重置为0  
	delay_us(20);  
} 


// 向LCD写命令字  
void lcd_command(unsigned char cmd)  
{  
	//printk("command: 0x%x\n", cmd);
	lcd_wait_until_finish(); // 等待执行完毕  
	lcd1602_rs_out(CMD);     // RS置0，写入命令字  
	lcd1602_rw_out(WRITE);     // RW置0，状态为写  

	delay_us(20);  
	lcd1602_en_out(HIGH);
	lcd1602_bus_write(cmd);  // 将命令字cmd送入LCD的数据端口
	lcd1602_en_out(LOW);
	delay_us(20);  
} 


// 设置显示位置(即写入显示地址)，x,y均从0开始  
void lcd_goto_xy(unsigned char x, unsigned char y)  
{  
	unsigned char p;       // p为字符显示位置,即DDRAM中的地址  
	if (y==0)  
	{  
		p = 0x00 + x;      // (0,0)显示位置为0x00  
	}  
	else  
	{  
		p = 0x40 + x;      // (0,1)显示位置为0x40  
	}  
	lcd_command(p + 0x80); // 写入显示地址时DB7须为高电平,加0x80  
} 

// 写字符(传入的参数实际为所需显示字符的地址,即液晶字符产生器中字符的地址)    
void lcd_putc(unsigned char c)  
{  
	lcd_wait_until_finish(); // 等待执行完毕  
	lcd1602_rs_out(DATA);    // RS置1，写入数据  
	lcd1602_rw_out(WRITE);     // RW置0，状态为写  
	delay_us(20);  
	lcd1602_en_out(HIGH);
	lcd1602_bus_write(c);	 // 将命令字cmd送入LCD的数据端口    
	lcd1602_en_out(LOW);
	delay_us(20);  
} 
unsigned char lcd_get_char()
{
	lcd_wait_until_finish(); // 等待执行完毕  
	lcd1602_rs_out(DATA);		// RS置0，读状态时RS需置低电平      
	lcd1602_rw_out(READ);		// RW置1，状态为读           
	lcd1602_en_out(HIGH);		// E置1，读取信息  
	delay_us(20);  
	unsigned char data=lcd1602_bus_read();
	// 循环直至BF=0  
	lcd1602_en_out(LOW);		// E重置为0  
	delay_us(20);  
	//printk("read data => 0x%2x\n", data);

	return data;
}

// 指定位置写字符  
void lcd_xy_putc(unsigned char x, unsigned char y, unsigned char c)  
{  
	lcd_goto_xy(x,y);
	delay_us(100); //要延迟下
	lcd_putc(c);  
}  

// 写字符串   
void lcd_puts(unsigned char *s)  
{  
	while(*s)  
	{  
		lcd_putc(*s);
		//delay_us(100); //要延迟下
		s++;  
	}  
}  

// 指定位置写字符串     
void lcd_xy_puts(unsigned char x, unsigned char y, unsigned char *s)  
{  
	lcd_goto_xy(x, y);
	delay_us(100); //要延迟下
	lcd_puts(s);  
} 

void lcd1602_clear(void)
{
	lcd_command(0x01);
	delay_ms(10);
}

// LCD初始化   
void lcd1602_init(void)  
{ 
#if 0
	delay_ms(15);
	lcd_command(0X38);//8位总线，双行显示, 5*7点阵列字符
	delay_ms(5);
	lcd_command(0X38);
	delay_ms(5);
	lcd_command(0X38);
	delay_ms(5);
	lcd_command(0X38);  
	delay_ms(5);
	lcd_command(0X08);//显示关闭
	delay_ms(5);
	lcd_command(0X01);//清屏
	delay_ms(10);
	lcd_command(0X06);//设置光标移动的方向为右
	delay_ms(5); 
	lcd_command(0X0C);//显示打开
	delay_ms(5); 
#endif
	lcd_command(0X38);//8位总线，双行显示, 5*7点阵列字符
	printk("8 bit bus, double line show, 5*7 character\n");
	lcd_command(0X0C);//显示打开
	printk("show ON\n");
	lcd_command(0X06);//设置光标移动的方向为右
	printk("set cursor right!\n");
	lcd_command(0X01);//清屏
	printk("clear the screen!\n");

}

/*
 * hl 为0：输出低电平,非0：输出高电平
 */
void lcd1602_en_out(int hl) //使能
{
	gpio_direction_output(LCDE, (hl == 0) ? 0 : 1);
}


//读写1602控制,H:读1602,L:写入1602
void lcd1602_rw_out(int hl) 
{
	gpio_direction_output(LCDRW, (hl == 0) ? 0 : 1);
}


//输入指令数据选择
void lcd1602_rs_out(int hl) 
{
	gpio_direction_output(LCDRS, (hl == 0) ? 0 : 1);
}


//向1602的8根数据总线写入数据
void lcd1602_bus_write(unsigned char c)
{
	//printk("write data: %c ==> 0x%x\n", c, c);
	gpio_direction_output(DB0, (c>>0)&1);
	gpio_direction_output(DB1, (c>>1)&1);
	gpio_direction_output(DB2, (c>>2)&1);
	gpio_direction_output(DB3, (c>>3)&1);
	gpio_direction_output(DB4, (c>>4)&1);
	gpio_direction_output(DB5, (c>>5)&1);
	gpio_direction_output(DB6, (c>>6)&1);
	gpio_direction_output(DB7, (c>>7)&1);
}

//从1602的8根数据总线读入数据
unsigned char lcd1602_bus_read(void)
{
	unsigned char data = 0;

	//读取8位数据
	data |= gpio_get_value(DB0)<<0;
	data |= gpio_get_value(DB1)<<1;
	data |= gpio_get_value(DB2)<<2;
	data |= gpio_get_value(DB3)<<3;
	data |= gpio_get_value(DB4)<<4;
	data |= gpio_get_value(DB5)<<5;
	data |= gpio_get_value(DB6)<<6;
	data |= gpio_get_value(DB7)<<7;


	return data;
}
//BF=1表示液晶显示器忙,返回值就是bf的状态
int lcd1602_bf_in(void)
{
	return (lcd1602_bus_read() & 0x80);
}

int lcd1602_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int lcd1602_release(struct inode *inode, struct file *filp)
{
	return 0; 
}

static ssize_t lcd1602_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	unsigned char buffer[33]={0};
	int i=0;
	for(i=0; i<16;i++) {
		lcd_goto_xy(i,0);
		buffer[i] = lcd_get_char();
		printk("addr:%x %c ==> %x\n",0x80|i,  buffer[i], buffer[i]);
	}

	for(i=0; i<16;i++) {
		lcd_goto_xy(i,0x40);
		buffer[16+i] = lcd_get_char();
		printk("addr:%x %c ==> %x\n",0x80|(0x40+i),  buffer[16+i], buffer[16+i]);
	}


	copy_to_user(buf, buffer,32);
#if 0
	for(i=0; i<0x27;i++){
		lcd_goto_xy(i,0);
		printk("0x%2x %c ==> %x\n", i, lcd_get_char(), lcd_get_char() );
	}
	for(i=0; i<0x27;i++){
		lcd_goto_xy(i,0x40);
		printk("0x%2x %c ==> %x\n", i+0x40, lcd_get_char(),  lcd_get_char());
	}
#endif

	return 32;
}

static ssize_t lcd1602_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned char line1[17]={0};
	unsigned char line2[17]={0};

	if(size > 32)size = 32;

	lcd1602_clear();

	if(size<16) {
		memcpy(line1, buf, size);
	} else {
		memcpy(line1, buf, 16);
		memcpy(line2, buf+16, size-16);
	}

	lcd_puts(line1);
	lcd_goto_xy(0, 0x40);
	lcd_puts(line2);

	return size;
}

#define PUT_CHAR 0
#define CLEAR 1
static long my_LCD1602_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	cmd =  _IOC_NR(cmd); 

	switch (cmd){
		case PUT_CHAR:
			printk("put char.\n");
			lcd_putc((unsigned char)arg);
			break;
		case CLEAR:
			printk("clear the screen.\n");
			lcd1602_clear();
			break;
		default:
			return -EINVAL;
	}
}

static struct file_operations my_LCD1602_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = my_LCD1602_ioctl,
	.read = lcd1602_read,
	.write = lcd1602_write,
	.open = lcd1602_open,
	.release = lcd1602_release,
};

static struct miscdevice misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,		 /*misc设备的设备号 值为255*/
	.fops = &my_LCD1602_fops ,	 /*这个设备的名称 值为my_LCD1602_fops */
	/* my_LCD1602_fops是上节所述的文件系统接口*/
};/*定义一个简单的小型设备。*/

static int my_LCD1602_init(void)
{
	int ret = 0;
	ret |= gpio_request(LCDRS, NULL);
	ret |= gpio_request(LCDRW, NULL);
	ret |= gpio_request(LCDE, NULL);
	ret |= gpio_request(DB0, NULL);
	ret |= gpio_request(DB1, NULL);
	ret |= gpio_request(DB2, NULL);
	ret |= gpio_request(DB3, NULL);
	ret |= gpio_request(DB4, NULL);
	ret |= gpio_request(DB5, NULL);
	ret |= gpio_request(DB6, NULL);
	ret |= gpio_request(DB7, NULL);

	if(ret !=0 ) {
		printk("gpio_request error. ret = %d\n", ret);	
	}

	ret = misc_register(&misc);
	if (ret < 0) {
		printk(DEVICE_NAME ":can't register device.");
		return ret;
	}
	//配置数据/命令选择、读/写选择、数据管脚为输出状态
	lcd1602_init();
	lcd_goto_xy(0,0);			 // 字符位置：(4,0) , 5*7点阵列字符
	lcd_puts(" Welcome to use");    // 显示字符
	lcd_goto_xy(0,40);
	lcd_puts("collector");

	printk(DEVICE_NAME " initialized\n");
	return 0;
}
static void __exit my_LCD1602_exit(void)
{
	gpio_free(LCDRS);
	gpio_free(LCDRW);
	gpio_free(LCDE);
	gpio_free(DB0);
	gpio_free(DB1);
	gpio_free(DB2);
	gpio_free(DB3);
	gpio_free(DB4);
	gpio_free(DB5);
	gpio_free(DB6);
	gpio_free(DB7);
	
	misc_deregister(&misc);
	printk(DEVICE_NAME " uninstall\n");
}

module_init(my_LCD1602_init);
module_exit(my_LCD1602_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wang He Lou");
MODULE_DESCRIPTION("s3c2416 LCD1602 driver");
