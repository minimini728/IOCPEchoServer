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


CEchoContent::CEchoContent():_msgQ(1000000)
{
	_eventContent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

CEchoContent::~CEchoContent()
{

}

bool CEchoContent::Init()
{
    // 에코 컨텐츠 스레드 생성
    EchoThreadArgs* args = new EchoThreadArgs;
    args->self = this;

    HANDLE hEchoThread = (HANDLE)_beginthreadex(NULL, 0, EchoThread, args, 0, NULL);
    if (hEchoThread == NULL)
    {
        wprintf(L"echo _beginthreadex fail!\n");
        return false;
    }
    _hEchoThread = hEchoThread;
    return true;
}

void CEchoContent::Stop()
{
    JobMsg jobMsg;
    jobMsg.sessionId = 0;
    jobMsg.EchoMsg.len = 0;
    jobMsg.EchoMsg.echo = 0;

    AcquireSRWLockExclusive(&_srwlMsgQ);
    _msgQ.Enqueue((char*)&jobMsg, sizeof(JobMsg));
    ReleaseSRWLockExclusive(&_srwlMsgQ);

    SetEvent(_eventContent);
    WaitForSingleObject(_hEchoThread, INFINITE);
    wprintf(L"echo thread exit\n");
}

void CEchoContent::AttachServer(CEchoServer* server)
{
	_server = server;
}

bool CEchoContent::EnqueueMsgWithLock(const JobMsg* jobMsg)
{
    AcquireSRWLockExclusive(&_srwlMsgQ);
    const bool result = _msgQ.Enqueue((char*)jobMsg, sizeof(JobMsg)); // RingBuffer가 바이트 단위라면 Copy Write
    ReleaseSRWLockExclusive(&_srwlMsgQ);
    return result;
}

void CEchoContent::WakeContentThread()
{
	SetEvent(_eventContent);
}

unsigned int WINAPI CEchoContent::EchoThread(PVOID arg)
{
    EchoThreadArgs* args = (EchoThreadArgs*)arg;
    CEchoContent* self = args->self;

    while (1)
    {
        bool isExist = false;
        int pushed = 0;
        int useSizeQ = 0;
        JobMsg jobMsg = { 0 };

        WaitForSingleObject(self->_eventContent, INFINITE);

        while (1)
        {
            AcquireSRWLockExclusive(&self->_srwlMsgQ);
            useSizeQ = self->_msgQ.GetUseSize();

            if (useSizeQ <= 0)
            {
                ReleaseSRWLockExclusive(&self->_srwlMsgQ);
                break;
            }

            // 메시지 디큐
            pushed = self->_msgQ.Dequeue((char*)&jobMsg, sizeof(JobMsg));
            ReleaseSRWLockExclusive(&self->_srwlMsgQ);

            if (pushed != sizeof(JobMsg))
            {
                wprintf(L"EchoThread MsgQ Dequeue fail\n");
                DebugBreak();
            }

            // 스레드 종료 메시지 확인
            if (jobMsg.sessionId == 0 && jobMsg.EchoMsg.len == 0 && jobMsg.EchoMsg.echo == 0)
                return 0;

            // 서버로 보낼 메시지 생성 및 초기화
            // 직렬화버퍼 포인터 + 직렬화버퍼 풀
            ProfileBegin(L"new");
            CPacket* newSendPacket = CPacket::Alloc();
            *newSendPacket << jobMsg.EchoMsg.len << jobMsg.EchoMsg.echo;
            ProfileEnd(L"new");

            // 서버로 메시지 전송 요청
            self->_server->SendPacket(jobMsg.sessionId, newSendPacket);

            // 에코 메시지 큐에 남아있나?
            AcquireSRWLockExclusive(&self->_srwlMsgQ);
            if (self->_msgQ.GetUseSize() >= sizeof(JobMsg))
                isExist = true;
            ReleaseSRWLockExclusive(&self->_srwlMsgQ);

            if (isExist)
                continue;
            else
                break;

        }

    }

    return 0;
}
