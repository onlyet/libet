#include <iostream>

#include "EventBase.h"
#include "PollerBase.h"
#include "TcpConn.h"
#include "Timer.h"
#include "Logger.h"

using namespace std;

EventBase::EventBase(int taskCapacity)
	: m_poller(CreatePoller()), m_exit(false), m_tasks(taskCapacity),
	m_nextTimeout(1 << 30), m_timerSeq(0), m_idleEnabled(false)
{
	//根据CPU核数创建工作线程
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	m_upConnThreadPool = make_unique<ConnThreadPool>(sysInfo.dwNumberOfProcessors);

	m_wakeupFds[0] = SOCKET_ERROR;
	m_wakeupFds[1] = SOCKET_ERROR;
	Init();
}

EventBase::~EventBase()
{
	m_upConnThreadPool->Exit();
	m_upConnThreadPool->Join();

	delete m_poller;

	::closesocket(m_wakeupFds[1]);
	WSACleanup();
}

void EventBase::AddConn(TcpConnPtr&& conn)
{
	m_conns.EmplaceBack(std::move(conn));
}

void EventBase::AddConn(const TcpConnPtr & conn)
{
	m_conns.EmplaceBack(conn);
}

void EventBase::RemoveConn(const TcpConnPtr& conn)
{
	m_conns.Remove(conn);
}

void EventBase::PostDataToOneConn(Buffer & buf, const std::weak_ptr<TcpConn>& wpConn)
{
	if (auto con = wpConn.lock())
	{
		m_upConnThreadPool->AddTask(con->GetChannel()->id(), [con, buf]() mutable
		{
			con->SendMsg(buf);
		});
	}
}

void EventBase::PostDataToAllConns(Buffer& buf)
{
	function<void(const TcpConnPtr& con)> task = [this, &buf](const TcpConnPtr& con)
	{
		m_upConnThreadPool->AddTask(con->GetChannel()->id(), [con, buf]() mutable
		{
			con->SendMsg(buf);
		});
	};

	m_conns.ForEach(task);

}

int EventBase::CreatePipe(SOCKET fds[2])
{
	SOCKET tcp1 = SOCKET_ERROR, tcp2 = SOCKET_ERROR;
	sockaddr_in name;
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	name.sin_port = 0;
	int namelen = sizeof(name);
	
	SOCKET tcp = ::socket(AF_INET, SOCK_STREAM, 0);
	if (tcp == SOCKET_ERROR) 
		goto clean;
	
	if (::bind(tcp, (sockaddr*)&name, namelen) == SOCKET_ERROR) 
		goto clean;
	
	if (::listen(tcp, 5) == SOCKET_ERROR) 
		goto clean;
	
	if (getsockname(tcp, (sockaddr*)&name, &namelen) == SOCKET_ERROR) 
		goto clean;
	
	tcp1 = ::socket(AF_INET, SOCK_STREAM, 0);
	if (tcp1 == SOCKET_ERROR) 
		goto clean;
	
	if (SOCKET_ERROR == ::connect(tcp1, (sockaddr*)&name, namelen)) 
		goto clean;
	
	tcp2 = ::accept(tcp, nullptr, nullptr);
	if (tcp2 == SOCKET_ERROR) 
		goto clean;
	
	if (::closesocket(tcp) == SOCKET_ERROR) 
		goto clean;
	
	fds[0] = tcp1;
	fds[1] = tcp2;
	return 0;
clean:
	LCritical("Create Pipe failed,code:{}", errcode);
	if (tcp != SOCKET_ERROR)
		::closesocket(tcp);
	
	if (tcp2 != SOCKET_ERROR) 
		::closesocket(tcp2);
	
	if (tcp1 != SOCKET_ERROR) 
		::closesocket(tcp1);
	
	return SOCKET_ERROR;
}

