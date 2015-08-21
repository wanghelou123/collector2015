#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
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
#define MAXLINE 129
#define LISTENQ 5
#define	CLI_NUM	5
#define	EVENT_NUM 20
static serial *myserial;
static int slaveAddr;		//在MODBUS RTU中采集器的地址
static int serial_mode;//串口的模式collector or tcp to rs485
static int tcp_port;//如果是tcp到rs485透传模式时，TCP端口
static int tcp_timeout;

void show_prog_info()
{
	cout << "\ncollector2015 Software, Inc."<<endl;
	cout << "Program name MbrtuServer run with hardware v1." << endl;
	cout << "Compiled on " << __DATE__ << " at "<< __TIME__ <<endl;
}
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
	sprintf(sql, "SELECT baudrate, databit, checkbit, stopbit, slave_addr, trans_flag, trans_port\
					FROM serial where serialnum='COM%d'", nSerial);

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
		serial_mode = sqlite3_column_int(stmt, 5);
		tcp_port = sqlite3_column_int(stmt, 6);

		DEBUG("Read from serialConf.db, parameters is:\n"\
				<<"serial port: "<<serialName << "\n"\
				<<"baudrate: "<<baud_i << "\n"\
				<<"data bit: "<<dataBit << "\n"\
				<<"parity bit: "<<parity << "\n"\
				<<"stop bit: "<<stopBit << "\n"\
				<<"slave addr: "<<slaveAddr<< "\n" \
				<<"serial_mode: "<<serial_mode << "\n" \
				<<"slave addr: "<<slaveAddr<<endl);

	}else{
		FATAL("read serial  configs error");
		return -1;
	} 
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	myserial = new serial(serialName, baud_i, dataBit, parity, stopBit);
}

void clean_socket_buf(int skt) 
{
	struct timeval tmOut;
	tmOut.tv_sec = 0;
	tmOut.tv_usec = 0;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(skt, &fds);
	int  nRet;
	char tmp[2];
	int count=0;
	int n=0;
	memset(tmp, 0, sizeof(tmp));
	while(1)
	{          
		nRet= select(FD_SETSIZE, &fds, NULL, NULL, &tmOut);
		if(nRet== 0)  
			break;
		if(nRet<0){
			FATAL("select():"<< strerror(errno));	
			break;
		}
		n=recv(skt, tmp, 1,0);
		if(n>0){
			count+=n;
			if((count%(0x100000))==0)
				printf("clean bytes %d\n", count);	

		}else if(n==0){
			NOTICE("recv(): the peer has  performed  an  orderly shutdown.");
			break;
		}else if(n<0){
			FATAL("recv():an error occurred."<<strerror(errno));
			break;
		}
	}
	DEBUG("clean the socket buff ok, total " << count <<" bytes");
}

