#pragma once

#define SERVERPORT 6000
#define MAXSESSIONCNT 1000

// ------------------------------------------------------------
// - 1개의 클라이언트 연결(Session)을 표현하는 구조체
// - Recv/Send 링버퍼 및 Overlapped 구조체 포함
// - IOCP 비동기 입출력 단위로 동작
// ------------------------------------------------------------
struct Session
{
	SOCKET sock = 0;						// 클라이언트 소켓 핸들
			
	RingBuffer recvQ;						// 수신 버퍼
	RingBuffer sendQ;						// 송신 버퍼

	OVERLAPPED ovSend = { 0 };				// WSASend용 Overlapped 구조체
	OVERLAPPED ovRecv = { 0 };				// WSARecv용 Overlapped 구조체
	WSABUF recvWSABUF[2] = { 0 };			// 송신 WSABUF 세그먼트 (순환 버퍼 구조 고려)
	WSABUF sendWSABUF[200] = { 0 };			// 송신 WSABUF 배열
	CPacket* sendPacketPtr[200] = { 0 };	// 송신 패킷 포인터 배열
	int addSendPacketCnt = 0;				// 송신 대기 패킷 개수
	int deleteSendPacketCnt = 0;			// 송신 완료 후 해제할 패킷 개수

	SOCKADDR_IN addr = { 0 };				// 클라이언트 IP/PORT 주소 정보
	 
	volatile unsigned long cntIO = 0;		// I/O 카운트 및 Release 플래그 (상위 비트 사용)
	unsigned long long sessionId = 0;		// 세션 고유 ID (상위32비트: 인덱스 / 하위32비트: 증가값)
	volatile long flagSend = 0;				// send 1번 제한 플래그

	SRWLOCK srwLock = SRWLOCK_INIT;			// 세션별 송신 큐 보호용 락
};

class CLanServer; // 전방선언

// Accept 스레드 전달 인자
struct AcceptThreadArgs
{
	HANDLE hcp;
	SOCKET listen_sock;
	CLanServer* self;
};

// 모니터링 스레드 전달 인자
struct MonitorThreadArgs
{
	CLanServer* self;
};

// 워커 스레드 전달 인자
struct WorkerThreadArgs
{
	HANDLE hcp;
	CLanServer* self;
};

//=====================================================================
//  [클래스 개요]
//  CLanServer
//   - IOCP 기반 네트워크 라이브러리 클래스 서버용
//=====================================================================
//  [주요 기능]
//   1) Accept/Worker/Monitor 스레드 기반
//	 2) 비동기 I/O 모델 사용
//   3) 컨텐츠 계층(CLanServer 상속)에서 이벤트 콜백 처리
//   4) 세션 재사용 및 안전한 종료를 위한 cntIO + flagRelease 구조
//=====================================================================
class CLanServer
{
public:
	CLanServer();
	~CLanServer();
	// ------------------------------------------------------------
	// [서버 시작]
	// param createThNum : 생성할 워커 스레드 수
	// param concurThNum : IOCP concurrency thread 수
	// param isNagle : Nagle 옵션 사용 여부
	// param isOverlapped : Overlapped I/O 여부
	// param maxSessionNum : 최대 세션 수
	// ------------------------------------------------------------
	bool Start(int createThNum, int concurThNum, bool isNagle, bool isOverlapped, int maxSessionNum);

	// -----------------------------------------------------------
	// [서버 종료]
	// -----------------------------------------------------------
	void Stop();

	// -----------------------------------------------------------
	// [현재 활성 세션 수 반환]
	// -----------------------------------------------------------
	int GetSessionCount() { return static_cast<int>(MAXSESSIONCNT - _emptySessionIndex.size()); }

	//------------------------------------------------------------
	// [컨텐츠 -> 서버 호출]
	//------------------------------------------------------------
	bool Disconnect(unsigned long long SessionId);
	bool SendPacket(unsigned long long sessionId, CPacket* packet);

	//------------------------------------------------------------
	// [서버 -> 컨텐츠 콜백 (상속받아 구현)]
	//------------------------------------------------------------
	// 클라이언트 접속 요청 시 호출
	virtual bool OnConnectionRequest() = 0;

	// 클라이언트 접속 완료 시 호출 (Accept 후 세션 생성 완료)
	virtual void OnAccept(unsigned long long sessionId) = 0;

	// 클라이언트 종료 시 호출 (ReleaseSession 완료 후)
	virtual void OnRelease(unsigned long long sessionId) = 0;

	// 클라이언트 패킷 수신 완료 시 호출
	virtual void OnMessage(unsigned long long sessionId, CPacket* packet) = 0;

	// 에러 발생 시 호출
	virtual void OnError(int errorCode, WCHAR*) = 0;

	//------------------------------------------------------------
	// [모니터링용 TPS 카운터]
	//------------------------------------------------------------
	int GetAcceptTPS() { return _acceptCnt; }
	int GetRecvMsgTPS() { return _recvCnt; }
	int GetSendMsgTPS() { return _sendCnt; }
 
private:
	//---------------------------------------------------------------
	// [세션 관리]
	//---------------------------------------------------------------
	Session* _arrSession[MAXSESSIONCNT] = { 0 };	// 세션 배열 (인덱스로 접근)
	std::stack<int> _emptySessionIndex;				// 비어있는 세션 인덱스 스택
	SRWLOCK _srwlEmptyStack = SRWLOCK_INIT;			// 스택 접근 보호용 락
		
	unsigned long long _sessionId = 0;				// 세션 ID 증가값

	//---------------------------------------------------------------
	// [모니터링 스레드]
	//---------------------------------------------------------------
	HANDLE _eMonitor;								// 모니터링 스레드 이벤트 핸들
	volatile long _acceptCnt;						// 초당 Accept 횟수
	volatile long _recvCnt;							// 초당 Recv 메시지 수
	volatile long _sendCnt;							// 초당 Send 메시지 수

	//------------------------------------------------------------
	// [핸들 및 스레드 관리]
	//------------------------------------------------------------
	HANDLE _hcp = nullptr;							// IOCP 핸들
	std::vector<HANDLE> _threadHandles;				// 워커 스레드 핸들 목록
	HANDLE _hAcceptThread = nullptr;				// Accpet 스레드 핸들


	//------------------------------------------------------------
	// [내부 동작 함수]
	//------------------------------------------------------------
	// 세션 정리 및 리소스 반환
	void ReleaseSession(Session* session);

	// IOCP 워커 스레드 루프
	static unsigned int WINAPI WorkerThread(PVOID arg);

	// 모니터링 스레드 (TPS 초기화)
	static unsigned int WINAPI MonitorThread(PVOID arg);

	// Accept 스레드
	static unsigned int WINAPI AcceptThread(PVOID arg);

	// 송신 IO 등록
	void PostSend(Session* ptr);

	// 수신 IO 등록
	void PostRecv(Session* ptr);
};