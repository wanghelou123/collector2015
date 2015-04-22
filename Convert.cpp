#include <string.h>
#include <math.h>
#include "Convert.h"
#include "sqlite3.h"
#include "Log.h"

Convert::Convert()
{
	DEBUG("construct the Convert object!");
	init_board_flag();
	init_board_info();
}

Convert::~Convert()
{
	DEBUG("destory the Convert object!");
}

int Convert::get_node_data(int sensor_number, unsigned char (&buffer)[BUFFER_SIZE])
{
	DEBUG(__func__);
	unsigned char recv_buf[1024];
	unsigned char node_buf[6][BUFFER_SIZE];
	memset(node_buf,0, 6*BUFFER_SIZE);
	int ret = 0;
	int node_type=0, node_len=0, node_bytes=0;
	int n = mcu.Read(recv_buf);
	if(n<0) {
		return -1;
	}
	unsigned char *p = recv_buf+2;

	for(int i=0; i<6;) {
		node_type = *p;
		node_len  = *(p+1);
		p+=2;
		switch (node_type) {
			case 0x01: DEBUG("node_type: AIN,  total "<< node_len << " bytes, "<< node_len/32*8 << " channels"); break;
			case 0x02: DEBUG("node_type: AOUT, total "<< node_len << " bytes, "<< node_len/16*4<< " channels"); break;
			case 0x03: DEBUG("node_type: DIN,  total "<< node_len << " bytes, "<< node_len/8*8<< " channels"); break;
			case 0x04: DEBUG("node_type: DOUT, total "<< node_len << " bytes, "<< node_len/4*4<< " channels"); break;
		}
		
		if(node_type == 1){			//模拟量采集
			node_bytes = 32; 
		}else if(node_type == 2){	//模拟量输出
			node_bytes = 16;		
		}else if(node_type == 3) {	//开关量采集
			node_bytes = 8;		
		}else if(node_type ==4) {	//开关量输出
			node_bytes = 4;		
		}
		
		DEBUG("node_len/node_bytes = "<< node_len/node_bytes);
		switch(node_len/node_bytes){
			case 1:
				memcpy(node_buf[i+0], p+node_bytes*0, node_bytes);
				break;
			case 2:
				memcpy(node_buf[i+0], p+node_bytes*0, node_bytes);
				memcpy(node_buf[i+1], p+node_bytes*1, node_bytes);
				break;
			case 3:
				memcpy(node_buf[i+0], p+node_bytes*0, node_bytes);
				memcpy(node_buf[i+1], p+node_bytes*1, node_bytes);
				memcpy(node_buf[i+2], p+node_bytes*2, node_bytes);
				break;
			case 4:
				memcpy(node_buf[i+0], p+node_bytes*0, node_bytes);
				memcpy(node_buf[i+1], p+node_bytes*1, node_bytes);
				memcpy(node_buf[i+2], p+node_bytes*2, node_bytes);
				memcpy(node_buf[i+3], p+node_bytes*3, node_bytes);
				break;
			case 5:
				memcpy(node_buf[i+0], p+node_bytes*0, node_bytes);
				memcpy(node_buf[i+1], p+node_bytes*1, node_bytes);
				memcpy(node_buf[i+2], p+node_bytes*2, node_bytes);
				memcpy(node_buf[i+3], p+node_bytes*3, node_bytes);
				memcpy(node_buf[i+4], p+node_bytes*4, node_bytes);
				break;
			case 6:
				memcpy(node_buf[i+0], p+node_bytes*0, node_bytes);
				memcpy(node_buf[i+1], p+node_bytes*1, node_bytes);
				memcpy(node_buf[i+2], p+node_bytes*2, node_bytes);
				memcpy(node_buf[i+3], p+node_bytes*3, node_bytes);
				memcpy(node_buf[i+4], p+node_bytes*4, node_bytes);
				memcpy(node_buf[i+5], p+node_bytes*5, node_bytes);
				break;
		}

		i += node_len/node_bytes;
		p += node_len;
	}

	//将AD原始值转换为模拟值
	ret = ad_to_asr(sensor_number, node_buf[sensor_number - 1]);

	//将模拟值转换为物理值
	int unit_type = 0;
	switch(sensor_number) {
		case 0x01: unit_type = flags.unit1_type; break;
		case 0x02: unit_type = flags.unit2_type; break;
		case 0x03: unit_type = flags.unit3_type; break;
		case 0x04: unit_type = flags.unit4_type; break;
		case 0x05: unit_type = flags.unit5_type; break;
		case 0x06: unit_type = flags.unit6_type; break;
		default: FATAL(__func__<<":error sensor_number");
	}
	if(flags.sensor_conf && (unit_type == 1)){
		ret = asr_to_phy(sensor_number, node_buf[sensor_number - 1]);
	}
	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, node_buf[sensor_number - 1], ret);

	return ret;
}

