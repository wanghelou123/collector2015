#include "Log.h"
#include "StdModbusTcp.h"

StdModbusTcp::StdModbusTcp()
{
	switch_input_channel_num = 0;		//开关量输入
	relay_output_channel_num = 0;		//继电器输出
	input_register_channel_num = 0;		//电流、电压采集
	holding_register_channel_num = 0;	//电流输出

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

	DEBUG("\ninput_register_channel_num = "<< input_register_channel_num
			<<"\nholding_register_channel_num = "<<holding_register_channel_num 	
			<<"\nswitch_input_channel_num = "<<switch_input_channel_num
			<<"\nrelay_output_channel_num = "<<relay_output_channel_num  );
}

StdModbusTcp::~StdModbusTcp()
{

}

int StdModbusTcp::process_modbus_data(unsigned char (&modbus_buffer)[256], int len)
{
	int ret = 0;
	int func_code = modbus_buffer[7];

	if(!check_modbus_packet(modbus_buffer, len)) {//错误响应包
		return 9;
	}

	switch (func_code) {
		case 0x01:
			ret = read_coil_status(modbus_buffer);
		break;
		case 0x03:
			ret = read_holding_registers(modbus_buffer);
		break;
		case 0x04:
			ret = read_intput_registers(modbus_buffer);
		break;
		case 0x05:
			ret = force_single_coil(modbus_buffer, len);
		break;
		case 0x06:
			ret = preset_single_register(modbus_buffer, len);
		break;
	}
	
	return ret;
}

bool StdModbusTcp::check_modbus_device_status(unsigned char (&modbus_buffer)[256])
{
	DEBUG(__func__);
	switch(modbus_buffer[7]){
		case 0x01:{
		
			int start_addr = MAKEWORD(modbus_buffer[8], modbus_buffer[9]);

			if(start_addr > RELAYOUT_STARTADDR){
				if(relay_output_channel_num == 0)return false; break;
			}
			else{
				if(switch_input_channel_num == 0)return false; break;
			}
		}
		case 0x03:
			if(holding_register_channel_num == 0 )return false; break;
		case 0x04:
			if(input_register_channel_num == 0) return false; break;
		case 0x05:
			if(relay_output_channel_num == 0)return false; break;
		case 0x06:
			if(holding_register_channel_num == 0 )return false; break;
	}

	return true;
}

bool StdModbusTcp::check_modbus_packet(unsigned char (&modbus_buffer)[256], int len)
{
	DEBUG(__func__);
	int exception_code = 0;

	if(!check_modbus_device_status(modbus_buffer)){
		FATAL("check modbus device status error!");
		exception_code = 0x04;
	}
	else if(!check_protocol_identifier(modbus_buffer)) {
		FATAL("check_protocol identifier error");
		modbus_buffer[2] = 0x00;
		modbus_buffer[3] = 0x00;
		exception_code = 0x0C;
	}
	else if(!check_length(modbus_buffer,len)) {
		FATAL("check_length error");	
		exception_code = 0x0D;
	}
	else if(!check_uid(modbus_buffer)){
		FATAL("device ID error!")
		exception_code = 0x0E;
	}
	else if(!check_func_node(modbus_buffer)){
		FATAL("check_func_node error");
		exception_code = 0x01;
	}
	else if(!check_start_addr(modbus_buffer)) {
		FATAL("check_start_addr error");	
		exception_code = 0x02;
	}
	else if(!check_data_num(modbus_buffer)) {
		FATAL("check_data_num error");
		exception_code = 0x03;
	}

	if(exception_code){
		char tmp_buffer[128];
		for(int i=0; i<8; i++) {
			snprintf(tmp_buffer+i*3, sizeof(tmp_buffer),"%.2x ", (char)modbus_buffer[i]);	
		}
		FATAL("the error packet is:"<<tmp_buffer);
		modbus_buffer[5] = 0x03;
		modbus_buffer[7] += 0x80;
		modbus_buffer[8] = exception_code;

		return false;
	}

	return true;
}

bool StdModbusTcp::check_protocol_identifier(unsigned char (&tcp_modbus_buf)[256])
{
	DEBUG(__func__);
	if(MAKEWORD(tcp_modbus_buf[2], tcp_modbus_buf[3]) != 0x00) {
		return false;	
	}	
	return true;
}

bool StdModbusTcp::check_length(unsigned char (&tcp_modbus_buf)[256], int len)
{
	DEBUG(__func__);

	int n = MAKEWORD(tcp_modbus_buf[4], tcp_modbus_buf[5]);	

	return ((n + 6) != 0x0C || len != 0x0C)?false:true; 
}

bool StdModbusTcp::check_uid(unsigned char (&modbus_buffer)[256])
{
	return (0x01 == modbus_buffer[6]) ? true : false;
}

bool StdModbusTcp::check_func_node(unsigned char (&modbus_buffer)[256])
{
	switch (modbus_buffer[7]) {
		case 0x01:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
			return true;
			break;
		default:
			return false;
			break;
	}
}

