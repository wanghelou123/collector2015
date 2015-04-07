#include <string.h>
#include "modbus.h"
#include "Log.h"
#define MAXLINE 80
struct DeivceInfo{
	char dev_ip[20];
	char dev_mask[20];
	char dev_gateway[20];
	char dev_dns[20];
	char dev_mac[20];
	char dev_serialno[20];
}collect_board;

ModbusTcp::ModbusTcp()
{
	DEBUG("construct the ModbusTcp object!");	 
}
ModbusTcp::~ModbusTcp()
{
	DEBUG("destory the ModbusTcp object!");
}
int ModbusTcp::process_modbus_data(unsigned char (&tcp_modbus_buf)[256])
{
	int ret=0;
	if(!check_modbus_packet(tcp_modbus_buf)) {
		printf("will response the error packet: ");
		for(int i=0; i<9; i++) {
			printf("%.2x ", tcp_modbus_buf[i]);
		}printf("\n");

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

bool ModbusTcp::check_modbus_packet(unsigned char (&tcp_modbus_buf)[256])
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
		char tmp_buffer[128];
		for(int i=0; i<tcp_modbus_buf[5]+6; i++) {
			snprintf(tmp_buffer+i*3, sizeof(tmp_buffer),"%.2x ", (char)tcp_modbus_buf[i]);	
		}
		FATAL("the error packet is:"<<tmp_buffer);
		err_buf[8] = exception_code;	
		memset(tcp_modbus_buf, '\0', sizeof(tcp_modbus_buf));
		memcpy(tcp_modbus_buf, err_buf, 9);
		return false;
	}

	return true;
}

bool ModbusTcp::check_protocol_identifier(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	if(MAKEWORD(tcp_modbus_buf[2], tcp_modbus_buf[3]) != 0x00) {
		return false;	
	}	
	return true;
}

bool ModbusTcp::check_length(unsigned char (&tcp_modbus_buf)[256])
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

bool ModbusTcp::check_uid(unsigned char (&tcp_modbus_buf)[256])
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

bool ModbusTcp::check_func_node(unsigned char (&tcp_modbus_buf)[256])
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

bool ModbusTcp::check_start_addr(unsigned char (&tcp_modbus_buf)[256])
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

bool ModbusTcp::check_data_num(unsigned char (&tcp_modbus_buf)[256])
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
				switch(start_addr&0xFF) {
					case 0x00: if(data_num != 0x0008) return false; break;
					case 0x08: if(data_num != 0x0008) return false; break;
					case 0x10: if(data_num != 0x0008) return false; break;
					case 0x18: if(data_num != 0x0008) return false; break;
					case 0x20: if(data_num != 0x0009) return false; break;
					case 0x29: if(data_num != 0x0008) return false; break;
				}
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

unsigned int ModbusTcp::crc(unsigned char *buf,int start,int cnt) 
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

void ModbusTcp::strcopyx(char *desc, char *src){
	while (*src != '\0') {
		if (*src == '\r' || *src == '\n') {
			src++;
			break;
		}   
		*desc++ = *src++;
	}           
} 
void ModbusTcp::get_device_info()
{
	FILE *fp;
	char buffer[128];
	char *pos;

	fp = fopen("/etc/gateway/network.txt", "r");

	if(NULL == fp){
		perror("fopen:");
	}


	while(fgets(buffer, MAXLINE, fp)){
		pos=strrchr(buffer, '=');
		if(pos){
			char tmp[128]={0};
			strncpy(tmp, buffer, strlen(buffer)-strlen(pos));
			pos++;
			if(strcmp(tmp, "Serialnum") == 0){
				strcopyx(collect_board.dev_serialno, pos);
				break;
			}
		}
	}
	rewind(fp);
	while(fgets(buffer, MAXLINE, fp)){
		pos=strrchr(buffer, '=');
		if(pos){
			char tmp[128]={0};
			strncpy(tmp, buffer, strlen(buffer)-strlen(pos));
			pos++;
			if(strcmp(tmp, "IP") == 0){
				strcopyx(collect_board.dev_ip, pos);
				break;
			}
		}
	}

	rewind(fp);
	while(fgets(buffer, MAXLINE, fp)){
		pos=strrchr(buffer, '=');
		if(pos){
			char tmp[128]={0};
			strncpy(tmp, buffer, strlen(buffer)-strlen(pos));
			pos++;
			if(strcmp(tmp, "Mask") == 0){
				strcopyx(collect_board.dev_mask, pos);
				break;
			}
		}
	}

	rewind(fp);
	while(fgets(buffer, MAXLINE, fp)){
		pos=strrchr(buffer, '=');
		if(pos){
			char tmp[128]={0};
			strncpy(tmp, buffer, strlen(buffer)-strlen(pos));
			pos++;
			if(strcmp(tmp, "Gateway") == 0){
				strcopyx(collect_board.dev_gateway, pos);
				break;
			}
		}
	}

	rewind(fp);
	while(fgets(buffer, MAXLINE, fp)){
		pos=strrchr(buffer, '=');
		if(pos){
			char tmp[128]={0};
			strncpy(tmp, buffer, strlen(buffer)-strlen(pos));
			pos++;
			if(strcmp(tmp, "DNS") == 0){
				strcopyx(collect_board.dev_dns, pos);
				break;
			}
		}
	}
	rewind(fp);
	while(fgets(buffer, MAXLINE, fp)){
		pos=strrchr(buffer, '=');
		if(pos){
			char tmp[128]={0};
			strncpy(tmp, buffer, strlen(buffer)-strlen(pos));
			pos++;
			if(strcmp(tmp, "MAC") == 0){
				strcopyx(collect_board.dev_mac, pos);
				break;
			}
		}
	}

	fclose(fp);
}

