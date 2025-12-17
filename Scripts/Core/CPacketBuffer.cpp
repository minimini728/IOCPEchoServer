#include <Windows.h>
#include <iostream>
#include "CMemoryPool.h"
#include "CPacketBuffer.h"


CPacket::CPacket() :_bufferSize(CPacket::eBUFFER_DEFAULT), _dataSize(0), _buffer(new char[_bufferSize]), _front(0), _rear(0)
{

}
CPacket::CPacket(int bufferSize)
{
	_bufferSize = bufferSize;
	_buffer = new char[_bufferSize];
	_dataSize = 0;
	_front = 0;
	_rear = 0;
}
CPacket::~CPacket()
{
	delete[] _buffer;
}

void CPacket::Clear()
{
	_dataSize = 0;
	_front = 0;
	_rear = 0;
}

int CPacket::GetBufferSize(void) { return _bufferSize; }
int CPacket::GetDataSize(void) { return _dataSize; }
char* CPacket::GetBufferPtr(void) { return _buffer; }

int CPacket::MoveWritePos(int size)
{
	_dataSize += size;
	_rear += size;
	return size;
}

int CPacket::MoveReadPos(int size)
{
	_dataSize -= size;
	_front += size;
	return size;
}

// 연산자 오버로딩
CPacket& CPacket::operator = (CPacket& value)
{
	if (this == &value)
		return *this;

	// 기존 버퍼 삭제
	delete[] _buffer;

	_dataSize = value._bufferSize;
	_front = value._front;
	_rear = value._rear;
	_bufferSize = value._bufferSize;
	_buffer = new char[value._bufferSize];
	memcpy(_buffer, value._buffer, _bufferSize);

	return *this;
}
CPacket& CPacket::operator << (unsigned char value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;
}
CPacket& CPacket::operator << (char value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;

}

CPacket& CPacket::operator << (short value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;

}
CPacket& CPacket::operator << (unsigned short value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;

}

CPacket& CPacket::operator << (int value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;

}
CPacket& CPacket::operator << (unsigned int value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;

}

CPacket& CPacket::operator << (long value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;

}
CPacket& CPacket::operator << (unsigned long value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;

}

CPacket& CPacket::operator << (float value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;

}

CPacket& CPacket::operator << (__int64 value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;

}
CPacket& CPacket::operator << (double value)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + (int)sizeof(value) > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return *this;
	}

	memcpy(_buffer + _rear, &value, sizeof(value));
	_dataSize += sizeof(value);
	_rear += sizeof(value);

	return *this;

}

CPacket& CPacket::operator >> (BYTE& value)
{
	// 버퍼 언더플로우 방지
	if (_dataSize < (int)sizeof(value))
	{
		std::cout << "packet buffer underflow" << std::endl;
		return *this;
	}

	memcpy(&value, _buffer + _front, sizeof(value));
	_dataSize -= sizeof(value);
	_front += sizeof(value);

	return *this;
}
CPacket& CPacket::operator >> (char& value)
{
	// 버퍼 언더플로우 방지
	if (_dataSize < (int)sizeof(value))
	{
		std::cout << "packet buffer underflow" << std::endl;
		return *this;
	}

	memcpy(&value, _buffer + _front, sizeof(value));
	_dataSize -= sizeof(value);
	_front += sizeof(value);

	return *this;
}

CPacket& CPacket::operator >> (short& value)
{
	// 버퍼 언더플로우 방지
	if (_dataSize < (int)sizeof(value))
	{
		std::cout << "packet buffer underflow" << std::endl;
		return *this;
	}

	memcpy(&value, _buffer + _front, sizeof(value));
	_dataSize -= sizeof(value);
	_front += sizeof(value);

	return *this;
}
CPacket& CPacket::operator >> (WORD& value)
{
	// 버퍼 언더플로우 방지
	if (_dataSize < (int)sizeof(value))
	{
		std::cout << "packet buffer underflow" << std::endl;
		return *this;
	}

	memcpy(&value, _buffer + _front, sizeof(value));
	_dataSize -= sizeof(value);
	_front += sizeof(value);

	return *this;
}