int Convert::init_board_flag()
{
	DEBUG(__func__);
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
	sprintf(sql, "select name, flag, sub_flag from flags;");
	rc = sqlite3_prepare(db, sql, (int)strlen(sql), &stmt, NULL); 
	if(rc != SQLITE_OK) { 
		FATAL("SQL error:" << sql << "->" << sqlite3_errmsg(db));
	}   
	rc = sqlite3_step(stmt);
	while(rc == SQLITE_ROW){
		if(0 == strcmp("fixed_point_number", (char*)sqlite3_column_text(stmt, 0))){
			flags.fixed_point_number = sqlite3_column_int(stmt, 1);	
			DEBUG("flags.fixed_point_number = "<<flags.fixed_point_number);
		}else if(0 == strcmp("sensor_conf", (char*)sqlite3_column_text(stmt, 0))){
			flags.sensor_conf = sqlite3_column_int(stmt, 1);	
		}else if(0 == strcmp("adc_type", (char*)sqlite3_column_text(stmt, 0))) {
			flags.adc_type = sqlite3_column_int(stmt, 1);	
		}else if(0 == strcmp("unit1_type", (char*)sqlite3_column_text(stmt, 0))) {
			flags.unit1_type =  sqlite3_column_int(stmt, 1);
			flags.unit1_sub_type =  sqlite3_column_int(stmt, 2);
		}else if(0 == strcmp("unit2_type", (char*)sqlite3_column_text(stmt, 0))) {
			flags.unit2_type =  sqlite3_column_int(stmt, 1);
			flags.unit2_sub_type =  sqlite3_column_int(stmt, 2);
		}else if(0 == strcmp("unit3_type", (char*)sqlite3_column_text(stmt, 0))) {
			flags.unit3_type =  sqlite3_column_int(stmt, 1);
			flags.unit3_sub_type =  sqlite3_column_int(stmt, 2);
		}else if(0 == strcmp("unit4_type", (char*)sqlite3_column_text(stmt, 0))) {
			flags.unit4_type =  sqlite3_column_int(stmt, 1);
			flags.unit4_sub_type =  sqlite3_column_int(stmt, 2);
		}else if(0 == strcmp("unit5_type", (char*)sqlite3_column_text(stmt, 0))) {
			flags.unit5_type =  sqlite3_column_int(stmt, 1);
			flags.unit5_sub_type =  sqlite3_column_int(stmt, 2);
		}else if(0 == strcmp("unit6_type", (char*)sqlite3_column_text(stmt, 0))) {
			flags.unit6_type =  sqlite3_column_int(stmt, 1);
			flags.unit6_sub_type =  sqlite3_column_int(stmt, 2);
		}
		DEBUG(sqlite3_column_text(stmt, 0)<<" "<< sqlite3_column_int(stmt, 1) << " "<<  sqlite3_column_int(stmt, 2));
		rc = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}
int Convert::init_board_info()
{
	DEBUG(__func__);
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
	sprintf(sql, "select * from demarcate;");
	rc = sqlite3_prepare(db, sql, (int)strlen(sql), &stmt, NULL); 
	if(rc != SQLITE_OK) { 
		FATAL("SQL error:" << sql << "->" << sqlite3_errmsg(db));
	}   
	rc = sqlite3_step(stmt);
	int i=0;
	while(rc == SQLITE_ROW){
		myboard[i].sn = sqlite3_column_int(stmt, 0); 	
		sscanf((const char *)sqlite3_column_text(stmt, 1), "%x",  &myboard[i].channel_type);
		myboard[i].asr_minval = sqlite3_column_int(stmt, 2); 	
		myboard[i].asr_maxval = sqlite3_column_int(stmt, 3); 	
		myboard[i].phy_minval = sqlite3_column_int(stmt, 4); 	
		myboard[i].phy_maxval = sqlite3_column_int(stmt, 5); 	
		myboard[i].decimal_num = sqlite3_column_int(stmt, 6); 	
		myboard[i].ad_fixedpoint1 = sqlite3_column_int(stmt, 7); 	
		myboard[i].ad_fixedpoint2  = sqlite3_column_int(stmt, 8); 	
		myboard[i].ad_fixedpoint3  = sqlite3_column_int(stmt, 9); 	
		myboard[i].ad_fixedpoint4  = sqlite3_column_int(stmt, 10); 	

		rc = sqlite3_step(stmt);
		i++;
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
#if 0
	for(i=0; i<48; i++){
		printf("%d\t%d\t%f\t%f\t%f\t%f\t%d\t%d\t%d\t%d\t%d\n", \
				myboard[i].sn,  myboard[i].channel_type, myboard[i].asr_minval, myboard[i].asr_maxval,\
				myboard[i].phy_minval, myboard[i].phy_maxval,myboard[i].decimal_num, myboard[i].ad_fixedpoint1,\
				myboard[i].ad_fixedpoint2,myboard[i].ad_fixedpoint3,myboard[i].ad_fixedpoint4 );
	}
#endif

}

int Convert::ad_to_asr(int sensor_number, unsigned char (&buffer)[BUFFER_SIZE])
{
	int unit_type=0;
	int unit_sub_type=0;
	int ret=0;
	unsigned char *p = buffer;
	switch(sensor_number) {
		case 0x01: unit_type = flags.unit1_type; unit_sub_type = flags.unit1_sub_type; break;
		case 0x02: unit_type = flags.unit2_type; unit_sub_type = flags.unit2_sub_type; break;
		case 0x03: unit_type = flags.unit3_type; unit_sub_type = flags.unit3_sub_type; break;
		case 0x04: unit_type = flags.unit4_type; unit_sub_type = flags.unit4_sub_type; break;
		case 0x05: unit_type = flags.unit5_type; unit_sub_type = flags.unit5_sub_type; break;
		case 0x06: unit_type = flags.unit6_type; unit_sub_type = flags.unit6_sub_type; break;
		default: FATAL(__func__<<":error sensor_number");
	}

	DEBUG(__func__<< ": sensor_number:"<<sensor_number<< ", unit_type:"<<unit_type<<", unit_sub_type:"<<unit_sub_type);
#if 0
	for(int i=0;i<32;i++){
		printf("%.2x ", p[i]);
	}printf("\n");
	printf("===>ad:%x\n", (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|(p[3]));
#endif

	switch (unit_type) {
		case 0x01: {//模拟信号采集(电流&电压)
			for(int channel_number = 0; channel_number<8; channel_number++,p+=4)
				asr_val[channel_number] = ad_to_asr_channel((sensor_number-1)*8+channel_number, (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|(p[3]), unit_sub_type);

			int sensor_type = 0x00;
			if(!unit_sub_type) { //电流信号,0xC0 ~ 0xC7
				sensor_type = 0xC0;
			} else { //电压信号 //0xC8 ~ 0xCF
				sensor_type = 0xC8;
			}
			memset(buffer, '\0', sizeof(buffer));
			for(int i=0;i<8;i++){
				DEBUG("asr_val["<<i<<"] = "<< asr_val[i]);
				buffer[i*4+0] = sensor_type+i;//数据组名称
				buffer[i*4+1] = 0x80 + myboard[(sensor_number-1)*8+i].decimal_num;//数据组格式
				int val = (int)(asr_val[i]*pow(10, myboard[(sensor_number-1)*8+i].decimal_num));
				buffer[i*4+2] = (val>>8)&0xFF;
				buffer[i*4+3] = (val)&0xFF;
			}
			ret = 32;
			break;
		}

		case 0x02://模拟信号输出(电流): 0xD0 ~ 0xD3
			for(int channel_number = 0; channel_number<6; channel_number++,p+=4)
				asr_val[channel_number] = ad_to_asr_channel((sensor_number-1)*8+channel_number, (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|(p[3]), unit_sub_type);
			memset(buffer, '\0', sizeof(buffer));
			for(int i=0;i<4;i++){
				buffer[i*4+0] = 0xD0+i;//数据组名称
				buffer[i*4+1] = 0x80 + myboard[(sensor_number-1)*8+i].decimal_num;//数据组格式
				int val = (int)(asr_val[i]*pow(10, myboard[(sensor_number-1)*8+i].decimal_num));
				buffer[i*4+2] = (val>>8)&0xFF;
				buffer[i*4+3] = (val)&0xFF;
			}
			ret = 16;
			break;

		case 0x03: {//开关量输入:0xB1 ~ 0xB8
			unsigned char tmp_buffer[32];
			for(int i=0;i<32;i+=4){
				tmp_buffer[i+0] = 0xB1+i/4;//数据组名称
				tmp_buffer[i+1] = 0x40;//数据组格式
				if(1 ==  buffer[i/4]){
					tmp_buffer[i+2] = 0xFF;
					tmp_buffer[i+3] = 0xFF;
				}else {
					tmp_buffer[i+2] = 0x00;
					tmp_buffer[i+3] = 0x00;
				}
			}
			memcpy(buffer, tmp_buffer, sizeof(tmp_buffer));
			ret = 32;
			break;
		}
		case 0x04: {//开关量输出:0xA1 ~ 0xA4
			unsigned char tmp_buffer[16];
			for(int i=0;i<16;i+=4){
				tmp_buffer[i+0] = 0xA1+i/4;//数据组名称
				tmp_buffer[i+1] = 0x40;//数据组格式
				if(1 ==  buffer[i/4]){
					tmp_buffer[i+2] = 0xFF;
					tmp_buffer[i+3] = 0xFF;
				}else {
					tmp_buffer[i+2] = 0x00;
					tmp_buffer[i+3] = 0x00;
				}
			}

			memcpy(buffer, tmp_buffer, sizeof(tmp_buffer));
			ret = 16;
			break;
		}
	}
	
	return ret;
}

//channel_num: 0~47
float Convert::ad_to_asr_channel(int channel_num, int ad_val,int asr_type)
{
	int i = channel_num;
	float asr_min, asr_mmin, asr_mid, asr_mmax, asr_max;
	float result;
	if(0 == asr_type) { //mA
		asr_min	 = 4;
		asr_mmin = 10;
		asr_mid = 12;
		asr_mmax = 15;
		asr_max = 20;
	} else {	//V
		asr_min	 = 0;
		asr_mmin = 1.5;
		asr_mid = 2.5;
		asr_mmax = 3.0;
		asr_max = 5.0;
	}
	switch(flags.fixed_point_number) {
		case 2:
			result = (asr_max-asr_min)*(ad_val-myboard[i].ad_fixedpoint1) / (myboard[i].ad_fixedpoint4-myboard[i].ad_fixedpoint1) + asr_min;
#if 0
			printf("asr_max:%f\n", asr_max);
			printf("asr_min:%f\n", asr_min);
			printf("ad_val:%d\n", ad_val);
			printf("ad_fixedpoint1:%d\n", myboard[i].ad_fixedpoint1);
			printf("ad_fixedpoint4:%d\n", myboard[i].ad_fixedpoint4);
			printf("asr val:%f\n", result);
#endif
			break;
		case 3:
			if(ad_val < myboard[i].ad_fixedpoint2) {
				result = (asr_mid-asr_min)*(ad_val-myboard[i].ad_fixedpoint1) / (myboard[i].ad_fixedpoint2-myboard[i].ad_fixedpoint1) + asr_min;
			} else {
				result = (asr_max-asr_mid)*(ad_val-myboard[i].ad_fixedpoint2) / (myboard[i].ad_fixedpoint4-myboard[i].ad_fixedpoint2) + asr_mid;
			}
			break;
		case 4:
			if(ad_val < myboard[i].ad_fixedpoint2) {
				result = (asr_mmin-asr_min)*(ad_val-myboard[i].ad_fixedpoint1) / (myboard[i].ad_fixedpoint2-myboard[i].ad_fixedpoint1) + asr_min;
			}else if(ad_val < myboard[i].ad_fixedpoint3) {
				result = (asr_mmax-asr_mmin)*(ad_val-myboard[i].ad_fixedpoint2) / (myboard[i].ad_fixedpoint3-myboard[i].ad_fixedpoint2) + asr_mmin;
			} else {
				result = (asr_max-asr_mmax)*(ad_val-myboard[i].ad_fixedpoint3) / (myboard[i].ad_fixedpoint4-myboard[i].ad_fixedpoint3) + asr_mmax;
			}
			break;
		default:
			FATAL("fixed_point_number:"<<flags.fixed_point_number<<", the fixed point number error!");
	}

	return result;

}


int Convert::asr_to_phy(int sensor_number,unsigned char (&buffer)[BUFFER_SIZE])
{
	int unit_type=0;
	int channel_number = 8;
	int ret=0;
	unsigned char *p = buffer;
	int four_bytes_count = 0;
	for(int i=0,j=0; i<channel_number; i++, j++) {
		channel_val[i] = asr_to_phy_channel((sensor_number-1)*8+i, asr_val[i]);	
		buffer[j*4+0] = myboard[(sensor_number-1)*8+i].channel_type;//数据组名称
		buffer[j*4+1] = 0x80 +  myboard[(sensor_number-1)*8+i].decimal_num;//数据组格式
		buffer[j*4+2] = (channel_val[i]>>8)&0xFF;
		buffer[j*4+3] = (channel_val[i])&0xFF;
		if(channel_val[i]>0xFFFF) {//两个数据组
			four_bytes_count ++;
			buffer[j*4+1] = (1<<7) + (1<<5) + (0<<4) + myboard[(sensor_number-1)*8+i].decimal_num;//数据组格式,低2字节
			buffer[j*4+4] = myboard[(sensor_number-1)*8+i].channel_type;//数据组名称
			buffer[j*4+5] = (1<<7) + (1<<5) + (1<<4) + myboard[(sensor_number-1)*8+i].decimal_num;//数据组格式，高2字节
			buffer[j*4+6] = (channel_val[i]>>24)&0xFF;
			buffer[j*4+7] = (channel_val[i]>>16)&0xFF;
			j++;
		
		}
	}

	return (channel_number+four_bytes_count)*4;
}

//channel_num: 0~47
int Convert::asr_to_phy_channel(int channel_num, float asr_val)
{
	int i=channel_num;
	int phy_val = (int)(((myboard[i].phy_maxval - myboard[i].phy_minval)*(asr_val-myboard[i].asr_minval) / \
				(myboard[i].asr_maxval - myboard[i].asr_minval) + myboard[i].phy_minval)*\
			(int)pow(10, myboard[i].decimal_num));

	return phy_val;

}


int Convert::asr_to_ad_channel(int sensor_num, int channel_num, float asr_val)
{
	int ad_val = 0;
	int i = (sensor_num-1)*8 + channel_num - 1;
	ad_val =(int)((myboard[i].ad_fixedpoint4-myboard[i].ad_fixedpoint1)*(asr_val - 4) / (20 -4) + myboard[i].ad_fixedpoint1);

	return ad_val;
}

int Convert::get_unit_type(int sensor_id)
{
	int unit_type = 0;
	switch (sensor_id) {
		case 0x01: unit_type = flags.unit1_type; break;
		case 0x02: unit_type = flags.unit2_type; break;
		case 0x03: unit_type = flags.unit3_type; break;
		case 0x04: unit_type = flags.unit4_type; break;
		case 0x05: unit_type = flags.unit5_type; break;
		case 0x06: unit_type = flags.unit6_type; break;
		default: FATAL("invalid sensor_id:"<< sensor_id ); break;
	}

	return unit_type;
	
}
int Convert::get_channel_sub_type(int channel_number)
{
	int unit_num = channel_number/8 + ((channel_number%8)?1:0);
	int ret = 0;
	switch (unit_num){
		case 0x01:
			ret = flags.unit1_sub_type;
		break;
		case 0x02:
			ret = flags.unit2_sub_type;
		break;
		case 0x03:
			ret = flags.unit3_sub_type;
		break;
		case 0x04:
			ret = flags.unit4_sub_type;
		break;
		case 0x05:
			ret = flags.unit5_sub_type;
		break;
		case 0x06:
			ret = flags.unit6_sub_type;
		break;
	}

	return ret;
}
