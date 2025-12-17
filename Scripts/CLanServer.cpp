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
#include "Profiler.h"

#pragma comment (lib, "ws2_32")

//---------------------------------------------------------------------
// [CLanServer]
// - 세션 배열 초기화 및 Empty Stack 구성
// - 모니터링 이벤트 생성
//---------------------------------------------------------------------
CLanServer::CLanServer()
{
	for (int i = 0; i < MAXSESSIONCNT; i++)
	{
		_emptySessionIndex.push(i);
	}

	for (int i = 0; i < MAXSESSIONCNT; i++)
	{
		_arrSession[i] = new Session;
	}

	_eMonitor = CreateEvent(NULL, FALSE, FALSE, NULL);
	wprintf(L"_eMonitor handle: %p\n", _eMonitor);
	wprintf(L"create event\n");
}

CLanServer::~CLanServer()
{

}

//---------------------------------------------------------------------
// [Start]
// - 서버 시작 (소켓, IOCP, 스레드 초기화)
//---------------------------------------------------------------------
bool CLanServer::Start(int createThNum, int concurThNum,
	bool isNagle, bool isOverlapped, int maxSessionNum)
{
	// [1] 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		wprintf(L"[CLanServer::Start] WSAStartup error\n");
		return false;
	}

	// [2] IOCP 생성
	HANDLE hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurThNum);
	if (hcp == NULL)
	{
		wprintf(L"[CLanServer::Start] IOCP Create Fail\n");
		return false;
	}
	_hcp = hcp;

	// [3] 워커 스레드 생성
	_threadHandles.reserve(createThNum);
	HANDLE hThread;
	WorkerThreadArgs* workerArgs = new WorkerThreadArgs;
	workerArgs->hcp = hcp;
	workerArgs->self = this;
	for (int i = 0; i < createThNum; i++)
	{
		hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, workerArgs, 0, NULL);
		if (hThread == NULL)
		{
			wprintf(L"[CLanServer::Start] Create Thread Fail\n");
			return false;
		}
		_threadHandles.push_back(hThread);

	}

	// [4] listen socket 생성
	int GLESocket;
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
	{
		GLESocket = WSAGetLastError();
		wprintf(L"[CLanServer::Start] listen_sock socket error code: %d\n", GLESocket);
		return false;
	}

	// [5] bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	int GLEBind;
	int retBind = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retBind == SOCKET_ERROR)
	{
		GLEBind = WSAGetLastError();
		wprintf(L"[CLanServer::Start] bind error code: %d\n", GLEBind);
		return false;
	}

	// [6] listen()
	int GLEListen;
	int retListen = listen(listen_sock, SOMAXCONN_HINT(0x7fffffff));
	if (retListen == SOCKET_ERROR)
	{
		GLEListen = WSAGetLastError();
		wprintf(L"[CLanServer::Start] listen error code: %d\n", GLEListen);
		return false;
	}

	// [7] Linger 0 걸기
	int GLESetSock;
	LINGER optval;
	optval.l_onoff = 1;
	optval.l_linger = 0;
	int retsetsockopt = setsockopt(listen_sock, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
	if (retsetsockopt == SOCKET_ERROR)
	{
		GLESetSock = WSAGetLastError();
		wprintf(L"setsockopt Linger() error : %d", GLESetSock);
		return 1;
	}


	// [8] 모니터링 스레드 생성
	MonitorThreadArgs* monitorArgs = new MonitorThreadArgs;
	monitorArgs->self = this;
	HANDLE hMonThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, monitorArgs, 0, NULL);
	if (hMonThread == NULL)
	{
		wprintf(L"[CLanServer::Start] moniterThread create fail\n");
		return false;
	}

	// [9] Accept 스레드 생성
	AcceptThreadArgs* acceptargs = new AcceptThreadArgs;
	acceptargs->hcp = hcp;
	acceptargs->listen_sock = listen_sock;
	acceptargs->self = this;

	HANDLE hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, acceptargs, 0, NULL);
	if (hAcceptThread == NULL)
	{
		wprintf(L"[CLanServer::Start] acceptThread create fail\n");
		return false;
	}

	_hAcceptThread = hAcceptThread;

	return true;
}

