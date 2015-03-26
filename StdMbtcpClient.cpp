#include <iostream>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
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

int connect_to_server(char* server_ip, int server_port)
{
	struct hostent *h;
	struct sockaddr_in servaddr;
	
	printf("server_ip:%s\n", server_ip);
	printf("server_port:%d\n", server_port);

	if((h=gethostbyname(server_ip))==NULL)
	{
		fprintf(stderr,"cat not getIP\n");
		return -1;	
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	memcpy(&servaddr.sin_addr, (struct in_addr *)h->h_addr, sizeof(struct in_addr));
	servaddr.sin_port = htons(server_port);

	int ret = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	printf("ret = %d\n", ret);

	return (0==ret)?sockfd:ret;
}
 
int hand_shake(int sockfd) {
	unsigned char send_buffer[22]={0x15, 0x01, 0x22, 0x22, 0x00, 0x10};	
	unsigned char recv_buffer[10];
	unsigned char right_response_buffer[7] = {0x15, 0x01, 0x22, 0x22, 0x00, 0x01, 0x80};
	unsigned char error_response_buffer[7] = {0x15, 0x01, 0x22, 0x22, 0x00, 0x01, 0x01};
	int ret = 0;
	FILE *fp;
	char buffer[128];
	char serial_number[20];
	char *pos;

	fp = fopen("/etc/gateway/network.txt", "r");

	if(NULL == fp){
		perror("fopen:");
	}


	while(fgets(buffer, 80, fp)){
		pos=strrchr(buffer, '=');
		if(pos){
			char tmp[128]={0};
			strncpy(tmp, buffer, strlen(buffer)-strlen(pos));
			pos++;
			if(strcmp(tmp, "Serialnum") == 0){
				strcpy(serial_number, pos);
				break;
			}
		}
	}

	memcpy(send_buffer+6, serial_number, 16);

	DEBUG("serial_number:"<< serial_number);
	char tmp_buffer[50];
	memset(tmp_buffer, 0, sizeof(tmp_buffer));
	for(int i=0;i<22;i++) {
		snprintf(tmp_buffer+i*3, sizeof(tmp_buffer), "%.2x ", (char)send_buffer[i]);
	}
	DEBUG("shak  hands packet:"<< tmp_buffer);

	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);

	ret = send(sockfd, send_buffer, sizeof(send_buffer),MSG_NOSIGNAL);
	if(ret != 22) {
		FATAL("send:"<< strerror(errno));
		return -1;
	}
	ret = select(sockfd+1, &fds, NULL, NULL, &timeout);
	if(ret == 0) {
		NOTICE("select, wiat hand shake response packet timeout!");
		return -1;
	}else if(ret == -1) {
			FATAL("select():"<< strerror(errno));	
			return -1;
	}
	recv(sockfd, recv_buffer, 7, 0);

	if(0 == memcmp(recv_buffer, right_response_buffer, sizeof(right_response_buffer))) {
		DEBUG("shake hands success!");
	} else if(0 == memcmp(recv_buffer, error_response_buffer, sizeof(error_response_buffer))) {
		NOTICE("shake hands failed!");
		ret = -1;
	}else {
		FATAL(__func__<<" error packet!");
		ret = -1;
	}

	return ret;
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
		char tmp_buffer[128]={0};
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
				break;	
			}
		}
#if 0
		if(hand_shake(sockfd)<0){
			shutdown(sockfd, SHUT_RDWR);
			close(sockfd);
		}
#endif

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
			if(process_socket_data(sockfd, std_modbus_tcp)<0){
				shutdown(sockfd, SHUT_RDWR);
				close(sockfd);
				break;
			}
		}

		DEBUG("I am the MbtcpClient");
	}

	return 0;
}