//串口用于与上位机通讯
void communite_mode()
{
	int n = 0, ret = 0;
	unsigned char buffer[256];
	unsigned char *p = buffer;
	fd_set rset;
	FD_ZERO(&rset);

	//modbus RTU object
	ModbusRtu modbus_rtu(slaveAddr);

	while(1) {
		//memset(buffer, 0, sizeof(buffer));
		FD_SET(myserial->fd, &rset);
		ret = select(myserial->fd+1, &rset, NULL, NULL, NULL);
		if(ret == 0) {
			NOTICE("select timeout!");
			continue;
		}else if(ret == -1) {
				FATAL("select():"<< strerror(errno));	
				continue;
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


//串口用于TCP到RS485透传
void collector_mode()
{
	int i, listenfd, connfd, sockfd,epfd,nfds;
	ssize_t n;
	unsigned char line[MAXLINE];
	socklen_t clilen;
	static int clifd_array[CLI_NUM]={0};
	static int send_bytes=0;

	tcp_timeout = 30;/*设置TCP超时时间为30分钟*/

	//声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件
	struct epoll_event ev, events[EVENT_NUM];

	//生成用于处理accept的epoll专用的文件描述符
	epfd=epoll_create(256);
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
	clilen = sizeof(struct sockaddr_in);
	listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port=htons(tcp_port);
	int option=1;
	if(setsockopt(listenfd, SOL_SOCKET,SO_REUSEADDR, &option, sizeof(int) ) < 0){
		perror("setsockopt ");
		exit(-1);
	}

	if( bind(listenfd,(sockaddr *)&serveraddr, sizeof(serveraddr)) ){
		perror("bind");
		exit(-1);
	}

	if( listen(listenfd, 5) < 0){
		FATAL("listen error");
		exit(-1);
	}

	//设置与要处理的事件相关的文件描述符
	ev.data.fd=listenfd;
	//设置要处理的事件类型
	//ev.events=EPOLLIN|EPOLLET;
	ev.events=EPOLLIN;
	//注册epoll事件
	epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);

	ev.data.fd=myserial->fd;
	//ev.events=EPOLLIN|EPOLLET;
	ev.events=EPOLLIN;
	epoll_ctl(epfd,EPOLL_CTL_ADD, myserial->fd, &ev);

	for ( ; ; ) {
		//等待epoll事件的发生,发生的事件放在events数据中
		nfds=epoll_wait(epfd,events,EVENT_NUM,tcp_timeout*60*1000);

		//cout << "alread to read file descriptor number is: " << nfds <<endl;
		if(0 == nfds){ //超时后断开所有tcp连接
			cout << "epollwait() time out." << endl;
			int j=0;
			for(j=0; j<CLI_NUM; j++){
				DEBUG("clifd_array["<<j<<"] = "<<  clifd_array[j]);
				if( 0 != clifd_array[j]){
					struct epoll_event ev_del;
					if (epoll_ctl(epfd, EPOLL_CTL_DEL, clifd_array[j], &ev_del) < 0){
						fprintf(stderr, "epoll_ctl delete fd error");
					}
					if(shutdown(clifd_array[j],SHUT_RDWR)<0){
						FATAL("shutdown "<<strerror(errno));
					}
					if(close(clifd_array[j])<0){
						FATAL("close "<<strerror(errno));
					}
					DEBUG("close socket.");
					clifd_array[j]=0;
				}
			}
			continue;	
		}
		//处理所发生的所有事件
		for(i=0;i<nfds;++i)
		{
			/*处理串口输入上的数据*/
			if(events[i].data.fd==myserial->fd)
			{ 
				if ( (sockfd = events[i].data.fd) < 0) {
					continue;
				}

				if ( (n = myserial->serialRead(line)) < 0) {
					FATAL("read data from serial port error,"<<n);
					continue;
				} 

				int j=0;
				for(j=0;j<CLI_NUM; j++) {
					if(clifd_array[j] != 0){
						if((send_bytes = send(clifd_array[j], line, n, MSG_NOSIGNAL))<0){//写socket出错后关闭socket
							perror("send");
							FATAL("send "<<strerror(errno)<< ",will close socket.");
							struct epoll_event ev_del;
							if (epoll_ctl(epfd, EPOLL_CTL_DEL, clifd_array[j], &ev_del) < 0){
								perror("epoll_ctl");
								FATAL("epoll_ctl delete fd error\n");
							}
							if(shutdown(clifd_array[j],SHUT_RDWR)<0){
								FATAL("shutdown "<<strerror(errno));
							}
							if(close(clifd_array[j])<0){
								perror("close");
								FATAL("close error "<<strerror(errno));	
							}
							clifd_array[j] = 0;
							events[i].data.fd = -1;
						}else{
							//DEBUG("send "<< send_bytes<<" bytes to socket success!");	
						}
					}
				}
#if 0	
				printf("recv from serial  num: %d\n", n);
				int ii=0;
				for(ii=0; ii<n; ii++){
					printf("%.2x ", line[ii]);
				}
				printf("\n");
#endif

			}/*处理sock新客户连接*/
		}
		for(i=0;i<nfds;++i)
		{
			if(events[i].data.fd==listenfd)//如果新监测到一个SOCKET用户连接到了绑定的SOCKET端口，建立新的连接。
			{
				connfd = accept(listenfd,(sockaddr *)&clientaddr, &clilen);
				if(connfd<0){
					perror("connfd<0");
					continue;
				}

				int j=0;
				for(j=0; j<CLI_NUM; j++){
					DEBUG("clifd_array["<<j<<"] = "<<  clifd_array[j]);
					if( 0 == clifd_array[j]){
						clifd_array[j]=connfd;
						break;
					}
				}
				if(5==j){
					if(shutdown(connfd,SHUT_RDWR)<0){
						FATAL("shutdown "<<strerror(errno));
					}
					if(close(connfd)<0){
						FATAL("close "<<strerror(errno));
					}
					NOTICE("The max connet socket number is 5!!!");
					continue;
				}

				char *str = inet_ntoa(clientaddr.sin_addr);
				cout << "accapt a connection from " << str << endl;

				//设置用于读操作的文件描述符
				ev.data.fd=connfd;
				//设置用于注测的读操作事件
				//ev.events=EPOLLIN|EPOLLET;
				ev.events=EPOLLIN;
				//注册ev
				epoll_ctl(epfd,EPOLL_CTL_ADD,connfd,&ev);

			}/*如果是已经连接的用户，并且收到数据，那么进行读入*/
			else if(events[i].events&EPOLLIN)
			{
				/*如果是串口则跳过，因为上边的循环中已经处理的串口*/
				if(events[i].data.fd==myserial->fd)
				{ 
					continue;
				}
				//cout << "EPOLLIN" << endl;
				if ( (sockfd = events[i].data.fd) < 0) {
					continue;
				}

				//cout << "already to read sockfd = " << sockfd << endl;

				if ( (n = recv(sockfd, line, MAXLINE, 0)) <= 0) {
					DEBUG("recv return value is: " << n);
					if(0 == n){
						DEBUG("the peer has performed an orderly shutdown.\n");
					}else{
						DEBUG("recv from sockfd error ECONNRESET. will close the socket\n");
					}
					int j=0;
					for(j=0; j<CLI_NUM; j++){
						if( sockfd == clifd_array[j]){
							clifd_array[j] = 0;
							break;
						}
					}
					struct epoll_event ev_del;
					if (epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, &ev_del) < 0){
						FATAL("epoll_ctl delete fd error");
					}

					if(shutdown(sockfd,SHUT_RDWR)<0){
						FATAL("shutdown "<<strerror(errno));
					}
					if(close(sockfd)<0){
						FATAL("close "<<strerror(errno));
					}
					DEBUG("close socket.");
					events[i].data.fd = -1;
				}else{
					DEBUG("recv bytes from socket is: "<< n);
					if(n>128){
						clean_socket_buf(sockfd);
						continue;
					}

					/*将从socket上接收到的数据写到串口上边*/
					unsigned char *p = NULL;
					p = line;
					if(n != myserial->serialWrite(p, n) ){
						FATAL("write to serial port error");
					}
					usleep(10000);
#if 0
					printf("write to serial num %d\n:",n);
					int i=0;
					for(i=0;i<n;i++){
						printf("%.2x ", p[i]);
					}
					printf("\n");
#endif
				}

			}
			else if(events[i].events&EPOLLOUT) // 如果有数据发送
			{
				cout << "write data" <<endl;
				sockfd = events[i].data.fd;
				write(sockfd, line, n);
				//设置用于读操作的文件描述符

				ev.data.fd=sockfd;
				//设置用于注测的读操作事件

				//ev.events=EPOLLIN|EPOLLET;
				ev.events=EPOLLIN;
				//修改sockfd上要处理的事件为EPOLIN

				epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&ev);
			}
		}
	}
	
}

int main(int argc, char*argv[])
{
	
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

	if(serial_mode == 0) {
		DEBUG("communicate mode.");
		communite_mode(); // modbus rtu	
	}else {
		DEBUG("tcp to serial mode.");
		collector_mode();//tcp to rs485	
	}

}