//---------------------------------------------------------------------
// [Stop]
// - IOCP 종료 신호 전달
// - 모든 스레드 종료 대기
//---------------------------------------------------------------------
void CLanServer::Stop()
{
	// IOCP 종료
	PostQueuedCompletionStatus(_hcp, 0, 0, 0);
	WaitForMultipleObjects(static_cast<DWORD>(_threadHandles.size()), _threadHandles.data(), TRUE, INFINITE);
	CloseHandle(_hcp);

	// 프로파일링 저장
	ProfileDataOut();

	// 모니터링 스레드 종료
	SetEvent(_eMonitor);

	
}

//---------------------------------------------------------------------
// [AcceptThread]
// - 클라이언트 연결 수락 및 세션 생성
// - IOCP에 소켓 등록
//---------------------------------------------------------------------
unsigned int WINAPI CLanServer::AcceptThread(PVOID arg)
{
	wprintf(L"accept thread run\n");

	AcceptThreadArgs* args = (AcceptThreadArgs*)arg;

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	//DWORD recvbytes, flags;
	HANDLE hcp = args->hcp;
	SOCKET listen_sock = args->listen_sock;
	CLanServer* self = args->self;

	while (1)
	{
		// [1] accept()
		int GLEAccept;
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			GLEAccept = WSAGetLastError();
			wprintf(L"[CLanServer::AcceptThread] accept error code: %d\n", GLEAccept);
			return 0;
		}

		InterlockedAdd(&self->_acceptCnt, 1);

		// 송신버퍼 없애기
		int sendBufSize = 0;
		setsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));

		// 접속한 클라이언트 정보 출력
		WCHAR clientIP[16] = { 0 };
		InetNtop(AF_INET, &clientaddr.sin_addr, clientIP, 16);
		wprintf(L"\n[TCP 서버] 클라이언트 접속: IP 주소 = %s, 포트번호 = %d, socket = 0x%016llX\n",
			clientIP, ntohs(clientaddr.sin_port), client_sock);


		// [2] 빈 인덱스 찾기
		AcquireSRWLockExclusive(&self->_srwlEmptyStack);
		unsigned long long index = self->_emptySessionIndex.top();
		self->_emptySessionIndex.pop();
		ReleaseSRWLockExclusive(&self->_srwlEmptyStack);

		// [3] 세션 초기화 및 ID 부여
		Session* newSession = self->_arrSession[(int)index];
		newSession->sessionId = (static_cast<unsigned long long>(index) << 32) | ((self->_sessionId++) & 0xFFFFFFFF);
		newSession->sock = client_sock;
		newSession->addr = clientaddr;
		newSession->recvQ.ClearBuffer();
		newSession->sendQ.ClearBuffer();
		newSession->cntIO = 0;
		newSession->flagSend = 0;
		newSession->addSendPacketCnt = 0;
		newSession->deleteSendPacketCnt = 0;


		// [4] IOCP에 세션 등록
		CreateIoCompletionPort((HANDLE)client_sock, hcp, (ULONG_PTR)newSession, 0);

		// I/O 카운트 증가
		InterlockedIncrement(&newSession->cntIO);

		// [5] 사용자 정의 이벤트 호출
		self->OnAccept(newSession->sessionId);

		// [6] 첫 번째 비동기 Recv 등록
		self->PostRecv(newSession);

		// I/O 카운트 감소
		InterlockedDecrement(&newSession->cntIO);

	}

}