int ModbusTcp::read_node_status(unsigned char (&tcp_modbus_buf)[256])
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

int ModbusTcp::read_register(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	int start_addr = MAKEWORD(tcp_modbus_buf[8], tcp_modbus_buf[9]);
	int data_num = MAKEWORD(tcp_modbus_buf[10], tcp_modbus_buf[11]);
	int ret = 9+data_num*2;
	int uid = tcp_modbus_buf[6];
	memset(tcp_modbus_buf+9, '\0', sizeof(tcp_modbus_buf)-9);
	tcp_modbus_buf[5] = data_num*2+3;
	tcp_modbus_buf[8] = data_num*2;

	if(uid<=6) {
		unsigned char node_buf[BUFFER_SIZE];
		int n = conver.get_node_data(uid, node_buf);
		if(n<0) {
			tcp_modbus_buf[5] = 0x03;
			tcp_modbus_buf[7] += 0x80;
			tcp_modbus_buf[8] = 0x04;
			ret = 9;	
		}else{
			memcpy(tcp_modbus_buf+9, node_buf+start_addr, data_num*2);
		}
	}else if(uid == 0xFF) {
		get_device_info();
		switch(start_addr&0xFF) {
			case 0x00: memcpy(tcp_modbus_buf+9, collect_board.dev_ip,data_num*2); break;
			case 0x08: memcpy(tcp_modbus_buf+9, collect_board.dev_mask,data_num*2); break;
			case 0x10: memcpy(tcp_modbus_buf+9, collect_board.dev_gateway,data_num*2); break;
			case 0x18: memcpy(tcp_modbus_buf+9, collect_board.dev_dns,data_num*2); break;
			case 0x20: memcpy(tcp_modbus_buf+9, collect_board.dev_mac,data_num*2); break;
			case 0x29: memcpy(tcp_modbus_buf+9, collect_board.dev_serialno,data_num*2); break;
		}

	}

	return ret;
}

//模拟量、 继电器输出
bool ModbusTcp::relay_output(int sensor_id, int channel_id,int control_type,  int data)
{
	DEBUG(__func__<< " sensor_id:"<<sensor_id << " channel_id:"<< channel_id<<" control_type:"<< control_type<<" data:"<<data);
	int unit_type = 0;
	int unit_find_count=0;
	for(int i=0; i< sensor_id; i++) {
		unit_type = conver.get_unit_type(i+1);
		//printf("unit_type = %d, unit_find_count = %d\n", unit_type, unit_find_count);
		if(control_type == unit_type) {
			unit_find_count++;
		}
	}
	channel_id += (--unit_find_count)*4;

	//printf("channel_id = %d\n", channel_id);

	unsigned char send_buf[1024] = {0x01,0x05};
	unsigned char recv_buf[1024] = {0x00};
	send_buf[2]= control_type;  //控制类型，开关量 or 模拟量
	send_buf[3]= channel_id;	//通道编号
	send_buf[4]= (data>>24)&0xFF;
	send_buf[5]= (data>>16)&0xFF;
	send_buf[6]= (data>>8)&0xFF;
	send_buf[7]= data&0xFF;

	unsigned short crc_calc = 0;
	crc_calc = crc( send_buf, 0, 10 - 2 );
	send_buf[8]=((unsigned)crc_calc >>8)&0xFF;
	send_buf[9]=((unsigned)crc_calc)&0xFF;
	int n = mcu.Write(send_buf, 10, recv_buf);
	if((n<0) || (0 != memcmp(send_buf, recv_buf, 10))) {
		FATAL(__func__ <<  " controle relay error");
		return false;
	}

	return true;
}

int ModbusTcp::write_register(unsigned char (&tcp_modbus_buf)[256])
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
					if (!relay_output(tcp_modbus_buf[6], p[0]&0x0F, 0x04, 0x01)) {
						error_flag=1;
						break;
					}
				}
				else if((0xA0|((start_addr+2)/2))==p[0] && 0x40==p[1] && 0x00==p[2] && 0x00==p[3]) {
					if (!relay_output(tcp_modbus_buf[6], p[0]&0x0F, 0x04, 0x00)) {
						error_flag=1;
						break;
					}
				}else {
					FATAL("invalid channel data");
					printf("%x %x %x %x %x\n", (0xA0|(i+i+1)),p[0], p[1], p[2], p[3] );
					error_flag=1;
				}
				break;
			case 0xD0: //模拟量输出
				if((0xD0|((start_addr+2)/2))==p[0] && 0x83==p[1]) {
					int ad_val = conver.asr_to_ad_channel(tcp_modbus_buf[6], p[0]&0x0F, ((p[2]<<8)|p[3])/1000);
					if (!relay_output(tcp_modbus_buf[6], p[0]&0x0F, 0x02, ad_val)) {
						error_flag=1;
						break;
					}
				}
				else {
					error_flag=1;
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
