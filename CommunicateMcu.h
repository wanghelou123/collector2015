#ifndef COMMMCU_H
#define COMMMCU_H
class CommunicateMcu{
public:
	CommunicateMcu();
	~CommunicateMcu();
	//int Read(unsigned char * *recv_buf);
	int Read(unsigned char (&recv_buf)[1024]);
	int Write(unsigned char (&send_buf)[1024], int size, unsigned char (&recv_buf)[1024]);
private:
	int init_sock();	
	int sockfd;
	char server_ip[128];
	int server_port;
};
#endif