//---------------------------------------------------------------------
// [WorkerThread]
// - GetQueuedCompletionStatus()로 I/O 완료 통지 수신
// - Recv / Send 완료 처리
//---------------------------------------------------------------------
unsigned int WINAPI CLanServer::WorkerThread(PVOID arg)
{
	WorkerThreadArgs* args = (WorkerThreadArgs*)arg;
	HANDLE hcp = args->hcp;
	CLanServer* self = args->self;
	int retGQCS = 0;

	while (1)
	{
		// 비동기 입출력 완료 기다리기
		DWORD cbTransferred = 0;
		ULONG_PTR completionKey = 0;
		LPOVERLAPPED overlapped = NULL;
		Session* ptr;

		retGQCS = GetQueuedCompletionStatus(hcp, &cbTransferred,
			&completionKey, &overlapped, INFINITE);

		// [1] IOCP 종료
		if (overlapped == NULL)
		{
			PostQueuedCompletionStatus(hcp, 0, 0, 0);
			return 0;
		}

		ptr = (Session*)completionKey;

		// [2] 클라 종료
		if (cbTransferred == 0)
		{
			// I/O 카운트 감소
			// 종료한 클라일 경우 정리
			if (InterlockedDecrement(&ptr->cntIO) == 0)
			{
				// 세션 정리
				self->ReleaseSession(ptr);
			}

			continue;
		}

		// [3] Recv 완료 처리
		if (overlapped == &ptr->ovRecv)
		{
			ptr->recvQ.MoveRear(cbTransferred);

			// ---------------------------------------------------------
			// 하나의 메시지가 왔는가?
			// ---------------------------------------------------------
			int retPeekHeader = 0;
			int retPeekPayload = 0;

			while (ptr->recvQ.GetUseSize() > sizeof(unsigned short))
			{
				unsigned short header = 0;
				INT64 payload = 0;

				// 헤더 복사
				retPeekHeader = ptr->recvQ.Peek((char*)&header, sizeof(unsigned short));
				if (retPeekHeader < sizeof(unsigned short))
				{
					wprintf(L"[CLanServer::WorkerThread] recvQ header peek error size: %d\n", retPeekHeader);
					break;
				}

				// 헤더 메시지 확인
				if (header != 8)
				{
					wprintf(L"[CLanServer::WorkerThread] header code error: header = %hu\n", header);
					WCHAR clientIP[16] = { 0 };
					InetNtop(AF_INET, &ptr->addr.sin_addr, clientIP, 16);
					wprintf(L"[TCP 서버] 클라이언트 헤더오류: IP 주소 = %s, 포트번호 = %d, socket = 0x%016llX\n",
						clientIP, ntohs(ptr->addr.sin_port), ptr->sock);
				}

				if (ptr->recvQ.GetUseSize() < sizeof(unsigned short) + sizeof(INT64))
					break;

				// 헤더 제거
				ptr->recvQ.MoveFront(sizeof(unsigned short));

				// 페이로드 추출
				retPeekPayload = ptr->recvQ.Peek((char*)&payload, sizeof(INT64));
				if (retPeekPayload < sizeof(INT64))
				{
					wprintf(L"[CLanServer::WorkerThread] recvQ payload peek error!\n");
					DebugBreak();
				}
				//페이로드 제거
				ptr->recvQ.MoveFront(sizeof(INT64));

				// 메시지 처리 호출
				// 직렬화버퍼 포인터 + 버퍼 풀
				CPacket* newpacket = CPacket::Alloc();
				*newpacket << header << payload;

				self->OnMessage(ptr->sessionId, newpacket);
				InterlockedIncrement(&self->_recvCnt);
			}

			// recv 재요청
			self->PostRecv(ptr);

			// I/O 카운트 감소
			// 종료한 클라일 경우
			if (InterlockedDecrement(&ptr->cntIO) == 0)
			{
				// 종료한 클라이언트 정보 출력
				WCHAR szClientIP[16] = { 0 };
				InetNtop(AF_INET, &ptr->addr.sin_addr, szClientIP, 16);
				//wprintf(L"\n[TCP 서버] 클라이언트 종료: IP 주소 = %s, 포트번호 = %d\n",
				//    szClientIP, ntohs(ptr->addr.sin_port));
				wprintf(L"recv 완료 종료\n");

				// 세션 정리
				self->ReleaseSession(ptr);

				continue;
			}

		}
		else if (overlapped == &ptr->ovSend) // [4] send 완료 처리
		{
			AcquireSRWLockExclusive(&ptr->srwLock);

			// 직렬화버퍼 포인터
			// 저장해놓은 packet 포인터 delete 시키기
			for (int i = 0; i < ptr->deleteSendPacketCnt; i++)
			{
				CPacket::Free(ptr->sendPacketPtr[i]);
			}
			ptr->deleteSendPacketCnt = 0;
			ReleaseSRWLockExclusive(&ptr->srwLock);

			// I/O 카운트 감소
			// 종료한 클라일 경우
			if (InterlockedDecrement(&ptr->cntIO) == 0)
			{
				// 종료한 클라이언트 정보 출력
				WCHAR szClientIP[16] = { 0 };
				InetNtop(AF_INET, &ptr->addr.sin_addr, szClientIP, 16);
				//wprintf(L"\n[TCP 서버] 클라이언트 종료: IP 주소 = %s, 포트번호 = %d\n",
				//    szClientIP, ntohs(ptr->addr.sin_port));
				WCHAR clientIP[16] = { 0 };
				InetNtop(AF_INET, &ptr->addr.sin_addr, clientIP, 16);
				wprintf(L"\n[TCP 서버] 클라 send 완료종료: IP 주소 = %s, 포트번호 = %d, socket = 0x%016llX\n",
					clientIP, ntohs(ptr->addr.sin_port), ptr->sock);

				wprintf(L"send 완료 종료\n");

				// 세션 정리
				self->ReleaseSession(ptr);
				continue;
			}

			// 남은거 확인
			bool isExist = false;
			AcquireSRWLockShared(&ptr->srwLock);
			if (ptr->sendQ.GetUseSize() > 0)
				isExist = true;
			ReleaseSRWLockShared(&ptr->srwLock);

			if (isExist)
			{
				self->PostSend(ptr);
			}
			else
			{
				// send flag 변경
				InterlockedExchange(&ptr->flagSend, 0);
			}
		}

	}

	return 0;
}

