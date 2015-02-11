// Log.cpp: implementation of the Log class.  
//  
//////////////////////////////////////////////////////////////////////  

#include "Log.h"  
#include <string>
#include <string.h>
#include <stdio.h>
using namespace std;

//////////////////////////////////////////////////////////////////////  
// Construction/Destruction  
//////////////////////////////////////////////////////////////////////  

/*定义一个全局的记录器对象*/
Logger Log::_logger = log4cplus::Logger::getInstance("main_log");  

Log::Log()  
{  
	snprintf(_log_path, sizeof(_log_path), "%s", "/home/plg");
	snprintf(_conf_path, sizeof(_conf_path), "%s", "/etc/gateway");
}  

Log::~Log()  
{  
}  

Log& Log::instance()  
{  
	static Log log;  
	return log;  
}  
bool Log::init_default_config_file() 
{
	FILE * fp_conf;
	fp_conf = fopen(_conf_name, "w+");
	if(fp_conf==NULL) {
		perror("fopen");
		return false;
	}

	char log_buffer[4096]={0};

	snprintf(log_buffer, sizeof(log_buffer),"%s%s\n%s",
			"log4cplus.rootLogger=INFO,ALL_MSGS\n"
			"log4cplus.appender.ALL_MSGS=log4cplus::RollingFileAppender\n"
			"log4cplus.appender.ALL_MSGS.File=",
			_log_name,
			"log4cplus.appender.ALL_MSGS.MaxFileSize=500KB\n"
			"log4cplus.appender.ALL_MSGS.MaxBackupIndex=0\n"
			"log4cplus.appender.ALL_MSGS.layout=log4cplus::PatternLayout\n"
			"log4cplus.appender.ALL_MSGS.layout.ConversionPattern=[%%D{%%m/%%d/%%y %%H:%%M:%%S}] [%%p] - %%m %%l%%n\n"
			"log4cplus.appender.STDOUT=log4cplus::ConsoleAppender\n"
			"log4cplus.appender.STDOUT.layout=log4cplus::PatternLayout\n"
			"log4cplus.appender.STDOUT.layout.ConversionPattern=[%%D{%%m/%%d/%%y %%H:%%M:%%S}] [%%p] - %%m %%l%%n");
	fprintf(fp_conf, log_buffer);
	fflush(fp_conf);
	fclose(fp_conf);
	return true;
}

bool Log::open_log(char name[])  
{  
	snprintf(_log_name, sizeof(_log_name), "%s/%s.%s", _log_path, strrchr(name,'/')+1, "log");
	snprintf(_conf_name, sizeof(_conf_name), "%s/%s.%s", _conf_path, strrchr(name,'/')+1, "properties");
	FILE * fp_conf;
	if(access(_conf_name, R_OK)!=0){
		cout <<"log4cplus configue file not exit ,will create the default configure file!" << endl;
		init_default_config_file();
	}

	Logger root = Logger::getRoot();
	PropertyConfigurator::doConfigure(_conf_name);

	return true;  
}
