#include <iostream>
#include <stdlib.h>
#include "Log.h"

using namespace std;

void show_prog_info()
{
	cout << "\ncollector2015 Software, Inc."<<endl;
	cout << "Program name MbtcpClient run with hardware v1." << endl;
	cout << "Compiled on " << __DATE__ << " at "<< __TIME__ <<endl;
}

int main(int argc, char * argv[])
{
	//显示程序版本信息
	show_prog_info();

	int log_level = 3;//default WARN_LOG_LEVEL
	if(argc == 2) {
		log_level = atoi(argv[1]);	
	}
	printf("===>log_level = %d\n", log_level);
	// 打开日志  
	if (!Log::instance().open_log(log_level, argv[0]))  
	{  
		std::cout << "Log::open_log() failed" << std::endl;  
		exit(-1);
	} 
	for(int i =0; i<10000; i++)
		WARNING("I am the MbtcpClient");
	DEBUG("I am the MbtcpClient");
	return 0;
}
