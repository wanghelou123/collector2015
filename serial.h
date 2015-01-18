#ifndef	__SERIAL__
#define	__SERIAL__

#define SERIAL_DEV_NAME		"/dev/ttySAC1"
#define TO_B110 3200000 /* These values are the timeout delays */
#define TO_B300 1600000 /* at the end of packets of data.      */
#define TO_B600  800000 /* At this stage a true calculation    */
#define TO_B1200 400000 /* has not been worked out. So these   */
#define TO_B2400 200000 /* values are just a guess.            */
#define TO_B4800 100000 /*                                     */
#define TO_B9600  50000 /* The spec says that a message frame  */
#define TO_B19200 25000 /* starts after a silent interval of   */
#define TO_B38400 12500 /* at least 3.5 character times.       */
#define TO_B57600  8333 /* These are uS times.                */
#define TO_B115200 4167 

#define MAX_DATA_LENGTH 256
#define PORT_FAILURE -11 
#define MAX_RESPONSE_LENGTH	256

#define FALSE 0 
#define TRUE 1

//#define DEBUG
class serial
{
public:
	serial(char *devName, int _baudRate, int _dataBit, char _parity, int _stopBit);
	~serial();
	int serialWrite(unsigned char *&buf, int size);
	int serialRead(unsigned char *received_string);
	int serialInit();
	int serialRestart();
	
private:
	int serialOpen();
	int serialClose();
	//void sigUsr(int signo);
	void initSignal();

public:
	int	fd;				//串口操作句柄
private:
	int char_interval_timeout;
	char serialName[20];//串口的名字
	int baud_i;			//波特率
	const int dataBit;	//数据位
	char parity;		//奇偶校验
	int stopBit;		//停止位
	int nRS485;

};

#endif