CPacket& CPacket::operator >> (int& value)
{
	// 버퍼 언더플로우 방지
	if (_dataSize < (int)sizeof(value))
	{
		std::cout << "packet buffer underflow" << std::endl;
		return *this;
	}

	memcpy(&value, _buffer + _front, sizeof(value));
	_dataSize -= sizeof(value);
	_front += sizeof(value);

	return *this;
}
CPacket& CPacket::operator >> (DWORD& value)
{
	// 버퍼 언더플로우 방지
	if (_dataSize < (int)sizeof(value))
	{
		std::cout << "packet buffer underflow" << std::endl;
		return *this;
	}

	memcpy(&value, _buffer + _front, sizeof(value));
	_dataSize -= sizeof(value);
	_front += sizeof(value);

	return *this;
}
CPacket& CPacket::operator >> (float& value)
{
	// 버퍼 언더플로우 방지
	if (_dataSize < (int)sizeof(value))
	{
		std::cout << "packet buffer underflow" << std::endl;
		return *this;
	}

	memcpy(&value, _buffer + _front, sizeof(value));
	_dataSize -= sizeof(value);
	_front += sizeof(value);

	return *this;
}

CPacket& CPacket::operator >> (__int64& value)
{
	// 버퍼 언더플로우 방지
	if (_dataSize < (int)sizeof(value))
	{
		std::cout << "packet buffer underflow" << std::endl;
		return *this;
	}

	memcpy(&value, _buffer + _front, sizeof(value));
	_dataSize -= sizeof(value);
	_front += sizeof(value);

	return *this;
}
CPacket& CPacket::operator >> (double& value)
{
	// 버퍼 언더플로우 방지
	if (_dataSize < (int)sizeof(value))
	{
		std::cout << "packet buffer underflow" << std::endl;
		return *this;
	}

	memcpy(&value, _buffer + _front, sizeof(value));
	_dataSize -= sizeof(value);
	_front += sizeof(value);

	return *this;
}

// 데이터 얻기
int CPacket::GetData(char* dest, int size)
{
	// 버퍼 언더플로우 방지
	if (_dataSize < size)
	{
		std::cout << "packet buffer underflow" << std::endl;
		return 0;
	}

	memcpy(dest, _buffer + _front, size);
	_dataSize -= size;
	_front += size;

	return size;
}

// 데이터 삽입
int CPacket::PutData(char* dest, int srcSize)
{
	// 버퍼 오버플로우 방지
	if (_dataSize + srcSize > _bufferSize)
	{
		std::cout << "packet buffer overflow" << std::endl;
		return 0;
	}

	memcpy(_buffer + _rear, dest, srcSize);

	_dataSize += srcSize;
	_rear += srcSize;

	return srcSize;

}

void CPacket::PrintData()
{
	for (int i = 0; i < _dataSize; i++)  // _dataSize + srcSize 만큼 반복
	{
		printf("%02X ", static_cast<unsigned char>(_buffer[i]));  // 한 바이트씩 16진수 출력
	}
	std::cout << std::endl;

}

SRWLOCK CPacket::_srwlPool = SRWLOCK_INIT;
CMemoryPool<CPacket> CPacket::_packetPool;

CPacket* CPacket::Alloc()
{
	CPacket* newPacket = nullptr;
	//AcquireSRWLockExclusive(&_srwlPool);
	newPacket =  _packetPool.Alloc();
	//ReleaseSRWLockExclusive(&_srwlPool);

	if (newPacket == nullptr)
		DebugBreak();

	newPacket->Clear();
	return newPacket;
}

void CPacket::Free(CPacket* packet)
{
	//AcquireSRWLockExclusive(&_srwlPool);
	_packetPool.Free(packet);
	//ReleaseSRWLockExclusive(&_srwlPool);
}

