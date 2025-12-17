#pragma once

class CEchoContent; // 전방선언

#pragma pack(push, 1)
struct JobMsg
{
	unsigned long long sessionId;

	struct EchoMsg
	{
		unsigned short len;
		INT64 echo;
	} EchoMsg;

};
#pragma pack(pop)


struct Player
{
	unsigned long long playerId;

};

class CEchoServer : public CLanServer
{
public:

	void AttachContent(CEchoContent* content);

protected:
	CEchoContent* _content = nullptr;

private:

	bool OnConnectionRequest();

	void OnAccept(unsigned long long sessionId);

	void OnRelease(unsigned long long sessionId);

	void OnMessage(unsigned long long sessionId, CPacket* packet);

	void OnError(int errorCode, WCHAR*);

};