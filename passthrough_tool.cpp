#include <iostream>
#include <list>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <signal.h>
#include "serial.h"
#include <pthread.h>
#include "sqlite3.h"
#include "Log.h"

using namespace std;

#define DB_PATH		"/etc/gateway/serialConf.db"
#define MAXLINE 256
#define LISTENQ 5
#define	CLI_NUM	5
#define	EVENT_NUM 20
#define COLLECT_INTERVAL 1  //数据采集间隔，单位：秒

serial *myserial;
static int clifd_array[CLI_NUM]={0};
static int passthrough = 0;//0:默认为透传模式, 1:MODBUS TCP 转MODBUS RTU
static int tcp_port;
static int tcp_timeout;

struct mcu_data{
	unsigned char buf[1024];
	int len;
} mcu_data_g;
static int adc_type;
static int unit_info[6];
void show_prog_info()
{
	cout << "\ncollector2014 Software, Inc."<<endl;
	cout << "Program name passthrough_tool run with hardware v1." << endl;
	cout << "Compiled on " << __DATE__ << " at "<< __TIME__ <<endl;
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
void setnonblocking(int sock)
{
	int opts;
	opts=fcntl(sock,F_GETFL);
	if(opts<0)
	{
		perror("fcntl(sock,GETFL)");
		exit(1);
	}
	opts = opts|O_NONBLOCK;
	if(fcntl(sock,F_SETFL,opts)<0)
	{
		perror("fcntl(sock,SETFL,opts)");
		exit(1);
	}
}
int init_unit_info()
{
	DEBUG(__func__);
	sqlite3 *db;
	sqlite3_stmt   *stmt;
	int rc;

	char sql[128]; 

	rc = sqlite3_open("/etc/gateway/collector.db", &db);
	if (rc) {
		FATAL("Can't open database: " << sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}  
	sprintf(sql, "select name, flag from flags;");
	rc = sqlite3_prepare(db, sql, (int)strlen(sql), &stmt, NULL); 
	if(rc != SQLITE_OK) { 
		FATAL("SQL error:" << sql << "->" << sqlite3_errmsg(db));
	}   
	int i=0;
	while(sqlite3_step(stmt) == SQLITE_ROW){
		if(!strncmp("adc_type", (const char*)sqlite3_column_text(stmt, 0), strlen("adc_type"))) {
			adc_type = sqlite3_column_int(stmt, 1);
			DEBUG(sqlite3_column_text(stmt, 0) <<" "<< sqlite3_column_int(stmt, 1));
		}else if(!strncmp("unit", (const char*)sqlite3_column_text(stmt, 0), strlen("unit"))) {
			unit_info[i++] = sqlite3_column_int(stmt, 1);
			DEBUG(sqlite3_column_text(stmt, 0) <<" "<< sqlite3_column_int(stmt, 1));
		}

	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

int init_serial(int nSerial)
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
	sprintf(sql, "SELECT transmode, baudrate, databit, checkbit, stopbit, tcpPort, timeout FROM serial where serialnum='COM%d'", nSerial);
	//DEBUG(sql);
	rc = sqlite3_prepare(db, sql, (int)strlen(sql), &stmt, NULL); 
	if(rc != SQLITE_OK) { 
		FATAL("SQL error:" << sql << "->" << sqlite3_errmsg(db));
	}   
	rc = sqlite3_step(stmt);
	if(rc == SQLITE_ROW){
		passthrough = sqlite3_column_int(stmt, 0);	
		baud_i = sqlite3_column_int(stmt, 1);		
		dataBit = sqlite3_column_int(stmt, 2);		
		int iparity = sqlite3_column_int(stmt, 3);
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

		stopBit = sqlite3_column_int(stmt, 4);
		tcp_port = sqlite3_column_int(stmt, 5);
		tcp_timeout = sqlite3_column_int(stmt, 6);
		if(tcp_timeout <0 || tcp_timeout>60){
			tcp_timeout=20;
		}

		DEBUG("Read from serialConf.db, parameters is:\n\t"\
				<<"serial port:\t"<<serialName << "\n\t"\
				<<"baudrate:\t"<<baud_i << "\n\t"\
				<<"data bit:\t"<<dataBit << "\n\t"\
				<<"parity bit:\t"<<parity << "\n\t"\
				<<"stop bit:\t"<<stopBit << "\n\t"\
				<<"tcp port:\t"<<tcp_port<<"\n\t"\
				<<"tcp timeout:\t"<<tcp_timeout);

	}else{
		FATAL("read serial  configs error");
		return -1;
	} 
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	myserial = new serial(serialName, baud_i, dataBit, parity, stopBit);
	DEBUG("myserial->fd = " << myserial->fd);
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

void test_list()
{
	list<int> ilist;
	for(int i=0; i<=10; i++)
		ilist.push_back(i);
	cout <<"ilist.size " << ilist.size()<<endl;;
	ilist.remove(5);
	cout <<"ilist.size " << ilist.size() << endl;;

	exit(1);
}


int init_server_socket()
{
	int listenfd;
	struct sockaddr_in serveraddr;
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

	if( listen(listenfd, 2) < 0){
		FATAL("listen error");
		exit(-1);
	}

	return listenfd;
}

//关闭所有客户端socket
void close_all_clifd(int epfd, int * clifd_array )
{
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

}

//处理串口上的数据
void process_serial_data()
{
	/*adc_falg:	 01--1549; 02--8320; 03--1247
	 *unit_flag: 01--AIN;  02--AOUT; 03--DIN; 04--DOUT
	 */
	unsigned char line[MAXLINE];
	//MCU启动后一直主动上报的数据包
	unsigned char mcu_recv_buf[7]={0x01, 0x60, 0x02, 0xA5, 0xA5, 0x1D, 0x2B};
	unsigned char mcu_send_buf[12]={0x01, 0x06, 0x07/*,adc_flag, unit1_flag, unit2_flag, unit3_flag,unit4_flag, unit5_flag,unit6_flag,crc_L, crc_H*/};

	int n=0;
	if ( (n = myserial->serialRead(line)) < 0) {
		FATAL("read data from serial port error,"<<n);
		return;
	} 

	int ii=0;
	char tmp[1024]={0};
	char *p=tmp;
	for(ii=0; ii<n; ii++){
		sprintf(p, "%.2x ", line[ii]);
		p+=3;
	}
	DEBUG("recv from serial  num: " << n<< " " << tmp);
	unsigned short crc_calc = 0;
	mcu_send_buf[3]=adc_type;
	mcu_send_buf[4]=unit_info[0];
	mcu_send_buf[5]=unit_info[1];
	mcu_send_buf[6]=unit_info[2];
	mcu_send_buf[7]=unit_info[3];
	mcu_send_buf[8]=unit_info[4];
	mcu_send_buf[9]=unit_info[5];
	crc_calc = crc( mcu_send_buf, 0, 12 - 2 );
	mcu_send_buf[10]=((unsigned)crc_calc >>8)&0xFF;
	mcu_send_buf[11]=((unsigned)crc_calc)&0xFF;

	unsigned char *p_send = mcu_send_buf;
	if(12 != myserial->serialWrite(p_send, 12) ){
		FATAL("write config information to serial port error");
	}
	printf("config information: \n");
	for(ii=0; ii<12; ii++){
		printf("%.2x ", p_send[ii]);
	}printf("\n");

	//下发配置命令后的响应
	if ( (n = myserial->serialRead(line)) < 0) {
		FATAL("read data from serial port error,"<<n);
	}
	printf("config info repot: \n");
	for(ii=0; ii<n; ii++){
		printf("%.2x ", line[ii]);
	}printf("\n");

	if( 0 == memcmp(mcu_send_buf, line, 12) ){
		NOTICE("config mcu sucess!");	
	}else{
		NOTICE("config mcu failure!");	
	}
}

//定时下发采集命令
void process_pipe_data(int fd)
{
	unsigned char buf[1024];	
	unsigned char line[MAXLINE];
	unsigned char *p = buf;
	int n;
	n = read(fd, buf, 8);
	DEBUG("read "<< n << " bytes from pipe.");
	if(n != myserial->serialWrite(p, n)){
		FATAL("write collect cmd to mcu failure!");
	}
	struct timeval tmOut;
	tmOut.tv_sec = 2;
	tmOut.tv_usec = 0;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(myserial->fd, &fds);
	int  nRet;

	nRet= select(FD_SETSIZE, &fds, NULL, NULL, &tmOut);
	if(nRet== 0){
		WARNING("select() read from serial timeout!");
		return ;
	}else if(nRet<0){
		FATAL("select error:"<< strerror(errno));	
		return ;
	}
	if ( (n = myserial->serialRead(line)) < 0) {
		FATAL("read data from serial port error,"<<n);
		return ;
	}
	DEBUG("recv from serial bytes is:"<< n);
	if(n == 7){
		DEBUG( __func__ << " read from serial bytes is "<< n << " will config the mcu.");
		process_serial_data(); 	
		return;
	}
	mcu_data_g.len = n;
	memcpy(mcu_data_g.buf, line, n);
}

//处理新的客户端
void process_new_client(int epfd, int listenfd)
{
	struct sockaddr_in clientaddr;
	socklen_t clilen = sizeof(struct sockaddr_in);

	int connfd = accept(listenfd,(sockaddr *)&clientaddr, &clilen);
	if(connfd<0){
		FATAL("accept "<<strerror(errno));
		return;
	}

	int j=0;
	for(j=0; j<CLI_NUM; j++){
		DEBUG("clifd_array["<<j<<"] = "<<  clifd_array[j]);
		if( 0 == clifd_array[j]){
			clifd_array[j]=connfd;
			break;
		}
	}
	if(CLI_NUM==j){
		if(shutdown(connfd,SHUT_RDWR)<0){
			FATAL("shutdown "<<strerror(errno));
		}
		if(close(connfd)<0){
			FATAL("close "<<strerror(errno));
		}
		WARNING("The max connet socket number is "<< CLI_NUM);
		return;
	}

	//char *str = inet_ntoa(clientaddr.sin_addr);
	//DEBUG("accapt a connection from " << str <<":"<< ntohs(clientaddr.sin_port));
	struct sockaddr_in peeraddr;
	socklen_t peeraddrlen;
	if(getpeername(connfd, (struct sockaddr*)&peeraddr, &peeraddrlen) == -1){
		FATAL("getpeername error:"<<strerror(errno));
	}
	DEBUG("accapt a connection from " <<  inet_ntoa(peeraddr.sin_addr) << ":" << ntohs(peeraddr.sin_port) );

	struct epoll_event ev;
	ev.data.fd=connfd;
	ev.events=EPOLLIN;
	if(-1 == epoll_ctl(epfd,EPOLL_CTL_ADD,connfd,&ev)){
		FATAL("epoll_ctl error, "<< strerror(errno));	
	}
}

//处理客户socket上的数据
int  process_clisocket_data(int epfd, int sockfd)
{

	int ret = 0, n=0;
	unsigned char line[MAXLINE];
	if ( (n = recv(sockfd, line, MAXLINE, 0)) <= 0) {
		if(0 == n){
			DEBUG("recv return 0, the peer has performed an orderly shutdown.\n");
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
		ret = -1;
		goto err;
	}else{
		DEBUG("recv bytes from socket is: "<< n);
		if(n>128){
			clean_socket_buf(sockfd);
			return 0;
		}
		if(line[1] == 0x04){//采集指令
			DEBUG("0x04 collect cmd.");
			char buf[] = {0x01, 0x04, 0x00, 0x00, 0xFF, 0xFF, 0xF1, 0xBA};
			if( 0 != memcmp(buf, line, sizeof(buf))){
				FATAL("collect cmd error!");
				goto err;
			}
			n = mcu_data_g.len;
			memcpy(line, mcu_data_g.buf, n);
		}else if(line[1] == 0x5){//控制指令
			DEBUG("0x05 control cmd.");
			/*将从socket上接收到的数据写到串口上边*/
			unsigned char *p_line = line;
			if(n != myserial->serialWrite(p_line, n) ){
				FATAL("write to serial port error");
			}

			char tmp[1024]={0};
			char *p=tmp;
			int i=0;
			for(i=0;i<n;i++){
				sprintf(p, "%.2x ", p_line[i]);
				p += 3;
			}
			DEBUG("write "<< n << " bytes to serial: " << tmp);

			struct timeval tmOut;
			tmOut.tv_sec = 2;
			tmOut.tv_usec = 0;
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(myserial->fd, &fds);
			int  nRet;

			nRet= select(FD_SETSIZE, &fds, NULL, NULL, &tmOut);
			if(nRet== 0){
				WARNING("select() read from serial timeout!");
				return 0;
			}else if(nRet<0){
				FATAL("select error:"<< strerror(errno));	
				return 0;
			}
			if ( (n = myserial->serialRead(line)) < 0) {
				FATAL("read data from serial port error,"<<n);
				return 0;
			} 
		}else{//非法指令
			FATAL("recv invalid cmd from socket!");	
			goto err;
		}


		if(send(sockfd, line, n, MSG_NOSIGNAL)<0){//写socket出错后关闭socket
			FATAL("send "<<strerror(errno)<< ",will close socket.");
			ret = -2;
			goto err;
		}
	}

err:
	if(ret<0){
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
	}

	return ret;
}

//线程处理函数，定时往管道里写采集指令
void *collect_data(void *arg)
{
	DEBUG("PIPE write fd is "<< *(int*)arg);
	char send_buf[] = {0x01, 0x04, 0x00, 0x00, 0xFF, 0xFF, 0xF1, 0xBA};
	int n;
	while(1){
		n = write(*(int*)arg, send_buf, sizeof(send_buf));
		if(n != sizeof(send_buf)){
			FATAL("write collect cmd to mcu error:"<< strerror(errno));
		}
		::sleep(COLLECT_INTERVAL);
	}

}

int main(int argc, char* argv[])
{
	int i, listenfd,epfd,nfds;
	int pipe_fd[2];
	ssize_t n;
	unsigned char line[MAXLINE]; 
	if ( 2 != argc )
	{
		fprintf(stderr,"Usage:%s number[1-3]\n",argv[0]);
		exit(-1);
	}

	//显示程序版本信息
	show_prog_info();

	//test_list();

	int log_level = 3;//default WARN_LOG_LEVEL
	if(argc == 2) {
		log_level = atoi(argv[1]);	
	}
	printf("===>log_level = %d\n", log_level);
	//打开日志  
	if (!Log::instance().open_log(log_level, argv[0]))  
	{  
		std::cout << "Log::open_log() failed" << std::endl;  
		exit(-1);
	} 

	//初始化每个单元的类型 
	init_unit_info();

	//初始化串口
	//init_serial( atoi(argv[1]) );
	init_serial( 3 );

	//初始化网络
	listenfd = init_server_socket();
	//初始化管道
	if(pipe(pipe_fd)<0){
		FATAL("pipe:"<< strerror(errno));
		exit(-1);
	}

	//创建客户端线程
#if 1
	int err;
	pthread_t tid;
	//err = pthread_create(&cli_tid, NULL, cli_process, NULL);
	err = pthread_create(&tid, NULL, collect_data, &pipe_fd[1]);
	if(err){
		FATAL("can not create trhread: " <<  strerror(errno));
	}
#endif

	//初始化epoll 
	//声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件
	struct epoll_event ev, events[EVENT_NUM];
	//生成用于处理accept的epoll专用的文件描述符
	epfd=epoll_create(256);

	//设置与要处理的事件相关的文件描述符
	ev.data.fd=listenfd;
	//设置要处理的事件类型
	ev.events=EPOLLIN;
	//注册epoll事件
	epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);

	ev.data.fd=myserial->fd;
	ev.events=EPOLLIN;
	epoll_ctl(epfd,EPOLL_CTL_ADD, myserial->fd, &ev);

	ev.data.fd=pipe_fd[0];
	ev.events=EPOLLIN;
	epoll_ctl(epfd,EPOLL_CTL_ADD, pipe_fd[0], &ev);

	while(1) {
		//等待epoll事件的发生,发生的事件放在events数据中
		nfds=epoll_wait(epfd,events,EVENT_NUM,tcp_timeout*60*1000);

		//cout << "alread to read file descriptor number is: " << nfds <<endl;
		if(0 == nfds){ //超时后断开所有tcp连接
			NOTICE("epollwait() time out.");
			close_all_clifd(epfd, clifd_array);
			continue;	
		}

		//处理所发生的所有事件
		for(i=0;i<nfds;++i){
			/*处理串口输入上的数据*/
			if(events[i].data.fd == myserial->fd){ 

				process_serial_data();

			}else if(events[i].data.fd == pipe_fd[0]){

				process_pipe_data(pipe_fd[0]);	

			}
			/*处理sock新客户连接*/
			//如果新监测到一个SOCKET用户连接到了绑定的SOCKET端口，建立新的连接。
			else if(events[i].data.fd == listenfd){

				process_new_client(epfd, events[i].data.fd);

			}/*如果是已经连接的用户，并且收到数据，那么进行读入*/
			else if(events[i].events&EPOLLIN){

				if(process_clisocket_data(epfd, events[i].data.fd)<0){
					events[i].data.fd = -1;
				}
			}

		}

	}

	return 0;
}