void EventBase::Init()
{
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
		return;

	if (CreatePipe(m_wakeupFds) == SOCKET_ERROR)
	{
		LCritical("CreatePipe error");
		return;
	}

	//创建唤醒通道，该通道用于唤醒I/O线程
	Channel *ch = new Channel(this, m_wakeupFds[0]);
	LInfo("wakeup fd:{}", m_wakeupFds[0]);

	ch->OnRead([ch, this]
	{
		char buf[1024];
		int r = (ch->fd() != SOCKET_ERROR) ? ::recv(ch->fd(), buf, sizeof(buf), 0) : 0;
		if (r > 0)
		{
			Task task;
			LDebug("EventBase addr: {}, m_tasks size: {}", (int32_t)this, m_tasks.size());
			while (m_tasks.pop_wait(&task, 0)) 	//从任务队列取一个任务执行
			{
				task();
			}
		}
		else if (r == 0) 	//closesocket(m_wakeupFds)后
		{
			LDebug("close wakeup fd: {}, thread id:{}", ch->fd(), std::hash<std::thread::id>{}(std::this_thread::get_id()));
			delete ch;
		}
		//else if (errno == EINTR) {
		//}
		else
			LCritical("wakeup channel recv error, ret:{},code{}", r, errcode);
	});
}

void EventBase::CallIdles()
{
	int64_t now = util::timeMilli() / 1000;
	//for (auto &l : m_idleConns)
	//{
	//	int idle = l.first;
	//	auto lst = l.second;
	//	while (lst.size())
	//	{
	//		IdleNode &node = lst.front();
	//		if (node.m_updated + idle > now)
	//			break;

	//		node.m_updated = now;
	//		lst.splice(lst.end(), lst, lst.begin());
	//		node.m_cb(node.m_conn);
	//	}
	//}
	//FIXME: 检查迭代器
	auto it = m_idleConns.begin();
	while (it != m_idleConns.end())
	{
		auto l = *it;
		++it;
		int idle = l.first;
		auto lst = l.second;
		size_t cnt = lst.size();

		while (cnt--)
		{
			IdleNode &node = lst.front();
			if (node.m_updated + idle > now)
				break;

			node.m_updated = now;
			lst.splice(lst.end(), lst, lst.begin());
			node.m_cb(node.m_conn);
		}
	}
}

IdleId EventBase::RegisterIdle(int idle, const TcpConnPtr & con, const TcpCallBack & cb)
{
	if (!m_idleEnabled) 
	{
		RunAfter(1000, [this] { CallIdles(); }, 1000);
		m_idleEnabled = true;
	}
	auto &lst = m_idleConns[idle];
	lst.push_back(IdleNode{ con, util::timeMilli() / 1000, move(cb) });
	LDebug("RegisterIdle() m_lst size: {}", lst.size());
	return IdleId(new IdleIdImp(&lst, --lst.end()));
}

void EventBase::UnregisterIdle(const IdleId & id)
{
	//m_lst是m_idleConns中的链表，当m_idleConns clear后，链表也清空了， 所有IDleId引用的链表和链表迭代器都失效了
	id->m_lst->erase(id->m_iter);
}

void EventBase::UpdateIdle(const IdleId & id)
{
	id->m_iter->m_updated = util::timeMilli() / 1000;
	id->m_lst->splice(id->m_lst->end(), *id->m_lst, id->m_iter);	//将m_iter拼接到链表尾部
}

void EventBase::HandleTimeouts()
{
	int64_t now = util::timeMilli();
	TimerId tid{ now, 1LL << 62 };
	while (m_timers.size() && m_timers.begin()->first < tid)
	{
		Task task = move(m_timers.begin()->second);
		m_timers.erase(m_timers.begin());
		task();
	}
	RefreshNearest();
}

void EventBase::RefreshNearest(const TimerId * tid)
{
	if (m_timers.empty())
		m_nextTimeout = 1 << 30;
	else
	{
		const TimerId &t = m_timers.begin()->first;
		m_nextTimeout = t.first - util::timeMilli();
		m_nextTimeout = m_nextTimeout < 0 ? 0 : m_nextTimeout;
	}
}

void EventBase::RepeatableTimeout(TimerRepeatable * tr)
{
	tr->m_at += tr->m_interval;
	tr->m_timerid = { tr->m_at, ++m_timerSeq };
	m_timers[tr->m_timerid] = [this, tr] { RepeatableTimeout(tr); };
	RefreshNearest(&tr->m_timerid);
	tr->m_cb();
}

