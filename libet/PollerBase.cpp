#include <iostream>
#include <WinSock2.h>

#include "PollerBase.h"
#include "EventBase.h"
#include "Logger.h"

using namespace std;

struct PollerSelect : public PollerBase
{
	using fd_set_ptr = fd_set*;

	PollerSelect();
	~PollerSelect();

	void AddChannel(Channel* ch) override;
	void RemoveChannel(Channel* ch) override;
	void UpdateChannel(Channel* ch) override;
	void LoopOnce(int waitMs) override;

public:
	//非线程安全，不能用static fd_set
	void SetParam(fd_set_ptr& readfds, fd_set_ptr& writefds, fd_set_ptr& exceptfds);

public:
	list<Channel*> m_liveChannels;
	list<Channel*>::iterator curIt;

	fd_set_ptr m_allReadSet;
	fd_set_ptr m_allWriteSet;
	fd_set_ptr m_allExceptSet;
	fd_set_ptr readSet;
	fd_set_ptr writeSet;
	fd_set_ptr exceptSet;

	struct timeval m_timeout;
};

PollerBase* CreatePoller()
{
	return new PollerSelect();
}

PollerSelect::PollerSelect() :
	m_allReadSet(new fd_set()), m_allWriteSet(new fd_set()), m_allExceptSet(new fd_set()),
	readSet(new fd_set()), writeSet(new fd_set()), exceptSet(new fd_set()),
	curIt(m_liveChannels.end())
{
	FD_ZERO(m_allReadSet);
	FD_ZERO(m_allWriteSet);
	FD_ZERO(m_allExceptSet);
}

PollerSelect::~PollerSelect()
{
	LInfo("destroying poller {}, thread id:{}", m_id, std::hash<std::thread::id>{}(std::this_thread::get_id()));
	if (curIt != m_liveChannels.end())
	{
		while (m_liveChannels.size())
		{
			auto it = m_liveChannels.begin();
			(*it)->Close();
		}
	}
	LInfo("poller destroyed, thread id:{}", m_id, std::hash<std::thread::id>{}(std::this_thread::get_id()));

	if (m_allReadSet)
	{
		delete m_allReadSet;
		m_allReadSet = nullptr;
	}
	if (m_allWriteSet)
	{
		delete m_allWriteSet;
		m_allWriteSet = nullptr;
	}
	if (m_allExceptSet)
	{
		delete m_allExceptSet;
		m_allExceptSet = nullptr;
	}
	if (readSet)
	{
		delete readSet;
		readSet = nullptr;
	}
	if (writeSet)
	{
		delete writeSet;
		writeSet = nullptr;
	}
	if (exceptSet)
	{
		delete exceptSet;
		exceptSet = nullptr;
	}
}

void PollerSelect::AddChannel(Channel* ch)
{
	if (ch->Readable())
		FD_SET(ch->fd(), m_allReadSet);
	if (ch->Writable())
		FD_SET(ch->fd(), m_allWriteSet);
	//m_liveChannels.insert(ch);
	m_liveChannels.emplace_back(ch);
}

void PollerSelect::RemoveChannel(Channel* ch)
{
	FD_CLR(ch->fd(), m_allReadSet);
	FD_CLR(ch->fd(), m_allWriteSet);
	//ch是当前通道，curIt是当前通道的下一个通道
	if (curIt != m_liveChannels.end() && *curIt == ch)
		++curIt;
	m_liveChannels.remove(ch);
}

void PollerSelect::UpdateChannel(Channel* ch)
{
	if (ch->Readable())
		FD_SET(ch->fd(), m_allReadSet);
	else
		FD_CLR(ch->fd(), m_allReadSet);

	if (ch->Writable())
		FD_SET(ch->fd(), m_allWriteSet);
	else
		FD_CLR(ch->fd(), m_allWriteSet);
}

void PollerSelect::LoopOnce(int waitMs)
{
	int nfds = 0; //windows忽略
	//fd_set *readfds, *writefds, *exceptfds;
	//SetParam(readfds, writefds, exceptfds);
	fd_set rs = *m_allReadSet;
	fd_set ws = *m_allWriteSet;
	fd_set es = *m_allExceptSet;

	//TODO 提供接口设置timeout
	m_timeout.tv_sec = waitMs / 1000;
	m_timeout.tv_usec = waitMs % 1000 * 1000;
	//m_lastActive = ::select(nfds, readfds, writefds, exceptfds, &m_timeout);
	m_lastActive = ::select(nfds, &rs, &ws, &es, &m_timeout);

	criticalif(m_lastActive == SOCKET_ERROR, "select waitMs:{}, ret:{}, code:{}", waitMs, m_lastActive, errcode);
	//if (m_lastActive == 0)	//超时
	//	LTrace("select time out, waitMs:{}", waitMs);

	curIt = m_liveChannels.begin();
	while (m_lastActive > 0 && curIt != m_liveChannels.end())
	{
		auto ch = *curIt;
		++curIt;
		bool dec = false;

		if (FD_ISSET(ch->fd(), &rs))
		{
			//LDebug("Read event triggled fd:{}, thread id:{}", ch->fd(), std::hash<std::thread::id>{}(std::this_thread::get_id()));
			ch->HandleRead();
			//ch->GetBase()->m_upConnThreadPool->AddTask(ch->id(), [=]() { ch->HandleRead(); });
			dec = true;
		}
		if (FD_ISSET(ch->fd(), &ws))
		{
			//LDebug("Write event triggled fd:{} triggled, thread id:{}", ch->fd(), std::hash<std::thread::id>{}(std::this_thread::get_id()));
			ch->HandleWrite();
			//ch->GetBase()->m_upConnThreadPool->AddTask(ch->id(), [=]() { ch->HandleWrite(); });
			dec = true;
		}
		if (dec && --m_lastActive <= 0)
			break;
	}
}

void PollerSelect::SetParam(fd_set_ptr & readfds, fd_set_ptr & writefds, fd_set_ptr & exceptfds)
{
	*readSet = *m_allReadSet;
	*writeSet = *m_allWriteSet;
	*exceptSet = *m_allExceptSet;

	m_allReadSet->fd_count ? (readfds = readSet) : (readfds = nullptr);
	m_allWriteSet->fd_count ? (writefds = writeSet) : (writefds = nullptr);
	m_allExceptSet->fd_count ? (exceptfds = exceptSet) : (exceptfds = nullptr);
}