//---------------------------------------------------------------------
// [Disconnect]
// - 컨텐츠에서 세션 종료 요쳥
// - recv 취소
//---------------------------------------------------------------------
bool CLanServer::Disconnect(unsigned long long sessionId)
{
	// [1] 배열에서 세션찾기
	Session* ptr = _arrSession[int(sessionId >> 32)];

	// [2] IOcnt 증가
	InterlockedIncrement(&ptr->cntIO);

	// [3] flagRelease 확인
	unsigned long mask = 0x80000000;
	if (ptr->cntIO & mask)
	{
		InterlockedDecrement(&ptr->cntIO);
		return true;
	}

	// [4] 세션 유효성 검증 (재사용 감지)
	if (ptr->sessionId != sessionId)
	{
		InterlockedDecrement(&ptr->cntIO);
		OnRelease(sessionId);
		return true;
	}

	// [5] 세션 사용
	// 항상 걸려있는 recv를 중단시켜야 함
	DWORD GLE = 0;
	if (CancelIoEx((HANDLE)ptr->sock, &ptr->ovRecv) == 0)
	{
		GLE = GetLastError();
		if (GLE != ERROR_NOT_FOUND)
		{
			wprintf(L"[Disconnect] CancelIoEx() fail. GLE=%u\n", GLE);
		}
	}

	// [6] IOCnt 감소
	if (InterlockedDecrement(&ptr->cntIO) == 0)
	{
		ReleaseSession(ptr);
	}

	return true;

}

//---------------------------------------------------------------------
// [SendPacket]
// - 세션에 패킷 송신 요청
//---------------------------------------------------------------------
bool CLanServer::SendPacket(unsigned long long sessionId, CPacket* packet)
{
	// 서버 모니터링용
	InterlockedAdd(&_sendCnt, 1);

	// [1] 배열에서 세션찾기
	Session* ptr = _arrSession[int(sessionId >> 32)];

	// [2] IOcnt 증가
	InterlockedIncrement(&ptr->cntIO);

	// [3] flagRelease 확인
	unsigned long mask = 0x80000000;
	if (ptr->cntIO & mask)
	{
		InterlockedDecrement(&ptr->cntIO);
		return true;
	}

	// [4] 세션 유효성 검증 (재사용 감지)
	if (ptr->sessionId != sessionId)
	{
		InterlockedDecrement(&ptr->cntIO);
		OnRelease(sessionId);
		return true;
	}
	
	// [5] 세션 사용
	// sendQ에 packet 포인터 넣기
	AcquireSRWLockExclusive(&ptr->srwLock);
	ptr->addSendPacketCnt++; // 직렬화버퍼 포인터
	ptr->sendQ.Enqueue((char*)&packet, sizeof(CPacket*)); // 직렬화버퍼 포인터
	ReleaseSRWLockExclusive(&ptr->srwLock);

	// 송신 플래그 검사 및 WSASend 요청
	if (InterlockedCompareExchange(&ptr->flagSend, /*교환할 값*/1, /*비교할 값*/0) == 0)
	{
		PostSend(ptr);
	}

	// [6] IOcnt 내리기
	if (InterlockedDecrement(&ptr->cntIO) == 0)
	{
		ReleaseSession(ptr);
	}

	return true;
}

