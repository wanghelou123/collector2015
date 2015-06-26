#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "Log.h"
#include "CommunicateMcu.h"

CommunicateMcu::CommunicateMcu()
{
	DEBUG("create a commMuc object.");
	memset(server_ip, '\0', sizeof(server_ip));
	strncpy(server_ip, "127.0.0.1", strlen("127.0.0.1"));
	server_port = 506;
	while(1){
		if(init_sock() == -1){
			FATAL("init socket error!");
		}else{
			NOTICE("init socket success!");
			break;
		}
		::sleep(2);
	}
}

CommunicateMcu::~CommunicateMcu()
{
	DEBUG(__func__);
	if(shutdown(sockfd,SHUT_RDWR)<0){
		FATAL("shutdown "<<strerror(errno));
	}
	if(close(sockfd)<0){
		FATAL("close "<<strerror(errno));
	}
}

int CommunicateMcu::Read(unsigned char (&recv_buf)[1024])
{
	if(this->socket_status == 0) {
		if(init_sock() == -1){
			FATAL("init socket error!");
			return -1;
		}
	}
	unsigned char send_buf[] = {0x01, 0x04, 0x00, 0x00, 0xFF, 0xFF, 0xF1, 0xBA};
	int  nRet;
	struct timeval tmOut;
	tmOut.tv_sec = 3;
	tmOut.tv_usec = 0;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	int n = send(sockfd, send_buf, sizeof(send_buf), MSG_NOSIGNAL);
	if(-1 == n) {
		FATAL("send:"<< strerror(errno));
		this->socket_status = 0;
		return -1;
	}

	nRet= select(FD_SETSIZE, &fds, NULL, NULL, &tmOut);
	if(nRet== 0){
		WARNING("select() read from serial timeout!");
		return -1;
	}else if(nRet<0){
		FATAL("select error:"<< strerror(errno));
		return -1;
	}
	n = recv(sockfd, recv_buf, 1024, 0);
	if(n==0){
		NOTICE("recv: the peer has  performed  an  orderly shutdown.");
	}else if(n<0){
		FATAL("recv:an error occurred."<<strerror(errno));
	}
	if(n<=0) {
		this->socket_status = 0;	
		shutdown(sockfd, SHUT_WR);
		close(sockfd);
	}

	return (n>0) ? n : -1;

}

int CommunicateMcu::Write(unsigned char (&send_buf)[1024], int size, unsigned char (&recv_buf)[1024])
{
	if(this->socket_status == 0) {
		if(init_sock() == -1){
			FATAL("init socket error!");
			return -1;
		}
	}
	int n = send(sockfd, send_buf, size, MSG_NOSIGNAL);
	if(-1 == n) {
		FATAL("send:"<< strerror(errno));
		this->socket_status = 0;
		return -1;
	}
	struct timeval tmOut;
	tmOut.tv_sec = 3;
	tmOut.tv_usec = 0;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	int  nRet;


	for(int i=0; i<size; i++) {
		printf("%.2x ", send_buf[i]);
	}printf("\n");

	nRet= select(FD_SETSIZE, &fds, NULL, NULL, &tmOut);
	if(nRet== 0){
		WARNING("select() read from serial timeout!");
		return 0;
	}else if(nRet<0){
		FATAL("select error:"<< strerror(errno));
		return 0;
	}
	n = recv(sockfd, recv_buf, 1024, 0);
	if(n==0){
		NOTICE("recv: the peer has  performed  an  orderly shutdown.");
	}else if(n<0){
		FATAL("recv:an error occurred."<<strerror(errno));
	}
	DEBUG("recv "<< n << " bytes from socket");

	return (n>0) ? n : -1;
}

int CommunicateMcu::init_sock()
{
	DEBUG("commMuc init the sock.");
	struct hostent *h;
	struct sockaddr_in servaddr;

	if((h=gethostbyname(server_ip))==NULL)
	{
		fprintf(stderr,"cat not getIP\n");
		return -1;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	memcpy(&servaddr.sin_addr, (struct in_addr *)h->h_addr, sizeof(struct in_addr));
	servaddr.sin_port = htons(server_port);

	int ret = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

	if(0 == ret) {
		this->socket_status = 1;
	}else {
		this->socket_status = 0;
	}

	return ret;

}