bool StdModbusTcp::check_start_addr(unsigned char (&modbus_buffer)[256])
{
	int start_addr = MAKEWORD(modbus_buffer[8], modbus_buffer[9]);
	int func_code = modbus_buffer[7];
	switch (func_code) {
		case 0x01:
			if(start_addr<0 || \
					(start_addr>switch_input_channel_num+SWITCH_INPUT_STARTADDR-1	 && start_addr<RELAYOUT_STARTADDR)|| \
					start_addr>relay_output_channel_num+RELAYOUT_STARTADDR-1)
				return false;
			break;
		case 0x03:
			if(start_addr<0 || start_addr>holding_register_channel_num+HOLDING_REGISTER_STARTADDR-1)
				return false;
			break;
		case 0x04:
			if(start_addr<0 || start_addr>input_register_channel_num+INPUT_REGISTER_STARTADDR-1)
				return false;
			break;
		case 0x05:
			if(start_addr<RELAYOUT_STARTADDR || \
					start_addr>relay_output_channel_num+RELAYOUT_STARTADDR-1)
				return false;
			break;
		case 0x06:
			if(start_addr<0 || start_addr>holding_register_channel_num+HOLDING_REGISTER_STARTADDR-1)
				return false;
			break;
		default:
			return false;
			break;
	}

	return true;
}

bool StdModbusTcp::check_data_num(unsigned char (&modbus_buffer)[256])
{
	int start_addr = MAKEWORD(modbus_buffer[8], modbus_buffer[9]);
	int data = MAKEWORD(modbus_buffer[10], modbus_buffer[11]);
	int func_code =modbus_buffer[7];
	switch (func_code) {
		case 0x01:
			if(((start_addr+data)>switch_input_channel_num && (start_addr+data)<relay_output_channel_num+RELAYOUT_STARTADDR)||(start_addr+data)>relay_output_channel_num+RELAYOUT_STARTADDR)
				return false;
			break;
		case 0x03:
			if(start_addr+data>holding_register_channel_num)
				return false;
			break;
		case 0x04:
			if(start_addr+data>input_register_channel_num)
				return false;
			break;
		case 0x05:
			if(data!=0x0000 && data!=0xFF00)
				return false;
			break;
		case 0x06://4~20mA,3小数
			if(data<4000 || data>20000)
				return false;
			break;
		default:
			return false;
			break;
	}

	return true;
}

unsigned int StdModbusTcp::read_coil_status(unsigned char (&modbus_buffer)[256])
{
	DEBUG(__func__);
	unsigned char recv_buf[1024];
	unsigned char *p = recv_buf;
	int n = mcu.Read(recv_buf);
	int ain_num = *(p+3);
	int aout_num = *(p +2 + 2 + ain_num + 1);
	int din_num = *(p + 2 + 2 + ain_num + 2 + aout_num + 1);
	unsigned char * din_addr = p + 2 + 2 + ain_num + 2 + aout_num + 2;
	int dout_num = *(p + 2 + 2 + ain_num + 2 + aout_num + 2 + din_num + 1);
	unsigned char * dout_addr = p + 2 + 2 + ain_num + 2 + aout_num + 2 + din_num + 2;
/*
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
*/
	int start_addr = MAKEWORD(modbus_buffer[8], modbus_buffer[9]);
	int data_num = MAKEWORD(modbus_buffer[10], modbus_buffer[11]);

	modbus_buffer[8] = data_num/8 + ((data_num % 8)?1:0); //字节个数 
	memset(modbus_buffer+9, 0, sizeof(modbus_buffer)-9);

	if(start_addr < RELAYOUT_STARTADDR) {
		for(int i=start_addr; i<data_num; i++) {
			modbus_buffer[9+(i-start_addr)/8] |= (din_addr[(i-start_addr)])? 1<<((i-start_addr)%8):0; 
		}
		
	} else {
		start_addr -= RELAYOUT_STARTADDR;
		for(int i=start_addr; i<data_num; i++) {
			modbus_buffer[9+(i-start_addr)/8] |= (dout_addr[(i-start_addr)])?1<<((i-start_addr)%8):0; 
		}
	}
	modbus_buffer[5] = 3+modbus_buffer[8];

	return 6+modbus_buffer[5];
}

