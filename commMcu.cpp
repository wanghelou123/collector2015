#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include "Log.h"
#include "commMcu.h"

commMcu::commMcu()
{
	DEBUG("create a commMuc object.");
	memset(server_ip, '\0', sizeof(server_ip));
	strncpy(server_ip, "192.168.0.93", strlen("192.168.0.93"));
	server_port = 506;
	while(1){
		if(init_sock() == -1){
			FATAL("init socket error!");
			exit(-2);
		}else{
			NOTICE("init socket success!");
			break;
		}
		::sleep(2);
	}
}

commMcu::~commMcu()
{
	DEBUG(__func__);
	if(shutdown(sockfd,SHUT_RDWR)<0){
		FATAL("shutdown "<<strerror(errno));
	}
	if(close(sockfd)<0){
		FATAL("close "<<strerror(errno));
	}
}

int commMcu::Read(unsigned char (&recv_buf)[1024])
{
	unsigned char send_buf[] = {0x01, 0x04, 0x00, 0x00, 0xFF, 0xFF, 0xF1, 0xBA};
	//unsigned char recv_buf[512];
	int n = send(sockfd, send_buf, sizeof(send_buf), MSG_NOSIGNAL);
	struct timeval tmOut;
	tmOut.tv_sec = 2;
	tmOut.tv_usec = 0;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	int  nRet;

	nRet= select(FD_SETSIZE, &fds, NULL, NULL, &tmOut);
	if(nRet== 0){
		WARNING("select() read from serial timeout!");
		return 0;
	}else if(nRet<0){
		FATAL("select error:"<< strerror(errno));
		return 0;
	}
	n = recv(sockfd, recv_buf, 1024, 0);
	DEBUG("recv "<< n << " bytes from socket");
	//	for(int i = 0; i<n; i++){
	//		printf("%.2x ", recv_buf[i]);
	//	}printf("\n");

	return n;

}

int commMcu::Write(unsigned char (&send_buf)[1024], int size, unsigned char (&recv_buf)[1024])
{
	int n = send(sockfd, send_buf, size, MSG_NOSIGNAL);
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
	DEBUG("recv "<< n << " bytes from socket");
	
	return n;
}

int commMcu::init_sock()
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

	return ret;

}
