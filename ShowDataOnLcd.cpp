#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include "Log.h"
#include "ShowDataOnLcd.h"
#include "sqlite3.h"


#define	CMD_CLEAR	1
using namespace std;

int ShowDataOnLcd::ShowUnit()
{
	DEBUG(__func__);
	int channel_id;
	char channel_def[10];
	char channel_tmp[50];
	char *p=NULL;
	int i=0;
	sqlite3 *db;
	sqlite3_stmt   *stmt;
	int rc;

	char sql[128]; 

	rc = sqlite3_open("/etc/gateway/collector.db", &db);
	if (rc) {
		FATAL("Can't open database: " << sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}  
	sprintf(sql, "select s1.sn,s1.channel_type,s2.channel_name from demarcate as s1,channel_def as s2 where s1.channel_type=s2.channel_type ;");
	rc = sqlite3_prepare(db, sql, (int)strlen(sql), &stmt, NULL); 
	if(rc != SQLITE_OK) { 
		FATAL("SQL error:" << sql << "->" << sqlite3_errmsg(db));
	}   

	rc = sqlite3_step(stmt);
	while(rc == SQLITE_ROW){
		sscanf((char*)sqlite3_column_text(stmt, 1), "%x", &channel_id);
		strcpy(channel_tmp, (char*)sqlite3_column_text(stmt, 2));

		memset(channel_def, 0, sizeof(channel_def));
		p = strchr(channel_tmp, '(');
		i=0;
		while(p && *p++ != ')'){
			channel_def[i++]=*p;	
		}
		channel_def[--i] = '\0';
		if(!strncmp(channel_def, "预留", strlen("预留")) 
				||!strncmp(channel_def, "输出", strlen("输出")) 
				||!strncmp(channel_def, "输入", strlen("输入")) )
			memset(channel_def, 0, sizeof(channel_def));

		strcpy(channel_unit[sqlite3_column_int(stmt,0)-1], channel_def);
		//printf("%d  %d	%s\n", sqlite3_column_int(stmt, 0), channel_id, channel_unit[sqlite3_column_int(stmt,0)-1]);
		rc = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

ShowDataOnLcd::ShowDataOnLcd()
{

	ShowUnit();
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
	int n = 0;
	while((n = mcu.Read(recv_buf)) < 0){
		FATAL("mcu.Read error.");
		::sleep(2);
	}

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

		if(n<0){
			::sleep(2);
			n = mcu.Read(recv_buf);
			continue;
		}
		ain_addr = p + 2 + 2;
		aout_addr = p +2 + 2 + ain_num*4 + 2;
		din_addr = p + 2 + 2 + ain_num*4 + 2 + aout_num*4 + 2;
		dout_addr = p + 2 + 2 + ain_num*4 + 2 + aout_num*4 + 2 + din_num + 2;

		memset(lcd_buffer, 0x0, sizeof(lcd_buffer));
		p_lcd = lcd_buffer;
		line_count = 0;

		for(int i=0; i<ain_num; i++) {
			value = ain_addr[0]<<24 | ain_addr[1]<<16 | ain_addr[2]<<8 | ain_addr[3]<<0;
			ain_addr += 4; 
			channel_sub_type = (convert.get_channel_sub_type(i+1)-1?0:1);
			//将AD值转换成模拟值
			result = convert.ad_to_asr_channel(i, value, channel_sub_type);
			//将模拟值转换成物理值
			result = (float)convert.asr_to_phy_channel(i, result)/1000;	

			//printf("value = %08x; result=%6.3f\n", value, result);
			snprintf(p_lcd, sizeof(lcd_buffer), "AIN%02d:%6.3f %s",i+1, result,channel_unit[i]);
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
			//debug message
			memset(tmp_buffer, 0, sizeof(tmp_buffer));
			memcpy(tmp_buffer, lcd_buffer+i*32, 32);
			DEBUG(tmp_buffer);
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
