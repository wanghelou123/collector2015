#ifndef MODBUS_H
#define MODBUS_H
#include "commMcu.h"
#define MAKEWORD(a,b) (unsigned short)((a<<8)|b)
//初始化MCU通讯对象
int process_modbus_data(unsigned char (&tcp_modbus_buf)[256]);
bool check_modbus_packet(unsigned char (&tcp_modbus_buf)[256]);
bool check_protocol_identifier(unsigned char (&tcp_modbus_buf)[256]);
bool check_length(unsigned char (&tcp_modbus_buf)[256]);
bool check_uid(unsigned char (&tcp_modbus_buf)[256]);
bool check_func_node(unsigned char (&tcp_modbus_buf)[256]);
bool check_start_addr(unsigned char (&tcp_modbus_buf)[256]);
bool check_data_num(unsigned char (&tcp_modbus_buf)[256]);
int read_node_status(unsigned char (&tcp_modbus_buf)[256]);
int read_register(unsigned char (&tcp_modbus_buf)[256]);
int write_register(unsigned char (&tcp_modbus_buf)[256]);

#endif
