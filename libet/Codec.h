#pragma once
#include "Buffer.h"
#include "Slice.h"

// > 0 解析出完整消息，消息放在msg中，返回已扫描的字节数
// == 0 解析部分消息
// < 0 解析错误
struct CodecBase 
{
	virtual int TryDecode(Slice data, Slice& msg) = 0;
	virtual void Encode(Slice msg, Buffer& buf) = 0;
	virtual CodecBase* Clone() = 0;
};

//给出长度的消息
struct LengthCodec :public CodecBase 
{
	int TryDecode(Slice data, Slice& msg) override;
	//添加len+msg
	void Encode(Slice msg, Buffer& buf) override;
	CodecBase* Clone() override { return new LengthCodec(); }
};
