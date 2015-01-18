// Log.cpp: implementation of the Log class.  
//  
//////////////////////////////////////////////////////////////////////  

#include "Log.h"  
#include <string>
#include <stdio.h>
using namespace std;

//////////////////////////////////////////////////////////////////////  
// Construction/Destruction  
//////////////////////////////////////////////////////////////////////  

/*定义一个全局的记录器对象*/
Logger Log::_logger = log4cplus::Logger::getInstance("main_log");  

Log::Log()  
{  
}  

Log::~Log()  
{  
}  

Log& Log::instance()  
{  
	static Log log;  
	return log;  
}  

bool Log::open_log()  
{  

	int Log_level = 0;    

	/* step 1: Instantiate an appender object,实例化一个挂接器对象 */  
	//SharedAppenderPtr _append(new FileAppender(_log_name)); //文件挂接器
	SharedAppenderPtr _append(new ConsoleAppender());//控制台挂接器
	_append->setName("file log test");  

	/* step 2: Instantiate a layout object，实例化一个布局器对象，控制输出信息的格式*/  
	//std::string pattern = "[%d{%m/%d/%y %H:%M:%S}] [%p] [%t] - %m %l%n";
	std::string pattern = "[%d{%m/%d/%y %H:%M:%S}] [%p] - %m %l%n";
	std::auto_ptr<Layout> _layout(new PatternLayout(pattern));  

	/* step 3: Attach the layout object to the appender，将布局器绑定到挂接器上 */  
	_append->setLayout(_layout);  

	/* step 4: Instantiate a logger object，实例化一个记录器对象 */  

	/* step 5: Attach the appender object to the logger，将挂接器添加到记录器中  */  
	Log::_logger.addAppender(_append);  

	/* step 6: Set a priority for the logger，设置记录器的优先级  */  
	Log::_logger.setLogLevel(Log_level);  

	return true;  
} 
