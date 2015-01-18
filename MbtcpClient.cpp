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

int main()
{
	//显示程序版本信息
	show_prog_info();

	// 打开日志  
	if (!Log::instance().open_log())  
	{  
		std::cout << "Log::open_log() failed" << std::endl;  
		exit(-1);
	} 
	DEBUG("I am the MbtcpClient");
	return 0;
}
