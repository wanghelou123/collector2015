#include <string.h>
#include "modbus.h"
#include "Log.h"
struct board_info{
	int ain_channel_num;
	int aout_channel_num;
	int din_channel_num;
	int dout_channel_num;
}collect_board;

int process_modbus_data(unsigned char (&tcp_modbus_buf)[256])
{
	int ret=0;
	if(!check_modbus_packet(tcp_modbus_buf)) {
		return 9;
	}
	switch(tcp_modbus_buf[7]) {
		case 0x01:
			ret = read_node_status(tcp_modbus_buf);
			break;
		case 0x03:
			ret = read_register(tcp_modbus_buf);
			break;
		case 0x10:
			ret = write_register(tcp_modbus_buf);
			break;
	}

	return ret;
}

bool check_modbus_packet(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	int exception_code = 0;
	unsigned char err_buf[256]={0};
	memcpy(err_buf, tcp_modbus_buf, 7);
	err_buf[5] = 0x03;
	err_buf[7] = tcp_modbus_buf[7] + 0x80;	
	if(!check_protocol_identifier(tcp_modbus_buf)) {
		FATAL("check_protocol identifier error");
		exception_code = 0x0C;
	}
	else if(!check_length(tcp_modbus_buf)) {
		FATAL("check_length error");	
		exception_code = 0x0D;
	}
	else if(!check_uid(tcp_modbus_buf)){
		FATAL("check_uid error");	
		exception_code = 0x0E;
	}
	else if(!check_func_node(tcp_modbus_buf)){
		FATAL("check_func_node error");
		exception_code = 0x01;
	}
	else if(!check_start_addr(tcp_modbus_buf)) {
		FATAL("check_start_addr error");	
		exception_code = 0x02;
	}
	else if(!check_data_num(tcp_modbus_buf)) {
		FATAL("check_data_num error");
		exception_code = 0x03;
	}

	if(exception_code){
		err_buf[8] = exception_code;	
		memset(tcp_modbus_buf, '\0', sizeof(tcp_modbus_buf));
		memcpy(tcp_modbus_buf, err_buf, 9);
		return false;
	}

	return true;
}

bool check_protocol_identifier(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	if(MAKEWORD(tcp_modbus_buf[2], tcp_modbus_buf[3]) != 0x00) {
		return false;	
	}	
	return true;
}

bool check_length(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	int len = MAKEWORD(tcp_modbus_buf[4], tcp_modbus_buf[5]);	
	switch(tcp_modbus_buf[7]) {
		case 0x01:
		case 0x03:
			if((len + 6) != 0x0C) {
				return false;
			}
			break;
		case 0x10:
			if((len+6) != tcp_modbus_buf[12]+13) {
				return false;
			}
	}
	return true;
}

bool check_uid(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	switch(tcp_modbus_buf[7]){
		case 0x01:
			if(tcp_modbus_buf[6] != 0xFF) 
				return false;
			break;
		case 0x03:
			if(tcp_modbus_buf[6]>0x40 && tcp_modbus_buf[6]!=0xFF)
				return false;
			break;
		case 0x10:
			if(tcp_modbus_buf[6]>0x40)
				break;
	}

	return true;
}

bool check_func_node(unsigned char (&tcp_modbus_buf)[256])
{
	//for(int i=0;i<20; i++){printf("%.2x ", tcp_modbus_buf[i]);}printf("\n");
	DEBUG(__func__);
	switch(tcp_modbus_buf[7]){
		case 0x01:
		case 0x03:
		case 0x10:
			break;
		default:
			DEBUG("func node: " << tcp_modbus_buf[7]);
			return false;
	}


	return true;
}

bool check_start_addr(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	int start_addr = MAKEWORD(tcp_modbus_buf[8], tcp_modbus_buf[9]);
	int data_num = MAKEWORD(tcp_modbus_buf[10], tcp_modbus_buf[11]);
	int uid = tcp_modbus_buf[6];
	switch(tcp_modbus_buf[7]){
		case 0x01:
			if(start_addr > 0x5595 || (start_addr-0x5555+data_num)>0x40 ||
					start_addr<0x5555)	{
				return false;
			}
			break;
		case 0x03:
			if(uid<=0x40) {
				if(start_addr > 0x0040 || start_addr%2 != 0) {
					return false;
				}
			}
			if(uid==0xFF) {
				if(start_addr != 0x0000 && start_addr != 0x0008 &&
						start_addr != 0x0010 && start_addr != 0x0018 &&
						start_addr != 0x0020 && start_addr !=0x0029) {
					return false;		
				}
			}
			break;
		case 0x10:
			if(start_addr > 0x0040) {
				return false;
			}
			break;
	}
	return true;
}