void EventBase::LoopOnce(int waitMs)
{
	m_poller->LoopOnce((std::min)(waitMs, m_nextTimeout));
	HandleTimeouts();
}

bool EventBase::Cancel(TimerId timerid)
{
	if (timerid.first < 0)	//重复任务
	{
		//
		auto p = m_timerReps.find(timerid);
		if (p != m_timerReps.end())
		{
			auto ptimer = m_timers.find(p->second.m_timerid);
			if (ptimer != m_timers.end())
				m_timers.erase(ptimer);
			m_timerReps.erase(p);
			return true;
		}
	}
	else	//一次性任务
	{
		auto p = m_timers.find(timerid);
		if (p != m_timers.end())
		{
			m_timers.erase(p);
			return true;
		}
	}
	return false;
}


void EventBase::Loop()
{
	while (!m_exit)
		LoopOnce(3000);

	m_timerReps.clear();
	m_timers.clear();

	/*
	 * TcpConn::m_idleIds::m_lst是m_idleConns中的链表
	 * 当m_idleConns 调用clear()后，链表清空了，所有IDleId引用的链表和链表迭代器都失效了,导致list崩溃
	 * 为了方便维护，此处注释掉clear
	*/
	//m_idleConns.clear();

	m_conns.Clear();

	LoopOnce(0);
}

EventBase & EventBase::Exit()
{
	m_exit = true;
	return *this;
}

bool EventBase::Exited()
{
	return m_exit;
}

void EventBase::Wakeup()
{
	int ret = ::send(m_wakeupFds[1], "", 1, 0);
	if (ret < 0)
		LCritical("Wakeup send error code:{}", errcode);
}

void EventBase::SafeCall(Task && task)
{
	m_tasks.push(move(task));
	//LDebug("m_tasks size: {}", m_tasks.size());
	Wakeup();
}

TimerId EventBase::RunAt(int64_t milli, Task && task, int64_t interval)
{
	if (m_exit)
	{
		return TimerId();
	}
	if (interval)
	{
		TimerId tid{ -milli, ++m_timerSeq };
		TimerRepeatable &rtr = m_timerReps[tid];
		rtr = { milli, interval,{ milli, ++m_timerSeq }, move(task) };
		TimerRepeatable *tr = &rtr;
		m_timers[tr->m_timerid] = [this, tr] { RepeatableTimeout(tr); };
		RefreshNearest(&tr->m_timerid);
		return tid;
	}
	else
	{
		TimerId tid{ milli, ++m_timerSeq };
		m_timers.insert({ tid, move(task) });
		RefreshNearest(&tid);
		return tid;
	}
}

EventBase * MultiBase::AllocBase()
{
	int c = m_id++;
	return &m_bases[c % m_bases.size()];
}

void MultiBase::Loop()
{
	int sz = m_bases.size();
	vector<thread> ths(sz - 1);
	for (int i = 0; i < sz - 1; i++)
	{
		thread t([this, i] { m_bases[i].Loop(); });
		ths[i].swap(t);
	}
	m_bases.back().Loop();
	for (int i = 0; i < sz - 1; i++)
		ths[i].join();
}

MultiBase & MultiBase::Exit()
{
	for (auto &b : m_bases)
		b.Exit();
	return *this;
}


Channel::Channel(EventBase* base, int fd) :
	m_base(base), m_fd(fd),
	m_readable(true), m_writable(false)
{
	static atomic<int64_t> id(0);
	m_id = ++id;
	m_base->m_poller->AddChannel(this);
}

Channel::~Channel()
{
	LDebug("~Channel(), fd:{}", m_fd);
	Close();
}

void Channel::Close()
{
	if (m_fd != SOCKET_ERROR)
	{
		m_base->m_poller->RemoveChannel(this);
		::shutdown(m_fd, SD_SEND);
		::closesocket(m_fd);
		m_fd = SOCKET_ERROR;
		HandleRead();	
	}
}

void Channel::EnableRead(bool enable)
{
	enable == true ? m_readable = true : m_readable = false;
	m_base->m_poller->UpdateChannel(this);
}

void Channel::EnableWrite(bool enable)
{
	enable == true ? m_writable = true : m_writable = false;
	m_base->m_poller->UpdateChannel(this);
}