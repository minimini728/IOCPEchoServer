#pragma once
class CPacket
{
public:
	/*---------------------------------------------------------------
	Packet Enum.
	----------------------------------------------------------------*/
	enum en_PACKET
	{
		eBUFFER_DEFAULT = 2000		// 패킷의 기본 버퍼 사이즈.
	};

	// -------------------------------------------------------------------
	// 생성자, 파괴자.
	// -------------------------------------------------------------------
	CPacket();
	CPacket(int iBufferSize);

	virtual	~CPacket();

	void	Clear(void);

	// 버퍼 사이즈 얻기.
	int	GetBufferSize(void);

	// 현재 사용중인 사이즈 얻기.
	int	GetDataSize(void);

	// 버퍼 포인터 얻기.
	char* GetBufferPtr(void);

	//--------------------------------------------------------------------------
	// 버퍼 Pos 이동. (음수이동은 안됨)
	// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//-------------------------------------------------------------------------
	int		MoveWritePos(int iSize);
	int		MoveReadPos(int iSize);


	/* ============================================================================= */
	// 연산자 오버로딩
	/* ============================================================================= */
	CPacket& operator = (CPacket& clSrcPacket);

	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	CPacket& operator << (unsigned char byValue);
	CPacket& operator << (char chValue);

	CPacket& operator << (short shValue);
	CPacket& operator << (unsigned short wValue);

	CPacket& operator << (int iValue);
	CPacket& operator << (unsigned int value);
	CPacket& operator << (long lValue);
	CPacket& operator << (unsigned long value);
	CPacket& operator << (float fValue);

	CPacket& operator << (__int64 iValue);
	CPacket& operator << (double dValue);


	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	CPacket& operator >> (BYTE& byValue);
	CPacket& operator >> (char& chValue);

	CPacket& operator >> (short& shValue);
	CPacket& operator >> (WORD& wValue);

	CPacket& operator >> (int& iValue);
	CPacket& operator >> (DWORD& dwValue);
	CPacket& operator >> (float& fValue);

	CPacket& operator >> (__int64& iValue);
	CPacket& operator >> (double& dValue);

	//-----------------------------------------------------------------------
	// 데이타 얻기.
	// Parameters: (char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//------------------------------------------------------------------------
	int		GetData(char* chpDest, int iSize);

	//------------------------------------------------------------------------
	// 데이타 삽입.
	// Parameters: (char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//------------------------------------------------------------------------
	int		PutData(char* chpSrc, int iSrcSize);

	// 데이터 출력
	void PrintData();

	// 메모리풀에서 꺼내기
	static CPacket* Alloc();
	// 메모리풀에 반납
	static void Free(CPacket*);

protected:
	int	_bufferSize;
	int	_dataSize; // 현재 버퍼에 사용중인 사이즈

private:
	char* _buffer;
	int _front;
	int _rear;
	static SRWLOCK _srwlPool;
	static CMemoryPool<CPacket> _packetPool;
};