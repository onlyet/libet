#include <iostream>

#include "TcpConn.h"
#include "Codec.h"
#include "EventBase.h"
#include "Timer.h"
#include "Logger.h"

using namespace std;

TcpConn::TcpConn():
	m_base(nullptr), m_channel(nullptr), m_codec(nullptr),
	m_state(State::Invalid)
{
}

TcpConn::~TcpConn()
{
	LDebug("tcp connection destroyed {} - {}", m_local.toString().c_str(), m_peer.toString().c_str());
	delete m_channel;
}

void TcpConn::AddIdleCB(int idle, const TcpCallBack & cb)
{
	LTrace("AddIdleCB");
	if (m_channel)
		m_idleIds.push_back(m_base->RegisterIdle(idle, shared_from_this(), cb));
}

void TcpConn::OnMsg(CodecBase* codec, const MsgCallBack & cb)
{
	assert(!m_readCb);	//读回调与消息回调不能共存
	m_codec.reset(codec);	
	OnRead([cb](const TcpConnPtr& con)
	{
		int ret = 1;
		while (ret)
		{
			Slice msg;
			ret = con->m_codec->TryDecode(con->m_input, msg);
			if (ret < 0)
			{
				LDebug("OnMsg Decode failed");
				con->m_channel->Close();
				break;
			}
			else if (ret > 0)
			{
				cb(con, msg);
				con->m_input.consume(ret);
			}
		}
	});
}

void TcpConn::SendMsg(const Slice msg)
{
	//lock_guard<mutex> lock(m_mtxOutput);
	m_codec->Encode(msg, GetOutput());
	SendOutput();
}

//void TcpConn::SendMsg(Buffer buf)
//{
//	//int t1 = *(int32_t*)msg.begin();
//	//string t2 = string(msg.begin() + 4, msg.size() - 4);
//	//LDebug("commandId:{}, msg:{}", t1, t2);
//	//Slice msg(buf.begin(), buf.end());
//	m_codec->Encode(Slice(buf.begin(), buf.end()), GetOutput());
//	SendOutput();
//}

void TcpConn::Close()
{
	if (m_channel) 
	{
		TcpConnPtr con = shared_from_this();
		m_base->SafeCall([con] 
		{
			if (con->m_channel)
				con->m_channel->Close();
		});
	}
	//if (m_channel)
	//{
	//	m_channel->Close();
	//}
}

int TcpConn::HandleHandshake(const TcpConnPtr & con)
{
	LDebug("HandleHandshake");
	if (m_state != State::Handshaking)
	{
		LCritical("HandleHandshake called when state = {}", m_state);
		return -1;
	}
	//fatalif(state_ != Handshaking, "handleHandshaking called when state_=%d", state_);
	fd_set writeSet;
	FD_ZERO(&writeSet);
	FD_SET(m_channel->fd(), &writeSet);
	timeval timeout = { 0 };
	int ret = select(m_channel->fd(), nullptr, &writeSet, nullptr, &timeout);
	if (ret == 1)
	{
		m_state = State::Connected;
		//connectedTime_ = util::timeMilli();
		LDebug("tcp connected {} - {} fd:{}", m_local.toString().c_str(), m_peer.toString().c_str(), m_channel->fd());
		if (m_stateCb)
			m_stateCb(con);

		if (m_timerCb)	//连接成功则创建定时发送任务
			m_timerCb(con);
	}
	else 
	{
		Cleanup(con);
		return -1;
	}
	return 0;
}

void TcpConn::HandleRead(const TcpConnPtr & con)
{
	//LDebug("HandleRead! thread id: {}", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	Timer t;
	if (m_state == State::Handshaking && HandleHandshake(con))
		return;

	while (m_state == State::Connected)
	{
		int nrecv = 0;
		m_input.makeRoom();	//如果buffer容量不够，则增大
		if (m_channel->fd() != SOCKET_ERROR)
		{
			t.start();
			nrecv = recv(m_channel->fd(), m_input.end(), m_input.space(), 0);
			LTrace("channel fd:{} readed {} bytes", (long long)m_channel->id(), m_channel->fd(), nrecv);
		}
		if (nrecv == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			LTrace("recv() succeed, no more data to read, already received {} bytes", m_input.size());
			for (auto& idle : m_idleIds) 
				m_base->UpdateIdle(idle);
			
			if (m_readCb && m_input.size())  
			{
				m_readCb(con);	//调用TcpServer传来的读回调
				LTrace("recv() and send() duration: {}ms", t.delta<Timer::ms>());
			}
			break;
		}
		else if (m_channel->fd() == SOCKET_ERROR || nrecv == 0 || nrecv == -1)
		{
			if (m_channel->fd() == SOCKET_ERROR)	//已经主动关闭通道
				LDebug("Channel already closed");
			else if (nrecv == 0)
				LDebug("client fd:{} closed the connection, code:{}", m_channel->fd(), errcode);
			else
				LInfo("recv error code:{}", errcode);
			Cleanup(con);
			//Clean后TcpConn已经析构
			break;
		}
		else
			m_input.addSize(nrecv);
	}
}

