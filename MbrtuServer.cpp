#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "sqlite3.h"
#include "serial.h"
#include "Log.h"
#include "ModbusRtu.h"
using namespace std;

#define DB_PATH "/etc/gateway/collector.db"
static serial *myserial;
static int slaveAddr;		//在MODBUS RTU中采集器的地址

static int init_serial(int nSerial)
{
	char serialName[30];
	int baud_i;			//波特率
	int dataBit;		//数据位
	char parity;		//奇偶校验
	int stopBit;		//停止位
	sqlite3 *db;
	sqlite3_stmt   *stmt;
	int rc;

	char sql[128]; 

	rc = sqlite3_open(DB_PATH, &db);
	/********************************************************************
	 *	读取串口参数
	 ********************************************************************/
	if (rc) {
		FATAL("Can't open database: " << sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}  
	sprintf(serialName, "/dev/ttySAC%d", nSerial);
	sprintf(sql, "SELECT baudrate, databit, checkbit, stopbit, slave_addr FROM serial where serialnum='COM%d'", nSerial);

	DEBUG(sql);

	rc = sqlite3_prepare(db, sql, (int)strlen(sql), &stmt, NULL); 
	if(rc != SQLITE_OK) { 
		FATAL("SQL error:" << sql << "->" << sqlite3_errmsg(db));
	}   
	rc = sqlite3_step(stmt);
	if(rc == SQLITE_ROW){
		baud_i = sqlite3_column_int(stmt, 0);		
		dataBit = sqlite3_column_int(stmt, 1);		
		int iparity = sqlite3_column_int(stmt, 2);
		switch (iparity){
			case 0:
				parity = 'n';
				break;
			case 1:
				parity = 'o';
				break;
			case 2:
				parity = 'e';
				break;
		}

		stopBit = sqlite3_column_int(stmt, 3);
		slaveAddr = sqlite3_column_int(stmt, 4);

		DEBUG("Read from serialConf.db, parameters is:\n"\
				<<"serial port: "<<serialName << "\n"\
				<<"baudrate: "<<baud_i << "\n"\
				<<"data bit: "<<dataBit << "\n"\
				<<"parity bit: "<<parity << "\n"\
				<<"stop bit: "<<stopBit << "\n"\
				<<"slave addr: "<<slaveAddr);

	}else{
		FATAL("read serial  configs error");
		return -1;
	} 
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	myserial = new serial(serialName, baud_i, dataBit, parity, stopBit);
}

int main(int argc, char*argv[])
{
	
	int n = 0, ret = 0;
	unsigned char buffer[256];
	unsigned char *p = buffer;
	fd_set rset;
	FD_ZERO(&rset);

	// 打开日志  
	if (!Log::instance().open_log(argv[0]))  
	{  
		std::cout << "Log::open_log() failed" << std::endl;  
		exit(-1);
	} 

	if(argc != 2) {
		printf("Usage: MbrtuServer <serial_number>\n");
		exit(-1);
	}else {
		if(atoi(argv[1]) != 1 && (atoi(argv[1]) != 2) && (atoi(argv[1]) != 3)) {
			printf("serial_number must be 1 or 2 !\n");
			exit(-1);
		}
	}

	//serial object
	init_serial(atoi(argv[1]));


	//modbus RTU object
	ModbusRtu modbus_rtu(slaveAddr);

	while(1) {
		//memset(buffer, 0, sizeof(buffer));
		FD_SET(myserial->fd, &rset);
		ret = select(myserial->fd+1, &rset, NULL, NULL, NULL);
		if(ret == 0) {
			NOTICE("select timeout!");
			return -1;
		}else if(ret == -1) {
				FATAL("select():"<< strerror(errno));	
				return -1;
		}
		if( FD_ISSET(myserial->fd, &rset)) {
			n = myserial->serialRead(buffer);
			if(n <= 0)
				continue;

			//char tmpbuffer[1024];
			//memset(tmpbuffer, 0, sizeof(tmpbuffer));
			for(int i=0; i<n; i++) {
				//snprintf(tmpbuffer+i*3, sizeof(tmpbuffer), "%.2x ", (char)buffer[i]);
				printf("%.2x ", (char)p[i]);
			}printf("\n");
			//DEBUG("recv request modbus packet:"<< tmpbuffer)
			n = modbus_rtu.process_modbus_data(buffer, n);
			if(n<0)
				continue;
			n = myserial->serialWrite(p,n);
		}
	}

}
