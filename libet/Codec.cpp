#include "Codec.h"
#include <iostream>
#include <WinSock2.h>
#include "Logger.h"

using namespace std;

int LengthCodec::TryDecode(Slice data, Slice& msg)
{
	//接收缓冲区不够一个包头大小
	if (data.size() <= 4)
		return 0;

	//int len = *(int32_t*)data.data();
	//LDebug("test uint len: {}",*(uint32_t*)data.data());
	//LDebug("test uint 2 len: {}", ntohl(*(uint32_t*)data.data()));
	int len = ntohl(*(int32_t*)data.data());
	if (len > 1024 * 1024) 
	{
		LDebug("decode len: {}", len);
		return -1;
	}

	if (data.size() >= len)
	{
		//string strMsg(data.data() + 8, len - 8);
		//LDebug("recv msg: {}", strMsg);
		msg = Slice(data.data() + 4, len - 4);
		return len;
	}
	return 0;
}  

void LengthCodec::Encode(Slice msg, Buffer& buf)
{
	buf.appendValue(htonl(4 + (int32_t)msg.size())).append(msg);
	//buf.appendValue(4 + (int32_t)msg.size()).append(msg);

	//buf.appendValue(htons((int32_t)msg.size())).append(msg);
	//LDebug("send len:{}, commandId:{}, msg:{}", 4 + msg.size(), *(int32_t*)msg.begin(), string(msg.begin() + 4, msg.size() - 4));
	//LDebug("send len:{}, commandId:{}", 4 + msg.size(), *(int32_t*)msg.begin());
}

