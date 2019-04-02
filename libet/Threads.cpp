#include "threads.h"
#include <assert.h>
#include <utility>
#include <algorithm>
#include "Logger.h"
using namespace std;

ThreadPool::ThreadPool(int threads, int maxWaiting, bool start) : 
	tasks_(maxWaiting), threads_(threads)
{
	if (start)
		this->start();
}

ThreadPool::~ThreadPool()
{
	assert(tasks_.exited());
	if (tasks_.size())
		LInfo("{} tasks nto processed when thread pool exited", tasks_.size());
}

void ThreadPool::start() 
{
	for (auto &th : threads_) 
	{
		thread t([this]
		{
			while (!tasks_.exited())
			{
				Task task;
				if (tasks_.pop_wait(&task))
					task();
			}
		});
		th.swap(t);
	}
}

void ThreadPool::join()
{
	for (auto &t : threads_)
		t.join();
}

bool ThreadPool::addTask(Task &&task)
{
	return tasks_.push(move(task));
}


template <typename T>
size_t SafeQueue<T>::size() 
{
	std::lock_guard<std::mutex> lk(*this);
	return items_.size();
}

template <typename T>
void SafeQueue<T>::exit() 
{
	exit_ = true;
	std::lock_guard<std::mutex> lk(*this);
	ready_.notify_all();
}

template <typename T>
bool SafeQueue<T>::push(T &&v) 
{
	//std::lock_guard<std::mutex> lk(*this);
	//if (exit_ || (capacity_ && items_.size() >= capacity_)) 
	//	return false;

	//items_.push_back(std::move(v));
	//ready_.notify_one();

	{
		std::lock_guard<std::mutex> lk(*this);
		if (exit_ || (capacity_ && items_.size() >= capacity_))
			return false;

		items_.push_back(std::move(v));
	}
	ready_.notify_one();

	return true;
}
template <typename T>
void SafeQueue<T>::wait_ready(std::unique_lock<std::mutex> &lk, int waitMs)
{
	if (exit_ || !items_.empty())
		return;

	if (waitMs == wait_infinite)
		ready_.wait(lk, [this] { return exit_ || !items_.empty(); });
	else if (waitMs > 0)
	{
		auto tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
		while (ready_.wait_until(lk, tp) != std::cv_status::timeout && items_.empty() && !exit_)
		{
		}
	}
}

template <typename T>
bool SafeQueue<T>::pop_wait(T *v, int waitMs)
{
	std::unique_lock<std::mutex> lk(*this);
	wait_ready(lk, waitMs);
	if (items_.empty())
		return false;

	*v = std::move(items_.front());
	items_.pop_front();
	return true;
}

template <typename T>
T SafeQueue<T>::pop_wait(int waitMs)
{
	std::unique_lock<std::mutex> lk(*this);
	wait_ready(lk, waitMs);
	if (items_.empty())
		return T();

	T r = std::move(items_.front());
	items_.pop_front();
	return r;
}

ConnThreadPool::ConnThreadPool(int threads, int maxTaskNum, bool start) :
	m_workers(threads), m_exited(false)
{
	if (start)
		Start();
}

ConnThreadPool::~ConnThreadPool()
{
	assert(m_exited);
}

void ConnThreadPool::Start()
{
	for (auto &worker : m_workers)
	{
		thread t([this, &worker]
		{
			while (!worker.m_tasks->exited())
			{
				Task task;
				if (worker.m_tasks->pop_wait(&task))
					task();
			}
		});
		worker.m_thread.swap(t);
	}
}

ConnThreadPool & ConnThreadPool::Exit()
{
	for (auto &worker : m_workers)
		worker.m_tasks->exit();
	m_exited = true;
	return *this;
}

void ConnThreadPool::Join()
{
	for (auto &workers : m_workers)
		workers.m_thread.join();
}

bool ConnThreadPool::AddTask(int64_t id, Task&& task)
{
	return m_workers[Index(id)].m_tasks->push(std::move(task));
}
