#pragma once
#include <memory>
#include <WinSock2.h>

#include "Global.h"
#include "Threads.h"
#include "Buffer.h"

struct EventBases : private noncopyable 
{
	virtual EventBase* AllocBase() = 0;
};

//事件管理器
struct EventBase : public EventBases
{
	EventBase(int taskCapacity = 0);
	~EventBase();

	//进入事件处理循环
	void Loop();
	void LoopOnce(int waitMs);
	//取消定时任务，若timer已经过期，则忽略
	bool Cancel(TimerId timerid);
	//定时任务,任务在HandleTimeouts中执行
	TimerId RunAt(int64_t milli, const Task &task, int64_t interval = 0) { return RunAt(milli, Task(task), interval); }
	TimerId RunAt(int64_t milli, Task &&task, int64_t interval = 0);
	TimerId RunAfter(int64_t milli, const Task &task, int64_t interval = 0) { return RunAt(util::timeMilli() + milli, Task(task), interval); }
	TimerId RunAfter(int64_t milli, Task &&task, int64_t interval = 0) { return RunAt(util::timeMilli() + milli, std::move(task), interval); }
	TimerId RunEvery(const Task &task, int64_t interval = 0) { return RunAt(util::timeMilli(), Task(task), interval); }
	TimerId RunEvery(Task &&task, int64_t interval = 0) { return RunAt(util::timeMilli(), std::move(task), interval); }

	//退出事件循环
	EventBase& Exit();
	bool Exited();
	void Wakeup();
	void SafeCall(Task &&task);
	void SafeCall(const Task &task) { SafeCall(Task(task)); }
	virtual EventBase* AllocBase() override { return this; }	

	void AddConn(TcpConnPtr&& conn);
	void AddConn(const TcpConnPtr& conn);
	void RemoveConn(const TcpConnPtr& conn);

	void PostDataToOneConn(Buffer& buf, const std::weak_ptr<TcpConn>& wpConn);
	void PostDataToAllConns(Buffer& buf);


public:
	int CreatePipe(SOCKET fds[2]);
	void Init();
	//闲置时间到达的连接执行回调任务,在HandleTimeouts中执行
	void CallIdles();

	IdleId RegisterIdle(int idle, const TcpConnPtr &con, const TcpCallBack &cb);
	void UnregisterIdle(const IdleId &id);
	void UpdateIdle(const IdleId &id);
	void HandleTimeouts();
	void RefreshNearest(const TimerId *tid = nullptr);
	void RepeatableTimeout(TimerRepeatable *tr);

	//void SendThreadProc();

public:
	PollerBase* m_poller;
	std::atomic<bool> m_exit;
	SOCKET m_wakeupFds[2];
	int m_nextTimeout;
	SafeQueue<Task> m_tasks;
	std::unique_ptr<ConnThreadPool> m_upConnThreadPool;

	std::map<TimerId, TimerRepeatable> m_timerReps;
	std::map<TimerId, Task> m_timers;
	std::atomic<int64_t> m_timerSeq;	//定时器序号
	// 记录每个idle时间点（单位秒）下所有的连接。链表中的所有连接，最新的插入到链表末尾。连接若有活动，会把连接从链表中移到链表尾部，做法参考memcache
	std::map<int, std::list<IdleNode>> m_idleConns;
	bool m_idleEnabled;

	SafeList<TcpConnPtr> m_conns;
};

//多线程的事件派发器
struct MultiBase : public EventBases 
{
	MultiBase(int sz) : m_id(0), m_bases(sz) {}
	virtual EventBase *AllocBase() override;
	void Loop();
	MultiBase &Exit();

private:
	std::atomic<int> m_id;
	std::vector<EventBase> m_bases;
};

struct Channel : private noncopyable
{
	Channel(EventBase* base, int fd);
	~Channel();

	EventBase* GetBase() { return m_base; }
	SOCKET fd() { return m_fd; }
	int64_t id() { return m_id; }

	/* 
	 * 被动关闭连接：HandleRead->Cleanup->~Channel->Channel::Close->HandleRead->~TcpConn
	 * 主动关闭连接：TcpConn::Close->Channel::Close->HandleRead->Cleanup->~Channel->Channel::Close->~TcpConn
	 * TryDecode失败：Channel::Close->HandleRead->Cleanup->~Channel->Channel::Close->~TcpConn
	*/
	void Close();

	//挂接事件处理器
	void OnRead(const Task &readcb) { m_readCb = readcb; }
	void OnWrite(const Task &writecb) { m_writeCb = writecb; }
	void OnRead(Task &&readcb) { m_readCb = std::move(readcb); }
	void OnWrite(Task &&writecb) { m_writeCb = std::move(writecb); }

	//处理读写事件
	void HandleRead() { m_readCb(); }
	void HandleWrite() { m_writeCb(); }

	void EnableRead(bool enable);
	void EnableWrite(bool enable);
	bool Readable() { return m_readable; }
	bool Writable() { return m_writable; }

protected:
	SOCKET m_fd;							
	EventBase* m_base;
	int64_t m_id;
	bool m_readable, m_writable;
	std::function<void()> m_readCb, m_writeCb, m_errorCb;
};