#include<WinSock2.h>
#include<atlimage.h>
#include<Windows.h>
#include<gdiplus.h>
#include<Mstcpip.h>
#include <iostream>
#include <spdlog\spdlog.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "Test.h"
#include "Logger.h"
#include "TcpServer.h"
#include "TcpConn.h"
#include "EventBase.h"
#include "Timer.h"
#include "base64.h"
#include "Util.h"

using namespace std;
//using LDebug = spdlog::debug;
//typedef spdlog::debug LDebug;
//using namespace spdlog;
//#define LogDebug spdlog::debug


enum MsgType
{
	handOut = 1,
	heartbeat = 4
};

void screenshot(std::vector<BYTE>& data);


void TestTcpServer()
{
	EventBase base;
	TcpServerPtr server = TcpServer::startServer(&base, "", 50003);
	server->OnConnCreate([]
	{
		TcpConnPtr con = make_shared<TcpConn>();
		con->OnMsg(new LengthCodec, [](const TcpConnPtr& con, Slice msg)
		{
			//info("recv msg: %.*s", (int)msg.size(), msg.data());
			con->SendMsg(msg);
		});
		return con;

	});
	base.Loop();
}

void TestTimer()
{
	EventBase base;
	TcpServerPtr server = TcpServer::startServer(&base, "", 50003);
	server->OnConnRead([](const TcpConnPtr& conn)
	{
		
	});

	//base->RunAfter(20000, []() { cout << "a task runAfter in 20000ms" << endl; });
	//base->RunAfter(100, []() { cout << "a task runAfter in 100ms interval 1000ms" << endl; }, 1000);
	TimerId id = base.RunAt(time(NULL) * 1000 + 300, []() { cout << "a task runAt in now+300 interval 500ms" << endl; }, 500);
	base.RunAfter(2000, [&]() 
	{
		cout << "cancel task of interval 500ms" << endl;
		base.Cancel(id);
	});
	base.RunAfter(3000, [&base]() { base.Exit(); });
	base.Loop();
}

void IdleCloseTest()
{
	EventBase base;
	TcpServerPtr server = TcpServer::startServer(&base, "", 50003);
	server->OnConnState([](const TcpConnPtr &con)
	{
		if (con->GetState() == con->Connected)
		{
			con->AddIdleCB(2, [](const TcpConnPtr &con)
			{
				cout << "idle for 2 seconds, close connection" << endl;
				//info("idle for 2 seconds, close connection");
				con->Close();
			});
		}
	});
	base.Loop();
}

void HeartbeatTest()
{
	EventBase base;
	TcpServerPtr server = TcpServer::startServer(&base, "", 50003);
	server->OnConnCreate([]
	{
		TcpConnPtr con = make_shared<TcpConn>();
		con->OnMsg(new LengthCodec, [](const TcpConnPtr& con, Slice msg)
		{
			//info("recv msg: %.*s", (int)msg.size(), msg.data());
			con->SendMsg(msg);
		});
		return con;

	});
	server->OnConnState([](const TcpConnPtr &con)
	{
		if (con->GetState() == con->Connected)
		{
			con->AddIdleCB(15, [](const TcpConnPtr &con)
			{
				cout << "idle for 15 seconds, close connection" << endl;
				con->Close();
			});
		}
	});
	base.Loop();
}

void TestLog()
{
	//static std::shared_ptr<spdlog::logger> combined_logger;
	//combined_logger"hello" << endl;
	//spdlog::debug("hello");
	//spdlog::debug("hello");
	//spdlog::set_level(spdlog::level::debug);
	//debug("hello");
	//LogDebug("ok");

	//LogDebug("JUST A TEST");
	//LogInfo("BBBBBBBBBBBBB");
	//DEBUG << "TEST <<" << "hello ";
}

