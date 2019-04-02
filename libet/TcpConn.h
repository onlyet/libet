#pragma once
#include <assert.h>
#include <atomic>

#include "Global.h"
#include "Buffer.h"
#include "Net.h"
#include "Util.h"

struct TcpConn : public std::enable_shared_from_this<TcpConn>, private noncopyable
{
	enum State {
		Invalid = 1,
		Handshaking,
		Connected,
		Closed,
		Failed,
	};
	// Tcp构造函数，实际可用的连接应当通过createConnection创建
	TcpConn();
	virtual ~TcpConn();

	EventBase* GetBase() { return m_base; }
	Buffer& GetInput() { return m_input; }
	Buffer& GetOutput() { return m_output; }
	Channel* GetChannel() { return m_channel; }
	std::atomic_int& GetState() { return m_state; }

	//数据到达时回调（HandleRead）
	void OnRead(const TcpCallBack &cb) {
		assert(!m_readCb);
		m_readCb = cb;
	};
	//当tcp缓冲区可写时回调
	void OnWrite(const TcpCallBack &cb) { m_writeCb = cb; }
	// tcp状态改变时回调
	void OnState(const TcpCallBack &cb) { m_stateCb = cb; }
	// tcp空闲回调
	void AddIdleCB(int idle, const TcpCallBack &cb);

	//消息回调，此回调与OnRead回调冲突，只能够调用一个, （调用OnRead）
	void OnMsg(CodecBase* codec, const MsgCallBack& cb);

	//定时任务
	void OnTimer(const TcpCallBack& cb) { m_timerCb = cb; }

	// conn会在下个事件周期进行处理
	void Close();

	//创建连接通道，连接通道注册read/write事件，连接通道添加到PollerBase
	void Attach(EventBase* base, int fd, Ip4Addr local, Ip4Addr peer);

	//发送数据，非线程安全
	void Send(const char *buf, size_t len);
	void Send(const char *s) { Send(s, strlen(s)); }
	void Send(const std::string &s) { Send(s.data(), s.size()); }
	void Send(Buffer & buf);
	void SendOutput() { Send(m_output); }

	//发送消息，线程安全
	//void SendMsg(Slice msg);
	void SendMsg(const Slice msg);
	//void SendMsg(Buffer buf);

public:
	int HandleHandshake(const TcpConnPtr &con);
	void HandleRead(const TcpConnPtr &con);
	/* 在发送数据的时候，先直接Send，剩余没发送的数据在HandleWrite中发送，
	如果没发送完会一直触发HandleWrite，发送完后关闭写通道 */
	void HandleWrite(const TcpConnPtr &con);

	//析构m_spChannel
	void Cleanup(const TcpConnPtr &con);
	int SendBase(const char * buf, size_t len);

public:
	EventBase* m_base;
	Channel* m_channel;	//连接通道
	Buffer m_input, m_output;

	Ip4Addr m_local, m_peer;
	std::atomic_int m_state;
	TcpCallBack m_readCb, m_writeCb, m_stateCb;
	TcpCallBack m_timerCb;
	std::list<IdleId> m_idleIds;	//维护所有时间点下的闲置任务
	TimerId m_timeoutId;	//未使用
	std::list<TimerId> m_timerTasks; //维护所有定时任务
	std::unique_ptr<CodecBase> m_codec;
};