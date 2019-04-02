#pragma once
#include <iostream>

#include "TcpServer.h"
#include "TcpConn.h"
#include "EventBase.h"
#include "PollerBase.h"
#include "Logger.h"

using namespace std;

TcpServer::TcpServer(EventBases* bases) : 
	m_base(bases->AllocBase()),
	m_bases(bases),
	m_listenChannel(nullptr),
	m_createCb([] { return make_shared<TcpConn>(); })
{
}

TcpServer::~TcpServer()
{
	LDebug("~TcpServer() begin");
	delete m_listenChannel;
	LDebug("~TcpServer() end");
}

bool TcpServer::Bind(const std::string &host, USHORT port)
{
	Ip4Addr m_addr(host, port);
	SOCKET listenFd;
	if ((listenFd = ::socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		return false;

	int bufSize = 10 * 1000 * 1000;
	int len = sizeof(int);
	setsockopt(listenFd, SOL_SOCKET, SO_RCVBUF, (char*)&bufSize, len);
	setsockopt(listenFd, SOL_SOCKET, SO_SNDBUF, (char*)&bufSize, len);

	//将fd设为非阻塞
	if (Net::SetNonBlocking(listenFd))
		return false;

	if (::bind(listenFd, (sockaddr*)&m_addr.getAddr(), sizeof(sockaddr)) == SOCKET_ERROR)
	{
		LError("bind() failed, addr: {}:{}", m_addr.ip(), m_addr.port());
		return false;
	}

	if (::listen(listenFd, SOMAXCONN) == SOCKET_ERROR)
		return false;

	LInfo("listen fd: {}", listenFd);
	m_listenChannel = new Channel(m_base, listenFd);		//创建监听通道
	m_listenChannel->OnRead([this] { HandleAccept(); });	//监听通道注册读事件
	return true;
}

bool TcpServer::HandleAccept()
{ 
	if (m_listenChannel->fd() == SOCKET_ERROR)
		return false;
	LDebug("HandleAccept, listen fd:{}, thread id:{}", m_listenChannel->fd(), std::hash<std::thread::id>{}(std::this_thread::get_id()));
	SOCKET connFd;
	sockaddr_in clntAddr = { 0 };
	int clntAddrLen = sizeof(clntAddr);

	while ((connFd = ::accept(m_listenChannel->fd(), (SOCKADDR*)&clntAddr, &clntAddrLen)) != SOCKET_ERROR)
	{
		Net::SetNonBlocking(connFd);

		sockaddr_in peer, local;
		int peerLen = sizeof(peer);
		int r = getpeername(connFd, (sockaddr *)&peer, &peerLen);
		if (r < 0)
		{
			//error("get peer name failed %d %s", errno, strerror(errno));
			continue;
		}
		r = getsockname(connFd, (sockaddr *)&local, &peerLen);
		if (r < 0)
		{
			//error("getsockname failed %d %s", errno, strerror(errno));
			continue;
		}
		EventBase *b = m_bases->AllocBase();	//Multibase为轮询调度分配EventBase
		auto addcon = [=]
		{
			TcpConnPtr con = m_createCb();	//构造TCP连接

			con->Attach(b, connFd, local, peer);

			if (m_stateCb)
				con->OnState(m_stateCb);	//将TcpServer的状态回调传给TcpConn

			if (m_readCb)
				con->OnRead(m_readCb);	//将TcpServer的读回调传给TcpConn

			if (m_msgCb)
				con->OnMsg(m_codec->Clone(), m_msgCb);	//将TcpServer的消息回调传给TcpConn

			if (m_timerCb)
				con->OnTimer(m_timerCb);	//将TcpServer的定时任务回调传给TcpConn

		};
		if (b == m_base)
			addcon();
		else
			b->SafeCall(std::move(addcon));
	}
	if (errcode != WSAEWOULDBLOCK)
		LWarn("accept error, return:{}, code:{}", connFd, errcode);

	return true;
}

TcpServerPtr TcpServer::startServer(EventBases* bases, const std::string & host, int port)
{
	TcpServerPtr p = make_unique<TcpServer>(bases);
	bool r = p->Bind(host, port);
	if (!r)
		LCritical("bind to {}:{} failed, code:{}", host, port, errcode);

	return r == true ? std::move(p) : nullptr;
}

