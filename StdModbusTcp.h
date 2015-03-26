#ifndef __MODBUS_TCP_
#define __MODBUS_TCP_
#include "CommunicateMcu.h"
#include "Convert.h"
#define MAKEWORD(a,b) (unsigned short)((a<<8)|b)
#define	SWITCH_INPUT_STARTADDR		0x0000	//开关量输入起始地址
#define RELAYOUT_STARTADDR			0x1000	//继电器控制起始地址
#define	INPUT_REGISTER_STARTADDR	0X0000	//输入寄存器起始地址
#define	HOLDING_REGISTER_STARTADDR	0x0000	//保持寄存器起始地址

class StdModbusTcp {
public:
	StdModbusTcp();
	~StdModbusTcp();
	int process_modbus_data(unsigned char (&modbus_buffer)[256], int len);

private:
	bool check_modbus_device_status(unsigned char (&modbus_buffer)[256]);
	bool check_modbus_packet(unsigned char (&modbus_buffer)[256], int len);
	bool check_protocol_identifier(unsigned char (&tcp_modbus_buf)[256]);
	bool check_length(unsigned char (&tcp_modbus_buf)[256], int len);
	bool check_uid(unsigned char (&modbus_buffer)[256]);
	bool check_func_node(unsigned char (&modbus_buffer)[256]);
	bool check_start_addr(unsigned char (&modbus_buffer)[256]);
	bool check_data_num(unsigned char (&modbus_buffer)[256]);
	unsigned int read_coil_status(unsigned char (&modbus_buffer)[256]);
	unsigned int read_holding_registers(unsigned char (&modbus_buffer)[256]);
	unsigned int read_intput_registers(unsigned char (&modbus_buffer)[256]);
	unsigned int  force_single_coil(unsigned char (&modbus_buffer)[256], int len);
	unsigned int  preset_single_register(unsigned char (&modbus_buffer)[256], int len);
	unsigned int crc(unsigned char *buf,int start,int cnt);

private:
	CommunicateMcu mcu;
	Convert convert;
	int switch_input_channel_num;
	int relay_output_channel_num;
	int input_register_channel_num;
	int holding_register_channel_num;
};
#endif
