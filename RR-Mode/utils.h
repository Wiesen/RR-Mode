#ifndef UTILS_H__
#define UTILS_H__

#include <netinet/in.h>
#include <unistd.h>
#include <sys/time.h>
#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/filesystem.hpp>
using namespace std;

namespace utils
{

static inline bool GetCurrentPath(std::string& current_path)
{
	try
	{
		boost::filesystem::path path = boost::filesystem::current_path();
		current_path = path.string();
		return true;
	}
	catch (boost::filesystem::filesystem_error & e)
	{
		cout << "current_path : " << current_path << ", error description :" << e.what() << endl;
		return false;
	}
}

static inline void SplitData(const std::string& str, const std::string& delimiter, std::vector<std::string>& vec_data)
{
	std::string s = str;
	size_t pos = 0;
	std::string token;
	while ((pos = s.find(delimiter)) != std::string::npos)
	{
		token = s.substr(0, pos);
		vec_data.push_back(token);
		s.erase(0, pos + delimiter.length());
	}
}

static inline bool FindCRLF(const std::string& s)
{
	if(s.find("\r\n") != std::string::npos)
		return true;
	else
		return false;
}

static inline std::string int2str(int v)
{
	std::stringstream ss;
	ss << v;
	return ss.str();
}

template<class T>
class Singleton: private T
{
public:
	// Mayer's Singleton（lazy initialization）
	// C++11之前的标准中并非线程安全，并未规定local static变量的内存模型
	// 在C++11或gcc4.0后线程安全，函数内的静态局部变量在第一次调用时初始化
	static T &Instance()
	{
		static Singleton<T> _instance;
		return _instance;
	}
private:
	Singleton()
	{
	}
	~Singleton()
	{
	}
};

// 模版类
template<typename T>
T& G()
{
	return Singleton<T>::Instance();
}

// 模版函数SafeDeleteArray
template<typename T> inline void SafeDeleteArray(T*& p)
{
	if (NULL != p)
	{
		delete[] p;
		p = 0;
	}
}

// 模版函数SafeDelete
template<typename T> inline void SafeDelete(T*& p)
{
	if (NULL != p)
	{
		delete p;
		p = 0;
	}
}

}
#endif
