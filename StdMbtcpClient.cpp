#include <iostream>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include "StdModbusTcp.h"
#include "Log.h"

using namespace std;
#define MAXLINE 256


void show_prog_info()
{
	cout << "collector2015 Software, Inc."<<endl;
	cout << "Program name StdMbtcpClient run with hardware v1." << endl;
	cout << "Compiled on " << __DATE__ << " at "<< __TIME__ <<endl;
}

void set_keepalive_option(int sockfd)
{
	int a=0,b=0,c=0,d=0;
	socklen_t optlen;
	getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*)&a, &optlen);  
	getsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (void*)&b, &optlen);  
	getsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void*)&c, &optlen);  
	getsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void*)&d, &optlen);  
	DEBUG("set before keepAlive:"<<a<<" keepIdle:"<<b<<" keepInterval:"<<c<<" keepCount:"<<d);

	int keepAlive = 1;   // 开启keepalive属性. 缺省值: 0(关闭)  
	int keepIdle = 60;   // 如果在60秒内没有任何数据交互,则进行探测. 缺省值:7200(s)  
	int keepInterval = 5;   // 探测时发探测包的时间间隔为5秒. 缺省值:75(s)  
	int keepCount = 3;   // 探测重试的次数. 全部超时则认定连接失效..缺省值:9(次)  
	int ret=0;

	ret = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepAlive, sizeof(keepAlive));  
	if(ret==0){
		DEBUG("SO_KEEPALIVE set sucess.");
	}else if(ret == -1){
		WARNING("SO_KEEPALIVE set failure.");
	}
	ret = setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (void*)&keepIdle, sizeof(keepIdle));  
	if(ret==0){
		DEBUG("SO_KEEPIDLE set sucess.");
	}else if(ret == -1){
		WARNING("SO_KEEPALIVE set sucess.");
	}
	ret = setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void*)&keepInterval, sizeof(keepInterval));  
	if(ret==0){
		DEBUG("TCP_KEEPINTVL set success.");
	}else if(ret == -1){
		WARNING("TCP_KEEPINTVL set failure.");
	}
	ret = setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void*)&keepCount, sizeof(keepCount));  
	if(ret==0){
		DEBUG("TCP_KEEPCNT set success.");
	}else if(ret == -1){
		WARNING("TCP_KEEPCNT set failure.");
	}

	a=0,b=0,c=0,d=0;
	getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*)&a, &optlen);  
	getsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (void*)&b, &optlen);  
	getsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void*)&c,&optlen );  
	getsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void*)&d, &optlen);  
	DEBUG("set afer keepAlive:"<<a<<" keepIdle:"<<b<<" keepInterval:"<<c<<" keepCount:"<<d);

	return;
}
int connect_to_server(char* server_ip, int server_port)
{
	struct hostent *h;
	struct sockaddr_in servaddr;
	
	DEBUG("server_ip:"<<server_ip << ", server_port:"<<server_port);

	if((h=gethostbyname(server_ip))==NULL)
	{
		FATAL("hgethostbyname error, can not getIP");
		return -1;	
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	memcpy(&servaddr.sin_addr, (struct in_addr *)h->h_addr, sizeof(struct in_addr));
	servaddr.sin_port = htons(server_port);

	int ret = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

	return (0==ret)?sockfd:ret;
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
				NOTICE("clean recv buffer "<< count<<" bytes");	

		}else if(n==0){
			NOTICE("recv(): the peer has  performed  an  orderly shutdown.");
			break;
		}else if(n<0){
			FATAL("recv():an error occurred."<<strerror(errno));
			break;
		}
	}
	NOTICE("clean the socket buff ok, total " << count <<" bytes");
}

int  process_socket_data(int sockfd, StdModbusTcp & std_modbus_tcp)
{
	int ret = 0, n=0;
	unsigned char line[MAXLINE];
	if ( (n = recv(sockfd, line, MAXLINE, 0)) <= 0) {
		if(0 == n){
			DEBUG("recv return 0, the peer has performed an orderly shutdown.");
		}else{
			DEBUG("recv from sockfd error ECONNRESET. will close the socket.");
		}
		ret = -1;
		goto err;
	}else{
		DEBUG("recv bytes from socket is: "<< n);
		if(n>128){
			clean_socket_buf(sockfd);
			return 0;
		}
		char tmp_buffer[512]={0};
		for(int i = 0; i< n; i++) {
			snprintf(tmp_buffer+i*3, sizeof(tmp_buffer), "%.2x ", (char)line[i]);
		}
		DEBUG("recvd request buf: "<< tmp_buffer);

		n = std_modbus_tcp.process_modbus_data(line, n);
	
		if(send(sockfd, line, n, MSG_NOSIGNAL)<0){//写socket出错后关闭socket
			FATAL("send "<<strerror(errno)<< ",will close socket.");
			ret = -2;
			goto err;
		}
	}

err:
	if(ret<0){
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

int main(int argc, char * argv[])
{
	
	if(argc != 3) {
		printf("usage: %s <hostname or IPaddr> <port>\n", strrchr(argv[0], '/')+1);	
		exit(-1);
	}
	//显示程序版本信息
	show_prog_info();

	// 打开日志  
	if (!Log::instance().open_log(argv[0]))  
	{  
		std::cout << "Log::open_log() failed" << std::endl;  
		exit(-1);
	} 

	//创建ModbusTcp对象
	StdModbusTcp std_modbus_tcp; 

	int sockfd;
	int ret = 0;
	for(;;) {
		while(1) { 
			if((sockfd = connect_to_server(argv[1], atoi(argv[2]) )) == -1) {
				NOTICE("connect to "<< argv[1]<<":"<<argv[2]<<" failed!");
				::sleep(30);
			}else {
				set_keepalive_option(sockfd);
				break;	
			}
		}

		while(1) {
			struct timeval timeout;
			timeout.tv_sec = 10;
			timeout.tv_usec = 0;
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(sockfd, &fds);
			ret = select (sockfd+1, &fds, NULL, NULL, &timeout);
			if(ret == -1) {
				FATAL("select():"<< strerror(errno));	
			}
			else if(ret == 0){
				continue;
			}

			if(process_socket_data(sockfd, std_modbus_tcp)<0){
				break;
			}
		}

		DEBUG("I am the MbtcpClient");
	}

	return 0;
}
