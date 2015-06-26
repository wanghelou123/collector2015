#ifndef LCD_H  
#define LCD_H  

#define HIGH 1
#define LOW 0
#define	DATA	1
#define	CMD		0
#define	READ	1
#define	WRITE	0

// 函数原型  

// 向LCD写命令字  
extern void lcd_command(unsigned char cmd);  

// 设置显示位置(即写入显示地址)，行列均从0开始  
extern void lcd_goto_xy(unsigned char x, unsigned char y);  

// 写字符(传入的参数实际为所需显示字符的地址,即液晶字符产生器中字符的地址)  
extern void lcd_putc(unsigned char c); 
// 指定位置写字符  
extern void lcd_xy_putc(unsigned char x, unsigned char y, unsigned char c);  

// 写字符串  
extern void lcd_puts(unsigned char *s);  

// 指定位置写字符串  
extern void lcd_xy_puts(unsigned char x, unsigned char y, unsigned char *s);  

// LCD初始化  
extern void lcd1602_init(void);
//对1602三根控制线的控制
extern void lcd1602_en_out(int hl); //使能
extern void lcd1602_rw_out(int hl); //读写1602控制
extern void lcd1602_rs_out(int hl); //输入指令数据选择
extern int lcd1602_bf_in(void); //读入操作状态，bf为busy free
extern void lvx3245_tr_out(int hl);
extern void lcd1602_bus_write(unsigned char c);
extern unsigned char lcd1602_bus_read(void);

#endif //LCD_H
