#pragma once
#include <atomic>
#include <memory>

#include "Global.h"
#include "Util.h"

//轮询器
struct PollerBase : private noncopyable
{
	PollerBase() : m_lastActive(-1)
	{
		static std::atomic<int64_t> id(0);
		m_id = ++id;
	}
	virtual ~PollerBase() {};

	virtual void AddChannel(Channel* ch) = 0;
	virtual void RemoveChannel(Channel* ch) = 0;
	virtual void UpdateChannel(Channel* ch) = 0;
	virtual void LoopOnce(int waitMs) = 0;

	int64_t m_id;
	int m_lastActive;	//激活的事件数
};

PollerBase* CreatePoller();