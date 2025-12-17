#pragma once

struct EchoThreadArgs
{
	CEchoContent* self;
};

class CEchoContent
{
public:
	CEchoContent();
	~CEchoContent();
	// 에코 스레드 생성
	bool Init();
	void AttachServer(CEchoServer* server);
	bool EnqueueMsgWithLock(const JobMsg* jobMsg);
	void WakeContentThread();
	void Stop();

	CEchoServer* _server = nullptr;
	HANDLE _hEchoThread = nullptr;

	HANDLE _eventContent = nullptr;
	RingBuffer _msgQ;
	SRWLOCK _srwlMsgQ = SRWLOCK_INIT;

private:

	static unsigned int WINAPI EchoThread(PVOID arg);
};