#include "Net.h"
#include "Util.h"
#include <iostream>
#include "Logger.h"
using namespace std;

void Net::error_handling(const char * message)
{
		fputs(message, stderr);
		fputc('\n', stderr);
		system("pause");
		exit(-1);
}

int Net::SetNonBlocking(SOCKET sock)
{
	int ret;
	unsigned long flag = 1;
	if ((ret = ioctlsocket(sock, FIONBIO, (unsigned long *)&flag)) == SOCKET_ERROR)
		LCritical("ioctlsocket() error:{}", errcode);
	return ret;
}

Ip4Addr::Ip4Addr(const std::string & host, unsigned short port)
{
	memset(&addr_, 0, sizeof addr_);
	addr_.sin_family = AF_INET;
	addr_.sin_port = htons(port);
	if (host.size())
	{
		//ÔÝÊ±²»ÓÃgetHostByName
		//addr_.sin_addr = getHostByName(host);
		//addr_.sin_addr.s_addr = INADDR_ANY;
		addr_.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
		addr_.sin_addr.s_addr = htonl(INADDR_ANY);
		//addr_.sin_addr.s_addr = INADDR_ANY;

	//if (addr_.sin_addr.s_addr == INADDR_NONE) 
		//error("cannot resove %s to ip", host.c_str());
}

std::string Ip4Addr::toString() const
{
	uint32_t uip = addr_.sin_addr.s_addr;
	return util::format("%d.%d.%d.%d:%d", (uip >> 0) & 0xff, (uip >> 8) & 0xff, (uip >> 16) & 0xff, (uip >> 24) & 0xff, ntohs(addr_.sin_port));
}

std::string Ip4Addr::ip() const
{
	uint32_t uip = addr_.sin_addr.s_addr;
	return util::format("%d.%d.%d.%d", (uip >> 0) & 0xff, (uip >> 8) & 0xff, (uip >> 16) & 0xff, (uip >> 24) & 0xff);
}

unsigned short Ip4Addr::port() const
{
	return ntohs(addr_.sin_port);
}

unsigned int Ip4Addr::ipInt() const
{
	return ntohl(addr_.sin_addr.s_addr);
}

bool Ip4Addr::isIpValid() const
{
	return addr_.sin_addr.s_addr != INADDR_NONE;
}
