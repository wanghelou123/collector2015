#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include "Log.h"
#include "ShowDataOnLcd.h"


#define	CMD_CLEAR	1
using namespace std;

ShowDataOnLcd::ShowDataOnLcd()
{
#if 0
	switch_input_channel_num = 0;
	relay_output_channel_num = 0;
	input_register_channel_num = 0;
	holding_register_channel_num = 0;

	for(int i=0; i<6; i++) {
		switch (convert.get_unit_type(i+1)) {
			case 0x01:
				input_register_channel_num += 8;
				break;
			case 0x02:
				holding_register_channel_num += 4;
				break;
			case 0x03:
				switch_input_channel_num += 8;
				break;
			case 0x04:
				relay_output_channel_num += 4;
				break;
		}
	}
#endif

	cout << "Create ShowDataOnLcd object!"<<endl;
}


ShowDataOnLcd::~ShowDataOnLcd()
{
	cout << "destory ShowDataOnLcd object!"<< endl;
}

void ShowDataOnLcd::run()
{
	int value = 0;
	float result=0.0;
	int line_count=0;
	char lcd_buffer[16*48+1];
	char *p_lcd = lcd_buffer;
	int channel_sub_type = 0;

	unsigned char recv_buf[1024];
	unsigned char *p = recv_buf;
	int n = mcu.Read(recv_buf);
	
	int ain_num = *(p+3)/4;
	unsigned char *ain_addr = p + 2 + 2; 

	int aout_num = *(p +2 + 2 + ain_num*4 + 1)/4;
	unsigned char *aout_addr = p +2 + 2 + ain_num*4 + 2;

	int din_num = *(p + 2 + 2 + ain_num*4 + 2 + aout_num*4 + 1);
	unsigned char * din_addr = p + 2 + 2 + ain_num*4 + 2 + aout_num*4 + 2;

	int dout_num = *(p + 2 + 2 + ain_num*4 + 2 + aout_num*4 + 2 + din_num + 1);
	unsigned char * dout_addr = p + 2 + 2 + ain_num*4 + 2 + aout_num*4 + 2 + din_num + 2;


	int fd = open("/dev/LCD1602", O_RDWR | O_NOCTTY | O_NDELAY );
	int cmd=0;
	int ret  = 0;
	unsigned char tmp_buffer[64]={0};
	if(fd == -1) {
		perror("open");
		exit(-1);
	}

	while(1) {
#if 0
		for(int i=0; i<n; i++) {
			printf("%.2x ", recv_buf[i]);
		}
		printf("\n");

		for(int i=0; i<din_num;i++)
			printf("%.2x ", din_addr[i]);
		printf("\n");

		for(int i=0; i<dout_num;i++)
			printf("%.2x ", dout_addr[i]);
		printf("\n");
#endif
		
		if(n<0){
			::sleep(2);
			continue;
		}
		ain_addr = p + 2 + 2;
		aout_addr = p +2 + 2 + ain_num*4 + 2;
		din_addr = p + 2 + 2 + ain_num*4 + 2 + aout_num*4 + 2;
		dout_addr = p + 2 + 2 + ain_num*4 + 2 + aout_num*4 + 2 + din_num + 2;

		memset(lcd_buffer, 0x20, sizeof(lcd_buffer));
		p_lcd = lcd_buffer;
		line_count = 0;
#if 0
	DEBUG("ain_num = "<< ain_num);
	DEBUG("aout_num = "<< aout_num);
	DEBUG("din_num = "<<din_num);
	DEBUG("dout_num = "<< dout_num);
#endif

		for(int i=0; i<ain_num; i++) {
			value = ain_addr[0]<<24 | ain_addr[1]<<16 | ain_addr[2]<<8 | ain_addr[3]<<0;
			ain_addr += 4; 
			channel_sub_type = (convert.get_channel_sub_type(i+1)-1?0:1);
			result = convert.ad_to_asr_channel(i, value, channel_sub_type);
			//printf("value = %08x; result=%6.3f\n", value, result);
			snprintf(p_lcd, sizeof(lcd_buffer), "AIN%02d:%6.3f %s",i+1, result,channel_sub_type?"V":"mA");
			p_lcd += 16; //移动到另一行
			line_count++;
		}

		for(int i=0; i<aout_num; i++) {
			value = aout_addr[0]<<24 | aout_addr[1]<<16 | aout_addr[2]<<8 | aout_addr[3]<<0;
			aout_addr += 4; 
			result = convert.ad_to_asr_channel((input_register_channel_num/8)*8 + i, value, 0);
			snprintf(p_lcd, sizeof(lcd_buffer), "AOUT%d:%6.3fmA",i+1, result);
			p_lcd += 16; //移动到另一行
			line_count++;
		}
		

		for(int i=0; i<din_num; i++) {
			snprintf(p_lcd, sizeof(lcd_buffer), "DIN%02d:%d ",i+1, *(din_addr+i) );
			p_lcd += 8; //移动半行
			line_count++;
		}
		line_count -= din_num/2;
		

		for(int i=0; i<dout_num; i++) {
			snprintf(p_lcd, sizeof(lcd_buffer), "DO%02d:%d   ",i+1, *(dout_addr+i) );
			//printf("DOUT%02d:%d \n",i+1, *(dout_addr+i) );
			p_lcd += 8; //移动半行
			line_count++;
		}
		line_count -= dout_num/2;



		for(int i=0; i<line_count/2; i++) {
		/*	
			for(int j=0; j<32;j++)
				printf("%c", lcd_buffer[i*32+j]);

			printf("\n");
		*/

			write(fd, lcd_buffer+i*32, 32);

			::sleep(2);
		}

		n = mcu.Read(recv_buf);

	}

	close(fd);

}

int main(int argc, char *argv[])
{

	// 打开日志  
	if (!Log::instance().open_log(argv[0]))  
	{  
		std::cout << "Log::open_log() failed" << std::endl;  
		exit(-1);
	} 

	ShowDataOnLcd lcd;
	lcd.run();

	return 0;
}