//---------------------------------------------------------------------
// [ReleaseSession]
// - 세션 리소스 해제 및 재사용 스택 복귀
//---------------------------------------------------------------------
void CLanServer::ReleaseSession(Session* session)
{
	unsigned long cmp = 0;
	unsigned long change = 0x80000000;

	// 원자적으로 flagRelease 올리기 시도
	if (InterlockedCompareExchange(&session->cntIO, change, cmp) == cmp)
	{
		// 컨텐츠에 연결 끊김 알리기
		OnRelease(session->sessionId);

		// empty 스택에 넣기
		AcquireSRWLockExclusive(&_srwlEmptyStack);
		int index = static_cast<int>(session->sessionId >> 32);
		_emptySessionIndex.push(index);
		ReleaseSRWLockExclusive(&_srwlEmptyStack);


		closesocket(session->sock);
		session->sock = INVALID_SOCKET;

		// 세션 상태 초기화 (다음 연결을 위해)
		session->cntIO = 0;
		session->sessionId = 0;
		session->flagSend = 0;
		session->recvQ.ClearBuffer();
		session->sendQ.ClearBuffer();
	}

}

//---------------------------------------------------------------------
// [MonitorThread]
// - 모니터링 스레드 (TPS 초기화용)
//---------------------------------------------------------------------
unsigned int WINAPI CLanServer::MonitorThread(PVOID arg)
{
	MonitorThreadArgs* args = (MonitorThreadArgs*)arg;
	CLanServer* self = args->self;
	HANDLE eShutdown = self->_eMonitor;

	while (1)
	{
		DWORD ret = WaitForSingleObject(eShutdown, 1000);
		if (ret == WAIT_OBJECT_0) // 종료용
		{
			break;
		}
		else if (ret == WAIT_TIMEOUT) // 1초마다 갱신용
		{
			InterlockedExchange(&self->_acceptCnt, 0);
			InterlockedExchange(&self->_recvCnt, 0);
			InterlockedExchange(&self->_sendCnt, 0);
		}
		else if (ret == WAIT_FAILED)
		{
			wprintf(L"WaitForSingleObject failed! GLE=%d\n", GetLastError());
		}

	}

	return 0;
}

