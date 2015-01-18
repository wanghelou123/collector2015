#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <strings.h>
#include <string.h>
#include "serial.h"
#include <iostream>
#include <iomanip>

using namespace std;

	serial::serial(char *devName, int _baudRate, int _dataBit, char _parity, int _stopBit)
: baud_i(_baudRate), dataBit(_dataBit), parity(_parity),stopBit(_stopBit)
{
	sscanf(devName+strlen(devName)-1, "%d", &nRS485);
	nRS485 = nRS485 - 1;
	cout << "Serial parameters: " << devName << " " << nRS485 << " "\
		<< baud_i << " " <<dataBit << " " <<parity<< " " << stopBit <<endl;

	strcpy(serialName, devName);
	serialOpen();
	serialInit();
}



serial::~serial()
{
	cout << __func__<<endl;
	close(fd);
}

int serial::serialOpen()
{
	cout << __func__ << " " << "serialNmae "<< serialName<<endl;
	if((fd=open(serialName, O_RDWR | O_NOCTTY | O_NDELAY)) == -1){
		perror("Open serial port error.\n");
		return -1; 
	}  
	if(fcntl(fd, F_SETFL, 0) < 0){ 
		perror("fcntl error.\n");
		return -1; 
	}  

	return 0;
}

int serial::serialWrite(unsigned char *&buf, int size)
{
	int n;
	n = write(fd, (void *)buf, size);
#if 0
	if(tcdrain(fd) < 0){
		perror("tcdrain:");	
	}
#endif

	return n;
}

int  serial::serialRead(unsigned char *received_string)
{
	int rxchar = PORT_FAILURE;
	int data_avail = FALSE;
	int bytes_received = 0;
	int read_stat;

	int timeout = 86400;			/* 1 day */

	fd_set rfds;

	struct timeval tv;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	FD_ZERO( &rfds );
	FD_SET( fd, &rfds );


	/* wait for a response */
	data_avail = select( FD_SETSIZE, &rfds, NULL, NULL, &tv );

	if( !data_avail )
	{
		bytes_received = 0; 
		fprintf( stdout, "Comms time out\n" );
		return (-1);
	}



	while( data_avail )
	{

		/* if no character at the buffer wait char_interval_timeout */
		/* before accepting end of response			    */
		tv.tv_sec = 0;
		tv.tv_usec = char_interval_timeout;
		FD_ZERO( &rfds );
		FD_SET( fd, &rfds );

		if( select( FD_SETSIZE, &rfds, NULL, NULL, &tv ) )
		{
			read_stat = read( fd, &rxchar, 1 );

			if( read_stat < 0 )
			{
				bytes_received = -11;
				data_avail = FALSE;
			}
			else
			{
				rxchar = rxchar & 0xFF;
				received_string[ bytes_received ++ ] = rxchar;
				//printf("%.2x ", rxchar);
			}


			if( bytes_received >= MAX_DATA_LENGTH )
			{
				printf("Read from serial bytes is %d\n", bytes_received);
				bytes_received = -12;
				data_avail = FALSE;
			}

		}

		else
		{
			data_avail = FALSE;
		}

	}	
	return( bytes_received );
}



int serial::serialInit()
{
	struct termios newtio, oldtio;
	bzero(&newtio, sizeof(newtio));

	if(tcgetattr(fd, &oldtio) != 0){ 
		perror("tcgetattr error \n");
		return -1;
	}


	/*设置输入输出波特率*/
	speed_t baud_rate;
	switch( baud_i )
	{
		case 110:
			baud_rate = B110;
			char_interval_timeout = TO_B110;
			break;
		case 300:
			baud_rate = B300;
			char_interval_timeout = TO_B300;
			break;
		case 600:
			baud_rate = B600;
			char_interval_timeout = TO_B600;
			break;
		case 1200:
			baud_rate = B1200;
			char_interval_timeout = TO_B1200;
			break;
		case 2400:
			baud_rate = B2400;
			char_interval_timeout = TO_B2400;
			break;
		case 4800:
			baud_rate = B4800;
			char_interval_timeout = TO_B4800;
			break;
		case 9600: case 0:
			baud_rate = B9600;
			char_interval_timeout = TO_B9600;
			break;
		case 19200:
			baud_rate = B19200;
			char_interval_timeout = TO_B19200;
			break;
		case 38400:
			baud_rate = B38400;
			char_interval_timeout = TO_B38400;
			break;
		case 57600:
			baud_rate = B57600;
			char_interval_timeout = TO_B57600;
			break;
		case 115200:
			baud_rate = B115200;
			char_interval_timeout = TO_B115200;
			break;
		default:
			baud_rate = B115200;
			char_interval_timeout = TO_B115200;
	}
	cfsetispeed(&newtio, baud_rate); 	//设置输入波特率
	cfsetospeed(&newtio, baud_rate); 	//设置输出波特率


	newtio.c_cflag |= CLOCAL | CREAD;	//CLOCAL,忽略调制解调器状态,CREAD,接收装置被启用，可以接收字符

	//设置数据位，5, 6, 7，8
	newtio.c_cflag &= ~CSIZE;
	switch (dataBit) {
		case 5:
			newtio.c_cflag |= CS5;
			break;
		case 6:
			newtio.c_cflag |= CS6;
			break;
		case 7:
			newtio.c_cflag |= CS7;
			break;
		case 8:
			newtio.c_cflag |= CS8;
			break;
		default:
			printf("Unsupported Data_bits\n");
			newtio.c_cflag |= CS8;//defalut,CS8,数据位为8位
	}

	/*设置校验位*/
	switch (parity) {
		default:
		case 'N':
		case 'n': 
			newtio.c_cflag &= ~PARENB;
			newtio.c_iflag &= ~INPCK;
			break;
			/*odd:奇校验*/
		case 'o':
		case 'O': 
			newtio.c_cflag |= (PARODD | PARENB);
			newtio.c_iflag |= INPCK;
			break;
			/*even:偶校验*/
		case 'e':
		case 'E': 
			newtio.c_cflag |= PARENB;
			newtio.c_cflag &= ~PARODD;
			newtio.c_iflag |= INPCK;
			break;

		case 's':
		case 'S': 
			newtio.c_cflag &= ~PARENB;
			newtio.c_cflag &= ~CSTOPB;
			break;
	}


	/*设置停止位, 值为1 or 2*/
	newtio.c_cflag |= CSTOPB;			
	switch (stopBit) {
		case 1:
			newtio.c_cflag &= ~CSTOPB;//1, stop bit
			break;
		case 2:
			newtio.c_cflag |= CSTOPB;//2, stop bits
			break;
		default:
			printf("Unsupported stopBit.\n");
			newtio.c_cflag &= ~CSTOPB;//1, stop bit

	}

	newtio.c_cc[VTIME] = 100;			//unix环境高级编程，p532
	newtio.c_cc[VMIN] = 49;
	//newtio.c_cc[VTIME] = 0;			//unix环境高级编程，p532
	//newtio.c_cc[VMIN] = 0;
	tcflush(fd, TCIOFLUSH);             //刷清输入输出队列
	if((tcsetattr(fd,TCSANOW,&newtio)) != 0){	//设置终端属性
		perror("tcsetattr error \n");
		return -1;
	}

	return 0;
}
int serial::serialClose()
{
	close(fd);
}

int serial::serialRestart()
{
	cout << __func__<<endl;
	serialClose();	
	serialOpen();
	serialInit();
}
