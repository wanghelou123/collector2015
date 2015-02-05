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

/*����һ��ȫ�ֵļ�¼������*/
Logger Log::_logger = log4cplus::Logger::getInstance("main_log");  

Log::Log()  
{  
	snprintf(_log_path, sizeof(_log_name), "%s", "/home/plg");
}  

Log::~Log()  
{  
}  

Log& Log::instance()  
{  
	static Log log;  
	return log;  
}  

bool Log::open_log(int Log_level, char name[])  
{  

	if(Log_level<0 || Log_level>6) {
		Log_level = 0;	
	}

	snprintf(_log_name, sizeof(_log_name), "%s/%s.%s", _log_path, strrchr(name,'/')+1, "log");

	/* step 1: Instantiate an appender object,ʵ����һ���ҽ������� */  
	SharedAppenderPtr _append;
	if(Log_level == 0) {
		_append = new ConsoleAppender();//����̨�ҽ���
	}else {
		_append = new RollingFileAppender(_log_name, 1024*1024, 0, true); //�ļ��ҽ���,file size 1MB
	}
	_append->setName("file log test");  

	/* step 2: Instantiate a layout object��ʵ����һ�����������󣬿��������Ϣ�ĸ�ʽ*/  
	std::string pattern = "[%D{%m/%d/%y %H:%M:%S}] [%p] - %m %l%n";
	std::auto_ptr<Layout> _layout(new PatternLayout(pattern));  

	/* step 3: Attach the layout object to the appender�����������󶨵��ҽ����� */  
	_append->setLayout(_layout);  

	/* step 4: Instantiate a logger object��ʵ����һ����¼������ */  

	/* step 5: Attach the appender object to the logger�����ҽ�����ӵ���¼����  */  
	Log::_logger.addAppender(_append);  

	/* step 6: Set a priority for the logger�����ü�¼�������ȼ�  */  
	/* TRACE_LOG_LEVEL   = 0
	 * DEBUG_LOG_LEVEL   = 10000
	 * INFO_LOG_LEVEL    = 20000
	 * WARN_LOG_LEVEL    = 30000
	 * ERROR_LOG_LEVEL   = 40000
	 * FATAL_LOG_LEVEL   = 50000;
	 * OFF_LOG_LEVEL     = 60000;
	 */
	Log::_logger.setLogLevel(Log_level*10000);  

	return true;  
}
