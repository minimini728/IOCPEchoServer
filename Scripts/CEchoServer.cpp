#include <WinSock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <process.h>

#include <stack>
#include <vector>
#include <unordered_map>

#include "CMemoryPool.h"
#include "CPacketBuffer.h"
#include "RingBuffer.h"
#include "CLanServer.h"
#include "CEchoServer.h"
#include "CEchoContent.h" 
#include "Profiler.h"


bool CEchoServer::OnConnectionRequest()
{
	// 화이트 IP 리스트 순회하면서 검증하기

	return true;
}

void CEchoServer::OnAccept(unsigned long long sessionId)
{
	// 서버로 보낼 메시지 생성 및 초기화
	// 직렬화버퍼 풀 사용
	CPacket* newSendPacket = CPacket::Alloc();
	unsigned short header = sizeof(INT64);
	INT64 payroad = 0x7fffffffffffffff;
	*newSendPacket << header << payroad;
	
	// 서버로 메시지 전송 요청
	SendPacket(sessionId, newSendPacket);

}

void CEchoServer::OnRelease(unsigned long long sessionId)
{

}

void CEchoServer::OnMessage(unsigned long long sessionId, CPacket* packet)
{
	JobMsg jobMsg = { 0 };
	jobMsg.sessionId = sessionId;
	memcpy(&jobMsg.EchoMsg, packet->GetBufferPtr(), sizeof(jobMsg.EchoMsg));
	CPacket::Free(packet); // 직렬화버퍼 풀

	_content->EnqueueMsgWithLock(&jobMsg);
	_content->WakeContentThread();
	
}

void CEchoServer::OnError(int errorCode, WCHAR*)
{

}

void CEchoServer::AttachContent(CEchoContent* content)
{
	_content = content;
}


