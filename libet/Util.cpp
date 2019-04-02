#include "Util.h"
#include <fcntl.h>
#include <stdarg.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <assert.h>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifndef WIN32
#define WIN32
#endif 

#ifdef WIN32  
#include <io.h>
#include <direct.h>  
#endif  
#ifdef linux   
#include <unistd.h> 
#include <sys/types.h>  
#include <sys/stat.h> 
#endif  
//#include <string>

using namespace std;


string util::format(const char *fmt, ...)
{
	char buffer[500];
	unique_ptr<char[]> release1;
	char *base;
	for (int iter = 0; iter < 2; iter++)
	{
		int bufsize;
		if (iter == 0)
		{
			bufsize = sizeof(buffer);
			base = buffer;
		}
		else
		{
			bufsize = 30000;
			base = new char[bufsize];
			release1.reset(base);
		}
		char *p = base;
		char *limit = base + bufsize;
		if (p < limit)
		{
			va_list ap;
			va_start(ap, fmt);
			p += vsnprintf(p, limit - p, fmt, ap);
			va_end(ap);
		}
		// Truncate to available space if necessary
		if (p >= limit)
		{
			if (iter == 0)
				continue;  // Try again with larger buffer
			else
			{
				p = limit - 1;
				*p = '\0';
			}
		}
		break;
	}
	return base;
}

int64_t util::timeMicro()
{
	chrono::time_point<chrono::system_clock> p = chrono::system_clock::now();
	return chrono::duration_cast<chrono::microseconds>(p.time_since_epoch()).count();
}
int64_t util::steadyMicro()
{
	chrono::time_point<chrono::steady_clock> p = chrono::steady_clock::now();
	return chrono::duration_cast<chrono::microseconds>(p.time_since_epoch()).count();
}

//std::string util::readableTime(time_t t) {
//	struct tm tm1;
//	localtime_r(&t, &tm1);
//	return format("%04d-%02d-%02d %02d:%02d:%02d", tm1.tm_year + 1900, tm1.tm_mon + 1, tm1.tm_mday, tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
//}

//int util::addFdFlag(int fd, int flag) {
//	int ret = fcntl(fd, F_GETFD);
//	return fcntl(fd, F_SETFD, ret | flag);
//}


std::string util::GetNowTimeString()
{
	struct tm tm_;
	auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
	localtime_s(&tm_, &now);
	auto tt = std::put_time(&tm_, "%Y_%m_%d_%H_%M_%S");
	stringstream ss;
	ss << tt;
	return ss.str();
}

bool util::MakeSureDirExist(const char *dir, size_t len)
{
	assert(dir != NULL);
	if (len < 1 || len > 1024)
		return false;
	char *head, *p;
	char tmpDir[1024] = { 0 };
	//strcpy_s(tmpDir, 1024, dir);	//拷贝1024字节发现，len之后的字符全部变为-2
	strcpy_s(tmpDir, len + 1, dir);
	head = tmpDir;
	if (*head == '\\' || *head == '/')
		++head;
	p = head;
	if (*(tmpDir + len - 1) != '\\' && *(tmpDir + len - 1) != '/')
		*(tmpDir + len) = '\\';

	while (*p)
	{
		if (*p == '\\' || *p == '/')
		{
			*p = '\0';
			if (_access(head, 0))// 头文件io.h
			{
				if (_mkdir(head))
				{
#ifdef _DEBUG
					fprintf(stderr, "Failed to create directory %s\n", head);
					return false;
#endif
				}
			}
			*p = '\\';
		}
		++p;
	}
	return true;
}

std::string util::GetCurrentWorkDir()
{
	char *buf;
	string ret;
	if ((buf = _getcwd(NULL, 0)) == NULL)
		ret = string("");
	else
	{
		ret = buf;
		free(buf);	//传NULL+0一定要free
	}
	return ret;
}