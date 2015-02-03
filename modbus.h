#ifndef MODBUS_H
#define MODBUS_H
#include "CommunicateMcu.h"
#include "Convert.h"
#define MAKEWORD(a,b) (unsigned short)((a<<8)|b)
//初始化MCU通讯对象
class ModbusTcp{
public:
	ModbusTcp();
	~ModbusTcp();
	int process_modbus_data(unsigned char (&tcp_modbus_buf)[256]);

private:
	bool check_modbus_packet(unsigned char (&tcp_modbus_buf)[256]);
	bool check_protocol_identifier(unsigned char (&tcp_modbus_buf)[256]);
	bool check_length(unsigned char (&tcp_modbus_buf)[256]);
	bool check_uid(unsigned char (&tcp_modbus_buf)[256]);
	bool check_func_node(unsigned char (&tcp_modbus_buf)[256]);
	bool check_start_addr(unsigned char (&tcp_modbus_buf)[256]);
	bool check_data_num(unsigned char (&tcp_modbus_buf)[256]);
	unsigned int crc(unsigned char *buf,int start,int cnt);
	void strcopyx(char *desc, char *src);
	void get_device_info();
	int read_node_status(unsigned char (&tcp_modbus_buf)[256]);
	int read_register(unsigned char (&tcp_modbus_buf)[256]);
	bool relay_output(int sensor_id, int channel_id, int control_type, int data);
	int write_register(unsigned char (&tcp_modbus_buf)[256]);
private:
	CommunicateMcu mcu;
	Convert conver;
};

#endif
