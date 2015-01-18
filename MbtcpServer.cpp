#include <iostream>
#include <stdlib.h>
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
#include <string.h>
#include <errno.h>
#include "Log.h"
#include "modbus.h"

using namespace std;

#define	EVENT_NUM 20
#define MAXLINE 256
#define	CLI_NUM	5

static int clifd_array[CLI_NUM]={0};

void show_prog_info()
{
	cout << "\ncollector2015 Software, Inc."<<endl;
	cout << "Program name MbtcpServer run with hardware v1." << endl;
	cout << "Compiled on " << __DATE__ << " at "<< __TIME__ <<endl;
}

int init_server_socket()
{
	int listenfd;
	struct sockaddr_in serveraddr;
	listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port=htons(502);
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

int  process_socket_data(int epfd, int sockfd)
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

		printf("recvd request buf: ");
		for(int i = 0; i< n; i++) {
			printf("%.2x ", line[i]);
		}printf("\n");

		n = process_modbus_data(line);
	
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

int main()
{
	int i, listenfd,epfd,nfds;

	//显示程序版本信息
	show_prog_info();

	// 打开日志  
	if (!Log::instance().open_log())  
	{  
		std::cout << "Log::open_log() failed" << std::endl;  
		exit(-1);
	} 

	//初始化网络
	listenfd = init_server_socket();


	struct epoll_event ev, events[EVENT_NUM];
	epfd=epoll_create(256);
	ev.data.fd=listenfd;
	ev.events=EPOLLIN;
	epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);

	
	while(1){
		//nfds=epoll_wait(epfd,events,EVENT_NUM,tcp_timeout*60*1000);
		nfds=epoll_wait(epfd,events,EVENT_NUM, 30*60*1000);

		for(i=0;i<nfds;++i){
			if(events[i].data.fd == listenfd){

				process_new_client(epfd, events[i].data.fd);

			}/*如果是已经连接的用户，并且收到数据，那么进行读入*/
			else if(events[i].events&EPOLLIN){

				if(process_socket_data(epfd, events[i].data.fd)<0){
					events[i].data.fd = -1;
				}
			}
		}
#if 0
		int n = mcu.Read(recv_buf);
		for(int i = 0; i<n; i++){
			printf("%.2x ", recv_buf[i]);
		}printf("\n");
		::sleep(2);
#endif
		
	}

	DEBUG("I am the MbtcpServer");

	return 0;
}
