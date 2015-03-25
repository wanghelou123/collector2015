#include <iostream>
#include <errno.h>
#include <stdio.h>
#include "serial.h"
#include "Log.h"
#include "ModbusRtu.h"
using namespace std;

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

	//serial object
	serial myserial = serial("/dev/ttySAC1", 115200, 8, 'n', 1);


	//modbus RTU object
	ModbusRtu modbus_rtu;

	while(1) {
		//memset(buffer, 0, sizeof(buffer));
		FD_SET(myserial.fd, &rset);
		ret = select(myserial.fd+1, &rset, NULL, NULL, NULL);
		if(ret == 0) {
			NOTICE("select timeout!");
			return -1;
		}else if(ret == -1) {
				FATAL("select():"<< strerror(errno));	
				return -1;
		}
		if( FD_ISSET(myserial.fd, &rset)) {
			n = myserial.serialRead(buffer);
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
			n = myserial.serialWrite(p,n);
		}
	}

}