Buffer GenerateData()
{
	Buffer buf;
	Timer timer;

	int length = 0;
	int commandId = MsgType::handOut;

	auto now = chrono::system_clock::to_time_t(system_clock::now());
	struct tm tm_;
	localtime_s(&tm_, &now);
	auto tt = put_time(&tm_, "[%Y-%m-%d %X]  ");
	stringstream ss;
	ss << tt;
	//LDebug(ss.str());
	string nowTime = ss.str();

	FILE *fp = nullptr;
	//errno_t err = fopen_s(&fp, FIVE_K_TXT, "r");
	//errno_t err = fopen_s(&fp, TWENTY_K_TXT, "r");
	//errno_t err = fopen_s(&fp, TWO_HUNDRED_K_TXT, "r");
	//errno_t err = fopen_s(&fp, ONE_M_TXT, "r");
	errno_t err = fopen_s(&fp, ONEM_TXT, "r");
	//errno_t err = fopen_s(&fp, TWOM_TXT, "r");
	if (err != 0) {
		//printf("fopen_s() failed, err = %d\n", err);
		LCritical("fopen_s failed, code:{}", err);
		return Buffer();
	}
	fseek(fp, 0, SEEK_END);
	int txtLen = ftell(fp);
	fseek(fp, 0, 0);
	//LDebug("txt size:{}", txtLen);
	//memcpy_s(send_buf, sizeof(int), &length, sizeof(int));
	//memcpy_s(send_buf + 4, sizeof(int), &commandId, sizeof(int));
	//memcpy_s(send_buf + 4 + 4, now_time_len, now_time, now_time_len);
	//memcpy_s(send_buf + 8, ss.size(), ss.data(), ss.size());

	//length = sizeof(length) + sizeof(commandId) + nowTime.size() + txtLen;
	//buf.appendValue(length);
	length = sizeof(commandId) + nowTime.size();
	buf.appendValue(commandId);
	buf.append(nowTime);
	char *sendBuf = new char[txtLen];
	fread(sendBuf, txtLen, 1, fp);
	fclose(fp);
	buf.append(sendBuf, txtLen);
	delete[] sendBuf;
	sendBuf = nullptr;
	return buf;
}

Buffer GenerateBase64PicData()
{
	Buffer buf;
	int commandId = MsgType::handOut;

	//截图
	vector<BYTE> vec;
	screenshot(vec);
	size_t pic_bytes = vec.size();
	if (!pic_bytes) 
	{
		LError("screenshot failed");
		return Buffer();
	}
	//base64编码截图数据
	string ss = base64_encode(vec.data(), vec.size());
	//LDebug("base64 pic size: {}", ss.size());
	buf.appendValue(commandId);
	buf.append((char*)ss.data(), ss.size());

	return buf;
}

Buffer GeneratePicData()
{
	Buffer buf;
	int commandId = MsgType::handOut;

	//截图
	vector<BYTE> vec;
	screenshot(vec);
	size_t pic_bytes = vec.size();
	if (!pic_bytes)
	{
		LError("screenshot failed");
		return Buffer();
	}

	buf.appendValue(commandId);
	buf.append((char*)vec.data(), vec.size());
	return buf;
}

bool save_png_memory(HBITMAP hbitmap, std::vector<BYTE> &data)
{
    Gdiplus::Bitmap bmp(hbitmap, nullptr);

    //write to IStream
    IStream* istream = nullptr;
    CreateStreamOnHGlobal(NULL, TRUE, &istream);

    CLSID clsid;
    LPCOLESTR lpsz_png = L"{557cf406-1a04-11d3-9a73-0000f81ef32e}";
    //LPCOLESTR lpsz_bmp = L"{557CF400-1A04-11D3-9A73-0000F81EF32E}";  //保存为bmp

    CLSIDFromString(/*lpsz_bmp*/lpsz_png, &clsid);
    Gdiplus::Status status = bmp.Save(istream, &clsid);
    if (status != Gdiplus::Status::Ok)
        return false;

    //get memory handle associated with istream
    HGLOBAL hg = NULL;
    GetHGlobalFromStream(istream, &hg);

    //copy IStream to buffer
    int bufsize = GlobalSize(hg);
    data.resize(bufsize);

    //lock & unlock memory
    LPVOID pimage = GlobalLock(hg);
    memcpy(&data[0], pimage, bufsize);
    GlobalUnlock(hg);

    istream->Release();
    return true;
}