//0x03 读保持寄存器,电流输出值
unsigned int  StdModbusTcp::read_holding_registers(unsigned char (&modbus_buffer)[256])
{
	DEBUG(__func__);

	if(holding_register_channel_num == 0) {
		return -1;
	}
	unsigned char recv_buf[1024];
	unsigned char *p = recv_buf;
	int value = 0;
	int result = 0;
	int n = mcu.Read(recv_buf);
	int ain_num = *(p+3);
	int aout_num = *(p +2 + 2 + ain_num + 1);
	unsigned char *aout_addr = p +2 + 2 + ain_num + 2;

	int start_addr = MAKEWORD(modbus_buffer[8], modbus_buffer[9]);
	int data_num = MAKEWORD(modbus_buffer[10], modbus_buffer[11]);
	
	aout_addr += start_addr*4;

	modbus_buffer[8] = data_num*2;//字节个数 
	memset(modbus_buffer+9, 0, sizeof(modbus_buffer)-9);

	for(int i=0; i<data_num; i++) {
		value = aout_addr[0]<<24 | aout_addr[1]<<16 | aout_addr[2]<<8 | aout_addr[3]<<0;
		aout_addr += 4; 
		result = (int)(convert.ad_to_asr_channel((input_register_channel_num/8)*8 + start_addr, value, 0)*1000);
		modbus_buffer[9+(i+1)*2-2] = (unsigned char)(result>>8)&0xFF;
		modbus_buffer[9+(i+1)*2-1] = (unsigned char)(result)&0xFF;
	}

	modbus_buffer[5] = 3+modbus_buffer[8];

	return 9 + modbus_buffer[2];
}

//0x04 读输入寄存器，电流电压采集值
unsigned int StdModbusTcp::read_intput_registers(unsigned char (&modbus_buffer)[256])
{
	DEBUG(__func__);
	if(input_register_channel_num == 0) {
		return -1;
	}
	unsigned char recv_buf[1024];
	unsigned char *p = recv_buf;
	int value = 0;
	int result = 0;
	int n = mcu.Read(recv_buf);
	int ain_num = *(p+3);
	unsigned char *ain_addr = p + 2 + 2; 

	int start_addr = MAKEWORD(modbus_buffer[8], modbus_buffer[9]);
	int data_num = MAKEWORD(modbus_buffer[10], modbus_buffer[11]);

	ain_addr += start_addr*4;

	modbus_buffer[8] = data_num*2;//字节个数 
	memset(modbus_buffer+9, 0, sizeof(modbus_buffer)-9);

	for(int i=0; i<data_num; i++) {
		value = ain_addr[0]<<24 | ain_addr[1]<<16 | ain_addr[2]<<8 | ain_addr[3]<<0;
		ain_addr += 4; 
		result = (int)(convert.ad_to_asr_channel(start_addr+i, value, (convert.get_channel_sub_type(start_addr+i)-1?0:1))*1000);
		modbus_buffer[9+(i+1)*2-2] = (unsigned char)(result>>8)&0xFF;
		modbus_buffer[9+(i+1)*2-1] = (unsigned char)(result)&0xFF;
	}

	modbus_buffer[5] = 3+modbus_buffer[8];

	return 6+modbus_buffer[5];
}

//0x05　写单个线圈  写继电器状态
unsigned int  StdModbusTcp::force_single_coil(unsigned char (&modbus_buffer)[256], int len)
{
	DEBUG(__func__);

	if(relay_output_channel_num == 0) {
		return -1;
	}

	int data = MAKEWORD(modbus_buffer[10], modbus_buffer[11]) ;
	if(data != 0xFF00 && data!=0x0000) {
		return -1;
	}

	int start_addr = MAKEWORD(modbus_buffer[8], modbus_buffer[9])- RELAYOUT_STARTADDR ;
	unsigned char send_buf[1024] = {0x01,0x05};
	unsigned char recv_buf[1024] = {0x00};
	send_buf[2]= 0x04;  //控制类型，开关量
	send_buf[3]= start_addr + 1;	//通道编号

	send_buf[4]= 0x00;
	send_buf[5]= 0x00;
	send_buf[6]= 0x00; 
	send_buf[7]= data ? 1 : 0;

	unsigned short crc_calc = 0;
	crc_calc = crc( send_buf, 0, 10 - 2 );
	send_buf[8]=((unsigned)crc_calc >>8)&0xFF;
	send_buf[9]=((unsigned)crc_calc)&0xFF;
	int n = mcu.Write(send_buf, 10, recv_buf);
	if((n<0) || (0 != memcmp(send_buf, recv_buf, 10))) {
		FATAL(__func__ <<  " controle relay error");
		return -1;
	}


	return len; 
}

//0x06 写单个保持寄存器, 是流值
unsigned int  StdModbusTcp::preset_single_register(unsigned char (&modbus_buffer)[256], int len)
{
	DEBUG(__func__);

	if(switch_input_channel_num == 0) {
		return -1;
	}

	int data = MAKEWORD(modbus_buffer[10], modbus_buffer[11]);
	int start_addr = MAKEWORD(modbus_buffer[8], modbus_buffer[9]);
	unsigned char send_buf[1024] = {0x01,0x05};
	unsigned char recv_buf[1024] = {0x00};

	int channel_num = input_register_channel_num/8*8 + holding_register_channel_num/4*8 + switch_input_channel_num/8*8 + start_addr;
	data = convert.asr_to_ad_channel(channel_num/8+1,channel_num%8+1, data);
	send_buf[2]= 0x02;  //控制类型，模拟量
	send_buf[3]= start_addr+1;	//通道编号
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
		FATAL(__func__ <<  " output AIN value error.");
		return -1;
	}

	return len; 
}


unsigned int StdModbusTcp::crc(unsigned char *buf,int start,int cnt)
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
