### 参考handy库修改的网络库。
### 基于select模型，适用于windows平台。
* TcpServer: 用于绑定IP,端口，处理Tcp连接。 
* TcpConn: 处理连接中的读，写。
* Channel: 维护一个fd的通道，处理读写。
* EventBase: 事件分发器，可以设置定时任务。
* PollerBase: 轮询器基类。
* PollerSelect: 主要用作select轮询事件，用于添加/删除/更新通道。
* Buffer: 一个Tcp连接有两个buffer，输入buffer，输出buffer。
* Slice: 作为 Buffer的扩展类，只维护Buffer的指针，不拥有实际资源。
* CodecBase: 用于编解码业务消息。
* Logger: spdlog的封装。
* SafeQueue: 线程安全队列。
* ThreadPool: 线程池。
* ConnThreadPool: 连接线程池。
* Timer: C++11封装的计算时间差类。

### 测试代码
```
// 测试TCP服务器
void TestTcpServer();

//测试定时器
void TestTimer();

//测试关闭闲置连接
void IdleCloseTest();

//测试echo和心跳
void HeartbeatTest();

//测试定时发送txt文本
void PeriodicallySendTest();

//测试MultiBase发送txt文本
void MultiEbPeriodicallySendTest();

//测试单EventBase，工作线程池
void OneIoMultiWork();

//测试单例模式
void TestSingleton();
```
