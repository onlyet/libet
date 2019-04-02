#pragma once
#include "../Logger.h"

#define FIVE_K_TXT "E:\\File\\5K.txt"
#define TWENTY_K_TXT "E:\\File\\20K.txt"
#define TWO_HUNDRED_K_TXT "E:\\File\\200K.txt"
#define ONEM_TXT "E:\\File\\1M.txt"
#define TWOM_TXT "E:\\File\\2M.txt"
#define BUF_SIZE 1024

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

