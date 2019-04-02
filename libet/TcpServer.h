#pragma once
#include <WinSock2.h>
#include <memory>
#include <assert.h>

#include "Global.h"
#include "Codec.h"
#include "Net.h"
#include "Util.h"

struct TcpServer : private noncopyable
{
public:
	TcpServer(EventBases* bases);
	~TcpServer();

	bool Bind(const std::string &host, USHORT port);
	bool Bind(USHORT port) { Bind(INADDR_ANY, port); }

	static TcpServerPtr startServer(EventBases* bases, const std::string &host, int port);

	void OnConnCreate(const std::function<TcpConnPtr()>& cb) { m_createCb = cb; }
	void OnConnState(const TcpCallBack& cb) { m_stateCb = cb; }
	void OnConnRead(const TcpCallBack& cb) { m_readCb = cb; }
	// 消息处理与Read回调冲突，只能调用一个
	void OnConnMsg(std::unique_ptr<CodecBase> codec, const MsgCallBack& cb) {
		m_codec.swap(codec);
		m_msgCb = cb;
		assert(!m_readCb);
	}

	void OnConnTimer(const TcpCallBack& cb) { m_timerCb = cb; }

private:
	bool HandleAccept();

private:
	EventBase* m_base;
	EventBases* m_bases;
	Channel* m_listenChannel;
	Ip4Addr m_addr;
	TcpCallBack m_stateCb, m_readCb, m_timerCb;
	MsgCallBack m_msgCb;
	std::function<TcpConnPtr()> m_createCb;	//用于创建TcpConn
	std::unique_ptr<CodecBase> m_codec;
};