bool check_data_num(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	int start_addr = MAKEWORD(tcp_modbus_buf[8], tcp_modbus_buf[9]);
	int data_num = MAKEWORD(tcp_modbus_buf[10], tcp_modbus_buf[11]);
	int uid = tcp_modbus_buf[6];
	switch(tcp_modbus_buf[7]){
		case 0x01:
			if(data_num > 0x40) {
				return false;
			}
			break;
		case 0x03:
			if(uid < 0x40) {
				if(start_addr + data_num > 0x40)	{
					return false;
				}
			}else if(uid == 0xFF) {
				switch(start_addr == 0x0000) if(data_num != 0x0008) return false; break;
				switch(start_addr == 0x0008) if(data_num != 0x0008) return false; break;
				switch(start_addr == 0x0010) if(data_num != 0x0008) return false; break;
				switch(start_addr == 0x0018) if(data_num != 0x0008) return false; break;
				switch(start_addr == 0x0020) if(data_num != 0x0009) return false; break;
				switch(start_addr == 0x0029) if(data_num != 0x0008) return false; break;
			}
			break;
		case 0x10:
			if(data_num > 0x40) {
				return false;
			}
			break;
	}

	return true;
}

unsigned int crc(unsigned char *buf,int start,int cnt) 
{
	int     i,j;
	unsigned    temp,temp2,flag;

	temp=0xFFFF;

	for (i=start; i<cnt; i++)
	{   
		temp=temp ^ buf[i];

		for (j=1; j<=8; j++)
		{   
			flag=temp & 0x0001;
			temp=temp >> 1;
			if (flag) temp=temp ^ 0xA001;
		}   
	}   

	/* Reverse byte order. */

	temp2=temp >> 8;
	temp=(temp << 8) | temp2;
	temp &= 0xFFFF;

	return(temp);
}

int read_node_status(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	int start_addr = MAKEWORD(tcp_modbus_buf[8], tcp_modbus_buf[9])-0x5555;
	int data_num = MAKEWORD(tcp_modbus_buf[10], tcp_modbus_buf[11]);
	unsigned char ret_buf[256]={0};
	unsigned char node_status_buf[64]={1,1,1,1,1,1};
	memcpy(ret_buf, tcp_modbus_buf, 8);
	ret_buf[8] = data_num/8+(data_num%8)?1:0;
	for(int i=start_addr;i<data_num;i++) {
		ret_buf[9+i/8] |= node_status_buf[i]<<i;
	}
	memset(tcp_modbus_buf, '\0', sizeof(tcp_modbus_buf));
	memcpy(tcp_modbus_buf, ret_buf, sizeof(ret_buf));

	return ret_buf[8]+9;
}

int read_register(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	int start_addr = MAKEWORD(tcp_modbus_buf[8], tcp_modbus_buf[9]);
	int data_num = MAKEWORD(tcp_modbus_buf[10], tcp_modbus_buf[11]);
	int uid = tcp_modbus_buf[6];
	commMcu mcu;
	unsigned char recv_buf[1024];
	unsigned char node_buf[6][128]={0};
	int n = mcu.Read(recv_buf);

	int node_type = recv_buf[2];
	int node_len = recv_buf[3];
	int node_bytes=0;
	unsigned char *p = recv_buf+4;
#if 1
	unsigned char *p_recv = recv_buf;
	int a = *(p_recv+3);
	int b = *(p_recv+3+a+2);
	int c = *(p_recv+3+a+2+b+2);
	int d = *(p_recv+3+a+2+b+2+c+2);
	printf("========>a:%d, b:%d, c:%d, d:%d\n",3, 3+a+2, 3+a+2+b+2, 3+a+2+b+2+c+2);
	printf("a:%d, b:%d, c:%d, d:%d\n", a,b,c,d);
	collect_board.ain_channel_num = a/4;
	collect_board.aout_channel_num = b/4;
	collect_board.din_channel_num = c/4;
	collect_board.dout_channel_num = d/4;
	DEBUG("ain_channel_num:" << collect_board.ain_channel_num << "aout_channel_num: "<<collect_board.aout_channel_num << "din_cout:"<< collect_board.din_channel_num << "dout_channel_num:"<< collect_board.dout_channel_num);

#endif
	for(int i=0; i<6; i++) {
		if(node_type%2 == 0){ 
			node_bytes = 16; //模拟量输出，开关量输出 
		}else {
			node_bytes = 32;//模拟量采集，开关量采集
		}
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
		node_type = *p;
		node_len = *(p+1);
	}
	DEBUG("node_type: "<< node_type);
	DEBUG("node_len: "<<node_len);
#if 1
	for(int i=0; i<6; i++) {
		for(int j=0; j<32; j++) {
			printf("%.2x ", node_buf[i][j]);
		}
		printf("\n");
	}
	printf("\n");
#endif
	memset(tcp_modbus_buf+9, '\0', sizeof(tcp_modbus_buf)-9);
	tcp_modbus_buf[5] = data_num+3;
	tcp_modbus_buf[8] = data_num*2;
	if(uid<=6)
		memcpy(tcp_modbus_buf+9, node_buf[uid-1]+start_addr, data_num*2);

	return 9+data_num*2;
}