void screenshot(std::vector<BYTE>& data)
{
    CoInitialize(NULL);

    ULONG_PTR token;
    Gdiplus::GdiplusStartupInput tmp;
    Gdiplus::GdiplusStartup(&token, &tmp, NULL);

    //take screenshot
    RECT rc;
    GetClientRect(GetDesktopWindow(), &rc);
    auto hdc = GetDC(0);
    auto memdc = CreateCompatibleDC(hdc);
    auto hbitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    auto oldbmp = SelectObject(memdc, hbitmap);
    BitBlt(memdc, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
    SelectObject(memdc, oldbmp);
    DeleteDC(memdc);
    ReleaseDC(0, hdc);

    save_png_memory(hbitmap, data);

    //std::vector<BYTE> data;
    //if (save_png_memory(hbitmap, data))
    //{
    //    //write from memory to file for testing:
    //    std::ofstream fout("F:\\test.png", std::ios::binary);
    //    fout.write((char*)data.data(), data.size());
    //}

    DeleteObject(hbitmap);
    Gdiplus::GdiplusShutdown(token);
    CoUninitialize();
}

void PeriodicallySendTest()
{
	EventBase base;
	TcpServerPtr server = TcpServer::startServer(&base, "", 50003);

	//接收消息->处理->发送消息
	server->OnConnCreate([]
	{
		TcpConnPtr con = make_shared<TcpConn>();
		con->OnMsg(new LengthCodec, [](const TcpConnPtr& con, Slice msg)
		{
			if (*(int32_t*)msg.data() == MsgType::heartbeat)
			{
				Buffer buf;
				int commandId = MsgType::heartbeat;
				string tmp = "heartbeat";
				buf.append((char*)&commandId, sizeof(int)).append(tmp.data(), tmp.size());
				con->SendMsg(buf);
			}
		});
		return con;

	});

	//Handshake后定时发送任务
	server->OnConnTimer([](const TcpConnPtr& con)
	{
		//int commandId = MsgType::handOut;
		//string tmp = "business";
		//buf.append((char*)&commandId, sizeof(int)).append(tmp.data(), tmp.size());
		TimerId task = con->GetBase()->RunEvery([con]()
		{
			Buffer buf;
			buf = GenerateData();
			con->SendMsg(buf);
		}, 1000);
		LDebug("start timer task, timer id: {} {}", task.first, task.second);
		//con->m_timerTasks.push_back(std::move(TimerId(task)));
		con->m_timerTasks.emplace_back(task);
	});

	//创建连接时添加心跳，关闭连接时取消定时任务，Cleanup中清理心跳
	//TODO: 定时任务的取消应该移到Clenaup
	server->OnConnState([&base](const TcpConnPtr &con)
	{
		if (con->GetState() == con->Connected)
		{
			con->AddIdleCB(15, [](const TcpConnPtr &con)
			{
				//cout << "idle for 15 seconds, close connection" << endl;
				LDebug("idle for 15 seconds, close connection, fd:{}", con->GetChannel()->fd());
				con->Close();
			});
		}
		else if (con->GetState() == con->Closed)
		{
			//取消该连接的所有定时任务
			while (!con->m_timerTasks.empty())
			{
				auto task = con->m_timerTasks.begin();
				LDebug("timer task cancel, timer id: {} {}", task->first, task->second);
				if (!base.Cancel(*task))
					LError("Can't find timer, timer id: {} {}", task->first, task->second);
				con->m_timerTasks.erase(task);
			}
		}
	});

	base.Loop();
}

void MultiEbPeriodicallySendTest()
{
	MultiBase bases(4);
	TcpServerPtr server = TcpServer::startServer(&bases, "", 50003);

	//接收消息->处理->发送消息
	server->OnConnCreate([]
	{
		TcpConnPtr con = make_shared<TcpConn>();
		//HandleRead中执行
		con->OnMsg(new LengthCodec, [](const TcpConnPtr& con, Slice msg)
		{
			if (*(int32_t*)msg.data() == MsgType::heartbeat)
			{
				Buffer buf;
				int commandId = MsgType::heartbeat;
				string tmp = "heartbeat";
				buf.append((char*)&commandId, sizeof(int)).append(tmp.data(), tmp.size());
				con->SendMsg(buf);
			}
		});
		return con;
	});

	//Handshake后定时发送任务
	server->OnConnTimer([](const TcpConnPtr& con)
	{
		//int commandId = MsgType::handOut;
		//string tmp = "business";
		//buf.append((char*)&commandId, sizeof(int)).append(tmp.data(), tmp.size());
		//TimerId timeId = con->GetBase()->RunEvery([con]()
		//{
		//	Buffer buf;
		//	//buf = GenerateData();
		//	buf = GeneratePicData();
		//	con->SendMsg(buf);
		//}, 1000);

		//HandleTimeouts中执行
		TimerId timeId = con->GetBase()->RunAfter(1000, [con]()
		{
			con->GetBase()->SafeCall([con]
			{
				Buffer buf;
				buf = GenerateData();
				//buf = GeneratePicData();
				con->SendMsg(buf);
			});
		}, 1000);

		//LDebug("start timer task, timer id: {} {}", timeId.first, timeId.second);
		//con->m_timerTasks.push_back(std::move(TimerId(task)));
		con->m_timerTasks.emplace_back(timeId);
	});

	//创建连接时添加心跳，关闭连接时取消定时任务，Cleanup中清理心跳
	//TODO: 定时任务的取消应该移到Clenaup
	//HandleRead中执行
	server->OnConnState([](const TcpConnPtr &con)
	{
		if (con->GetState() == con->Connected)
		{
			con->AddIdleCB(60, [](const TcpConnPtr &con)
			{
				//cout << "idle for 15 seconds, close connection" << endl;
				LDebug("idle for 15 seconds, close connection, fd:{}, id:{}", con->GetChannel()->fd(), con->GetChannel()->id());
				con->Close();
			});
		}
		else if (con->GetState() == con->Closed)
		{
			//取消该连接的所有定时任务
			while (!con->m_timerTasks.empty())
			{
				auto task = con->m_timerTasks.begin();
				LDebug("timer task cancel, timer id: {} {}", task->first, task->second);
				if (!con->GetBase()->Cancel(*task))
					LError("Can't find timer, timer id: {} {}", task->first, task->second);
				con->m_timerTasks.erase(task);
			}
		}
	});

	bases.Loop();
}

void OneIoMultiWork()
{
	EventBase base;
	TcpServerPtr server = TcpServer::startServer(&base, "", 50003);

	//接收消息->处理->发送消息
	server->OnConnCreate([]
	{
		TcpConnPtr con = make_shared<TcpConn>();
		con->OnMsg(new LengthCodec, [](const TcpConnPtr& con, Slice msg)
		{
			Buffer buf;
			buf.append(msg);
			//投入线程池异步处理
			con->GetBase()->m_upConnThreadPool->AddTask(con->GetChannel()->id(),
				[con, buf]()
			{
				//通道为nullptr时证明连接已经关闭，不再发送数据
				if (con->GetChannel())
				{
					LDebug("SendMsg after HandleRead, fd:{}, thread id:{}", con->GetChannel()->fd(), std::hash<std::thread::id>{}(std::this_thread::get_id()));
					if (*(int32_t*)buf.data() == MsgType::heartbeat)
					{
						Buffer buf;
						int commandId = MsgType::heartbeat;
						string tmp = "heartbeat";
						buf.append((char*)&commandId, sizeof(int)).append(tmp.data(), tmp.size());
						con->SendMsg(buf);
					}
				}
			});
		});
		return con;
	});

	//定时将发送任务投入工作线程中
	server->OnConnTimer([](const TcpConnPtr& con)
	{
		//连接1秒后发送，间隔1秒
		TimerId timeId = con->GetBase()->RunAfter(1000, [con]()	
		{
			//通道为nullptr时证明连接已经关闭，不再发送数据
			if (con->GetChannel())
			{
				con->GetBase()->m_upConnThreadPool->AddTask(con->GetChannel()->id(),
					[con]()
				{
					Buffer buf;
					buf = GenerateData();
					//buf = GeneratePicData();
					con->SendMsg(buf);
				});
			}
		}, 1000);

		//LDebug("start timer task, timer id: {} {}", timeId.first, timeId.second);
		con->m_timerTasks.emplace_back(timeId);
	});

	//创建连接时添加心跳，Cleanup中取消定时任务（m_stateCb）和清理心跳（UnregisterIdle）
	//HandleRead中执行，CallIdle（con->Close）在HandleTimeout中执行
	server->OnConnState([](const TcpConnPtr &con)
	{
		if (con->GetState() == con->Connected)
		{
			con->AddIdleCB(15, [](const TcpConnPtr &con)
			{
				//cout << "idle for 15 seconds, close connection" << endl;
				LDebug("idle for 15 seconds, close connection, fd:{}, id:{}", con->GetChannel()->fd(), con->GetChannel()->id());
				con->Close();
			});
		}
		else if (con->GetState() == con->Closed)
		{
			//取消该连接的所有定时任务
			while (!con->m_timerTasks.empty())
			{
				auto task = con->m_timerTasks.begin();
				LDebug("timer task cancel, timer id: {} {}", task->first, task->second);
				if (!con->GetBase()->Cancel(*task))
					LError("Can't find timer, timer id: {} {}", task->first, task->second);
				con->m_timerTasks.erase(task);
			}
		}
	});

	base.Loop();
}

class A : public Singleton<A> {
public:
	void Show() { cout << "A" << endl; }
private:
	friend Singleton<A>;
	A() {}
	~A() {}
};
class B {
public:
	void Show() { cout << "B" << endl; }
private:
	friend Singleton<B>;
	B() {}
	~B() {}
};

void TestSingleton()
{
	//A a;
	A::Instance().Show();
	Singleton<B>::Instance().Show();
}

//template<class ActualClass>
//class Singleton
//{
//public:
//	static ActualClass* GetInstance()
//	{
//		static ActualClass ActualClassDefine;
//		return &ActualClassDefine;
//	}
//
//protected:
//	Singleton() {}
//	~Singleton() {}
//
//private:
//	Singleton(Singleton const &);
//	Singleton& operator = (Singleton const &);
//};
//
//
//class Foo : public Singleton<Foo>
//{
//	friend  Singleton<Foo>;
//	Foo() {};
//	~Foo() {};
//
//};
//
//void TestSingleton2()
//{
//	//SingletonClass s;
//	Singleton<Foo>::GetInstance();
//}

//template <class ActualClass>
//class Singleton
//{
//public:
//	static ActualClass& GetInstance()
//	{
//		if (p == nullptr)
//			p = new ActualClass;
//		return *p;
//	}
//
//protected:
//	static ActualClass* p;
//	Singleton() {}
//private:
//	Singleton(Singleton const &);
//	Singleton& operator = (Singleton const &);
//};
//template <class T>
//T* Singleton<T>::p = nullptr;
//
//class Voo : public Singleton<Voo>
//{
//public:
//	void Show() { cout << "Voo" << endl; }
//private:
//	friend Singleton<Voo>;
//	Voo() {}
//	~Voo() {}
//};
//
//void TestSingleton()
//{
//	//Voo v;
//	//Voo *p = new Voo;
//	Singleton<Voo>::GetInstance().Show();
//}

//class X {
//	X();
//};
//
//void TestX()
//{
//	X* x = new X;
//}

void TestSendFrame()
{
	EventBase base;
	TcpServerPtr server = TcpServer::startServer(&base, "", 50003);
	base.Loop();
}