#pragma once
#include <memory>
#include <functional>
#include <list>

struct TcpConn;
struct TcpServer;
struct EventBase;
struct EventBases;
struct PollerBase;
struct Channel;
struct Slice;
struct CodecBase;

using TcpServerPtr = std::unique_ptr<TcpServer>;
using TcpConnPtr = std::shared_ptr<TcpConn>;
using TcpCallBack = std::function<void(const TcpConnPtr &)>;
using MsgCallBack = std::function<void(const TcpConnPtr&, Slice)>;
using TimerId = std::pair<int64_t, int64_t>;
using Task = std::function<void()>;

struct TimerRepeatable
{
	int64_t m_at;  // current timer timeout timestamp
	int64_t m_interval;
	TimerId m_timerid;
	Task m_cb;
};

struct IdleNode	//闲置结点
{
	TcpConnPtr m_conn;
	int64_t m_updated;	//秒
	TcpCallBack m_cb;
};

struct IdleIdImp	//维护一个闲置结点
{
	typedef std::list<IdleNode>::iterator Iter;
	IdleIdImp() {}
	IdleIdImp(std::list<IdleNode> *lst, Iter iter) : m_lst(lst), m_iter(iter) {}
	Iter m_iter;	//指向给定时间点下的当前闲置结点
	std::list<IdleNode> *m_lst;	//指向给定时间点下的闲置链表
};
using IdleId = std::unique_ptr<IdleIdImp>;	//标识给定时间点下的一个闲置结点