void TcpConn::HandleWrite(const TcpConnPtr & con)
{
	if (m_state == State::Handshaking)
		HandleHandshake(con);
	else if (m_state == State::Connected)
	{
		int sended = SendBase(m_output.begin(), m_output.size());
		m_output.consume(sended);
		if (m_output.empty() && m_writeCb)	//处理TcpServer传来的写回调
			m_writeCb(con);

		if (m_output.empty() && m_channel->Writable())  // 写完数据关闭写通道
			m_channel->EnableWrite(false);
	}
	else
		LError("Handlewrite unexpected");
}

void TcpConn::Attach(EventBase* base, int fd, Ip4Addr local, Ip4Addr peer)
{
	m_base = base;
	m_state = State::Handshaking;
	m_local = local;
	m_peer = peer;
	if (m_channel)
		delete m_channel;
	m_channel = new Channel(base, fd);	//创建连接通道
	LDebug("tcp constructed {} - {} fd:{}",
		m_local.toString(),
		m_peer.toString(),
		fd);

	TcpConnPtr con = shared_from_this();
	con->m_channel->OnRead([=] {con->HandleRead(con); });	//连接通道注册读事件，即TcpConn的HandleRead
	con->m_channel->OnWrite([=] {con->HandleWrite(con); }); // 连接通道注册写事件，即TcpConn的Handlewrite

	con->GetBase()->AddConn(con);
}

void TcpConn::Cleanup(const TcpConnPtr & con)
{
	{
		if (m_readCb && m_input.size())
			m_readCb(con);
	}

	if (m_state == State::Handshaking) 
		m_state = State::Failed;
	else 
		m_state = State::Closed;
	

	LDebug("tcp closing {} - {} fd:{}, code:{}", m_local.toString(), m_peer.toString(), m_channel ? m_channel->fd() : SOCKET_ERROR, errcode);
	//取消任务暂时不用
	//m_base->Cancel(m_timeoutId);
	if (m_stateCb)
		m_stateCb(con);

	//if (reconnectInterval_ >= 0 && !getBase()->exited()) 
	//{  // reconnect
	//	reconnect();
	//	return;
	//}
	for (auto &idle : m_idleIds) 
		m_base->UnregisterIdle(idle);
	//while (!m_idleIds.empty())
	//{
	//	auto &idleId = m_idleIds.begin();
	//	m_base->UnregisterIdle(*idleId);
	//	m_idleIds.erase(idleId);
	//}

	m_base->RemoveConn(con);
	
	m_readCb = m_writeCb = m_stateCb =  m_timerCb = nullptr;
	Channel *ch = m_channel;
	m_channel = nullptr;
	//LDebug("2 TcpConn use_count:{}", con.use_count());
	delete ch;	//通道析构后TcpConn随之析构
}

int TcpConn::SendBase(const char* buf, size_t len)
{
	int nsend;
	size_t sended = 0;
	while (len > sended)
	{
		nsend = ::send(m_channel->fd(), buf + sended, len - sended, 0);
		//trace("channel %lld fd %d write %ld bytes", (long long)m_channel->id(), m_channel->fd(), wd);
		if (nsend > 0)
		{
			sended += nsend;
			continue;
		}
		else if (nsend == SOCKET_ERROR && (WSAGetLastError() == WSAEWOULDBLOCK))
		{
			LDebug("send WSAEOULDBLOCK");
			if (!m_channel->Writable())
				m_channel->EnableWrite(true);

			break;
		}
		else
		{
			LError("send error: channel fd:{}, ret:{}, code:{}", m_channel->fd(), nsend, errcode);
			break;
		}
	}
	return sended;
}

void TcpConn::Send(Buffer& buf)
{
	if (m_channel)
	{
		//Writable证明写通道打开--HandleWrite在写数据，此时追加buf到m_output,由HandleWrite统一写数据
		if (m_channel->Writable()) 
			m_output.absorb(buf);
		if (buf.size())
		{
			int sended = SendBase(buf.begin(), buf.size());
			buf.consume(sended);
		}
		if (buf.size())
		{
			m_output.absorb(buf);
			if (!m_channel->Writable())
				m_channel->EnableWrite(true);
		}
	}
	else
	{
	/*	warn("connection %s - %s closed, but still writing %lu bytes",
			local_.toString().c_str(), peer_.toString().c_str(), buf.size());*/
	}
}

void TcpConn::Send(const char * buf, size_t len)
{
	if (m_channel)
	{
		if (m_output.empty())
		{
			int sended = SendBase(buf, len);
			buf += sended;
			len -= sended;
		}
		if (len)
			m_output.append(buf, len);
	}
	else
		LWarn("connection {} - {} closed, but still writing {} bytes", m_local.toString(), m_peer.toString(), len);
}
