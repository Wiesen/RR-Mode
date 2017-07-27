#ifndef DEFINES_H_
#define DEFINES_H_

#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

// boost多线程库
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/thread.hpp>

// libevent头文件
#include <event.h>

// log4j的c++移植版，为C++程序提供日志功能
#include <log4cxx/logger.h>
#include <log4cxx/logstring.h>
#include <log4cxx/propertyconfigurator.h>
using namespace log4cxx;
extern LoggerPtr g_logger;

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
            TypeName(const TypeName&); \
            void operator=(const TypeName&)


#define CRLF "\r\n"

#define TOKEN_LENGTH 5
#define TOKEN_STR	 "12345"

#endif /* DEFINES_H_ */