//---------------------------------------------------------------------
// [PostSend]
// - 실제 WSASend 등록
//---------------------------------------------------------------------
void CLanServer::PostSend(Session* ptr)
{
	// ------------------------------------------------------
	// 세션 락
	AcquireSRWLockExclusive(&ptr->srwLock);

	int want = ptr->sendQ.GetUseSize();
	if (want <= 0)
	{
		// 보낼거 없으면 플래그 내리기
		InterlockedExchange(&ptr->flagSend, 0);
		ReleaseSRWLockExclusive(&ptr->srwLock);

		return;
	}
	// I/O 카운트 증가
	InterlockedIncrement(&ptr->cntIO);

	ZeroMemory(&ptr->ovSend, sizeof(ptr->ovSend));

	// 직렬화버퍼 포인터
	int sendCnt = ptr->addSendPacketCnt;
	ptr->addSendPacketCnt = 0;
	ptr->deleteSendPacketCnt = sendCnt;

	for (int i = 0; i < sendCnt; i++)
	{
		CPacket* packetPtr = nullptr;
		ptr->sendQ.Dequeue((char*)&packetPtr, sizeof(CPacket*));
		ptr->sendWSABUF[i].buf = packetPtr->GetBufferPtr();
		ptr->sendWSABUF[i].len = packetPtr->GetDataSize();
		ptr->sendPacketPtr[i] = packetPtr;

	}

	ReleaseSRWLockExclusive(&ptr->srwLock);
	// ----------------------------------------------------------

	DWORD sendbytes;
	DWORD GLESend;
	LARGE_INTEGER startTime, endTime, freq;

	// 직렬화버퍼 포인터
	int retSend = WSASend(ptr->sock, ptr->sendWSABUF, ptr->deleteSendPacketCnt, &sendbytes, 0, &ptr->ovSend, NULL);

	if (retSend == SOCKET_ERROR)
	{
		GLESend = WSAGetLastError();
		if (GLESend != WSA_IO_PENDING)
		{
			wprintf(L"thread WSASend error: %d\n", GLESend);

			InterlockedExchange(&ptr->flagSend, 0);

			// I/O 카운트 감소
			// 종료한 클라일 경우
			if (InterlockedDecrement(&ptr->cntIO) == 0)
			{
				// 종료한 클라이언트 정보 출력
				WCHAR szClientIP[16] = { 0 };
				InetNtop(AF_INET, &ptr->addr.sin_addr, szClientIP, 16);
				WCHAR clientIP[16] = { 0 };
				InetNtop(AF_INET, &ptr->addr.sin_addr, clientIP, 16);
				wprintf(L"\n[TCP 서버] 클라이언트 PostSend 실패 종료: IP 주소 = %s, 포트번호 = %d, socket = 0x%016llX\n",
					clientIP, ntohs(ptr->addr.sin_port), ptr->sock);

				wprintf(L"PostSend 종료\n");

				// 세션 정리
				ReleaseSession(ptr);

				return;
			}

			if (GLESend == WSA_IO_PENDING)
			{
				//wprintf(L"thread send I/O PENDING\n");
			}

		}
		else if (retSend == 0)
		{
			//wprintf(L"thread 동기 send: %d\n", sendbytes);
		}
	}

}

//---------------------------------------------------------------------
// [PostRecv]
// - 실제 WSARecv 등록
//---------------------------------------------------------------------
void CLanServer::PostRecv(Session* ptr)
{
	// I/O 카운트 증가
	InterlockedIncrement(&ptr->cntIO);

	ZeroMemory(&ptr->ovRecv, sizeof(ptr->ovRecv));

	int segCnt = 0;
	int remain = 0;

	ptr->recvWSABUF[0].buf = ptr->recvQ.GetRearBufferPtr();
	ptr->recvWSABUF[0].len = ptr->recvQ.DirectEnqueueSize();
	segCnt++;
	remain = ptr->recvQ.GetFreeSize() - ptr->recvQ.DirectEnqueueSize();
	if (remain > 0)
	{
		ptr->recvWSABUF[1].buf = ptr->recvQ.GetBufferBasePtr();
		ptr->recvWSABUF[1].len = remain;
		segCnt++;
	}

	DWORD recvbytes;
	DWORD flags = 0;
	int GLERecv;
	int retRecv = WSARecv(ptr->sock, ptr->recvWSABUF, segCnt, &recvbytes, &flags, &ptr->ovRecv, NULL);
	if (retRecv == SOCKET_ERROR)
	{
		GLERecv = WSAGetLastError();
		if (GLERecv != WSA_IO_PENDING)
		{
			wprintf(L"thread WSARecv error: %d\n", GLERecv);

			// I/O 카운트 감소
			// 종료한 클라일 경우
			if (InterlockedDecrement(&ptr->cntIO) == 0)
			{
				// 종료한 클라이언트 정보 출력
				WCHAR szClientIP[16] = { 0 };
				InetNtop(AF_INET, &ptr->addr.sin_addr, szClientIP, 16);
				WCHAR clientIP[16] = { 0 };
				InetNtop(AF_INET, &ptr->addr.sin_addr, clientIP, 16);
				wprintf(L"\n[TCP 서버] 클라이언트 접속: IP 주소 = %s, 포트번호 = %d, socket = 0x%016llX\n",
					clientIP, ntohs(ptr->addr.sin_port), ptr->sock);

				wprintf(L"PostRecv 종료\n");

				// 세션 정리
				ReleaseSession(ptr);

				return;
			}

		}

		if (GLERecv == WSA_IO_PENDING)
		{
			//wprintf(L"thread recv I/O PENDING\n");
		}

	}
	else if (retRecv == 0)
	{
		//wprintf(L"thread 동기 recv: %d\n", recvbytes);
	}

}
