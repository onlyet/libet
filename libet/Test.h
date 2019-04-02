#pragma once
#include "Logger.h"

#define FIVE_K_TXT "E:\\File\\5K.txt"
#define TWENTY_K_TXT "E:\\File\\20K.txt"
#define TWO_HUNDRED_K_TXT "E:\\File\\200K.txt"
#define ONEM_TXT "E:\\File\\1M.txt"
#define TWOM_TXT "E:\\File\\2M.txt"
#define BUF_SIZE 1024

void TestTcpServer();

void TestTimer();

void IdleCloseTest();

void HeartbeatTest();

void TestLog();

//Buffer GenerateData();

void PeriodicallySendTest();

void MultiEbPeriodicallySendTest();

void OneIoMultiWork();

void TestSingleton();

