#pragma once
#include <functional>
#include <string>

struct noncopyable 
{
protected:
	noncopyable() = default;
	virtual ~noncopyable() = default;

	noncopyable(const noncopyable &) = delete;
	noncopyable &operator=(const noncopyable &) = delete;
};

template<typename DerivedClass>
struct Singleton
{
	static DerivedClass& Instance() {
		static DerivedClass m_instance;
		return m_instance;
	}
protected:
	Singleton() = default;
	~Singleton() = default;
private:
	Singleton(const Singleton&) = delete;
	Singleton& operator=(Singleton&) = delete;
};

struct util
{
	static std::string format(const char* fmt, ...);
	static int64_t timeMicro();	//Î¢Ãë
	static int64_t timeMilli() { return timeMicro() / 1000; }	//ºÁÃë
	static int64_t steadyMicro();
	static int64_t steadyMilli() { return steadyMicro() / 1000; }
	//static std::string readableTime(time_t t);
	static int64_t atoi(const char *b, const char *e) { return strtol(b, (char **)&e, 10); }
	static int64_t atoi2(const char *b, const char *e) {
		char **ne = (char **)&e;
		int64_t v = strtol(b, ne, 10);
		return ne == (char **)&e ? v : -1;
	}
	static int64_t atoi(const char *b) { return atoi(b, b + strlen(b)); }
	//static int addFdFlag(int fd, int flag);

	static std::string GetNowTimeString();

	static bool MakeSureDirExist(const char *dir, size_t len);
	static bool MakeSureDirExist(const char *dir) { return MakeSureDirExist(dir, strlen(dir)); }
	static bool MakeSureDirExist(const std::string& dir) { return MakeSureDirExist(dir.c_str(), dir.size()); }
	//Ê§°Ü·µ»Ø"", ·µ»ØµÄÄ¿Â¼×Ö·û´®Ä©Î²Ã»ÓÐ'\\'ºÍ'/'
	static std::string GetCurrentWorkDir();
};

struct ExitCaller : private noncopyable 
{
	~ExitCaller() { functor_(); }
	ExitCaller(std::function<void()> &&functor) : functor_(std::move(functor)) {}

private:
	std::function<void()> functor_;
};