//继电器输出
bool relay_output(int sensor_id, int channel_id, int data)
{
	DEBUG(__func__<< " sensor_id:"<<sensor_id << " channel_id:"<< channel_id<<" data:"<<data);
	int zigbee_node_start = collect_board.ain_channel_num/8 + collect_board.aout_channel_num/4 + collect_board.din_channel_num/8+1;
	int zigbee_node_end = collect_board.ain_channel_num/8 + collect_board.aout_channel_num/4 + collect_board.din_channel_num/8 + collect_board.dout_channel_num/4;

	printf("collect_board.ain_channel_num=%d\n", collect_board.ain_channel_num);
	printf("collect_board.aout_channel_num=%d\n", collect_board.aout_channel_num);
	printf("collect_board.din_channel_num=%d\n", collect_board.din_channel_num);
	printf("collect_board.dout_channel_num=%d\n", collect_board.dout_channel_num);
	if(sensor_id > 6 || sensor_id < zigbee_node_start || sensor_id > zigbee_node_end) {
		FATAL("zigbee_node_start="<<zigbee_node_start<<", zigbee_node_end:"<< zigbee_node_end);
		return false;
	}
	unsigned char send_buf[1024] = {0x01,0x05};
	unsigned char recv_buf[1024] = {0x00};
	send_buf[2]= 0x04; //控制类型，开关量输出
	send_buf[3]= sensor_id - zigbee_node_start + channel_id;//通道编号
	send_buf[4]= (data>>24)&0xFF;
	send_buf[5]= (data>>16)&0xFF;
	send_buf[6]= (data>>8)&0xFF;
	send_buf[7]= data&0xFF;

	unsigned short crc_calc = 0;
	crc_calc = crc( send_buf, 0, 10 - 2 );
	send_buf[8]=((unsigned)crc_calc >>8)&0xFF;
	send_buf[9]=((unsigned)crc_calc)&0xFF;
	commMcu mcu;
	int n = mcu.Write(send_buf, 10, recv_buf);
	if(0 != memcmp(send_buf, recv_buf, 10)) {
		FATAL(__func__ <<  " controle relay error");
		return false;
	}

	return true;
}

int write_register(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	int error_flag = 0;
	unsigned char channel_type;
	int channel_num = tcp_modbus_buf[12]/4;
	int start_addr = MAKEWORD(tcp_modbus_buf[8], tcp_modbus_buf[9]);
	unsigned char *p = tcp_modbus_buf + 13;
	unsigned char right_buf[12];
	unsigned char error_buf[9];
	memcpy(right_buf, tcp_modbus_buf, sizeof(right_buf));
	memcpy(error_buf, tcp_modbus_buf, sizeof(error_buf));
	right_buf[5] = 0x06;
	error_buf[5] =0x03;
	error_buf[7] = tcp_modbus_buf[7]+0x80;
	error_buf[8] = 0x04;

	channel_type = p[0]&0xF0;	
	for(int i=0; i<channel_num; i++) {//要写多少个通道
		if((p[0]&0x0F)<1 || (p[0]&0x0F) >4)	{
			FATAL("ivalid channel ctl!");
			error_flag=1;
			break;
		}
		switch(channel_type) {
			case 0xA0://开关量输出
				if((0xA0|((start_addr+2)/2))==p[0] && 0x40==p[1] && 0xFF==p[2] && 0xFF==p[3]) {
					if (!relay_output(tcp_modbus_buf[6], p[0]&0x0F, 1)) {
						break;
					}
				}
				else if((0xA0|((start_addr+2)/2))==p[0] && 0x40==p[1] && 0x00==p[2] && 0x00==p[3]) {
					if (!relay_output(tcp_modbus_buf[6], p[0]&0x0F, 0)) {
						break;
					}
				}else {
					FATAL("invalid channel data");
					printf("%x %x %x %x %x\n", (0xA0|(i+i+1)),p[0], p[1], p[2], p[3] );
				}
				break;
			case 0xD0: //模拟量输出
				if((0xD0|((start_addr+2)/2))==p[0] && 0x40==p[1]) {
					if (!relay_output(tcp_modbus_buf[6], p[0]&0x0F, 1)) {
						break;
					}
				}
				else {
					FATAL("invalid channel data");
					printf("%x %x %x %x %x\n", (0xA0|(i+i+1)),p[0], p[1], p[2], p[3] );
				}
				break;
			default:
				error_flag=1;
				break;

		}

		p+=4;//移动到下一个通道
		start_addr+=2;
	}

	memset(tcp_modbus_buf, '\0', sizeof(tcp_modbus_buf));
	if(error_flag){
		memcpy(tcp_modbus_buf, error_buf, sizeof(error_buf));	
		return sizeof(error_buf);
	}else{
		memcpy(tcp_modbus_buf, right_buf, sizeof(right_buf));
		return sizeof(right_buf);
	}
}
