#include "RingBuffer.h"
#include <string.h>
#include <windows.h>
#include <iostream>
#include <emmintrin.h>

RingBuffer::RingBuffer() :_bufferSize(100000), _buffer(new char[_bufferSize]), _head(0), _rear(0)
{

}

RingBuffer::~RingBuffer()
{
	//wprintf(L"~ringbuffer 호출됨\n");
	delete[](_buffer);
}

RingBuffer::RingBuffer(int bufferSize)
{
	_bufferSize = bufferSize;
	_buffer = new char[_bufferSize];
	_head = 0;
	_rear = 0;
}

int RingBuffer::GetBufferSize()
{
	return _bufferSize;
}

int RingBuffer::GetUseSize()
{
	return (_rear + _bufferSize - _head) % _bufferSize;
}

int RingBuffer::GetFreeSize()
{
	int retValue = _bufferSize - GetUseSize() - 1;
	if (retValue < 0)
	{
		printf("[GetFreeSize] _head: %d\t _rear: %d\t retValue: %d\n", _head, _rear, retValue);
		DebugBreak();

	}
	return retValue;
}

int RingBuffer::Enqueue(const char* data, int size)
{
	int freeSize = GetFreeSize();
	unsigned int rear = _rear;

	if (size > freeSize)
	{
		wprintf(L"[Ringbuffer] Enqueue full fail\n");
		return 0;
		DebugBreak();
	}


	int firstCopySize;
	if (size <= _bufferSize - rear)
	{
		firstCopySize = size;
		memcpy(_buffer + rear, data, firstCopySize);
	}
	else
	{
		firstCopySize = _bufferSize - rear;
		memcpy(_buffer + rear, data, firstCopySize);

		int remainSize = size - firstCopySize;
		memcpy(_buffer, data + firstCopySize, remainSize);
	}

	_rear = (rear + size) % _bufferSize;

	return size;
}

int RingBuffer::Dequeue(char* dest, int size)
{
	unsigned int head = _head;

	int useSize = GetUseSize();

	if (size > useSize)
	{
		wprintf(L"[RingBuffer] Dequeue fail\n");
		return 0;
		DebugBreak();
	}


	int firstCopySize;
	if (size <= _bufferSize - head)
	{
		firstCopySize = size;
		memcpy(dest, _buffer + head, firstCopySize);
	}
	else
	{
		firstCopySize = _bufferSize - head;
		memcpy(dest, _buffer + head, firstCopySize);

		int remainSize = size - firstCopySize;
		memcpy(dest + firstCopySize, _buffer, remainSize);
	}

	_head = (head + size) % _bufferSize;

	// 버퍼가 비면 0으로 초기화
	//if (_head == _rear)
	//{
	//	_head = _rear = 0;
	//}

	return size;
}

int RingBuffer::Peek(char* dest, int size)
{
	unsigned int head = _head;

	int useSize = GetUseSize();
	if (size > useSize)
	{
		size = useSize;
		wprintf(L"[RingBuffer] Peek fail\n");
		return 0;
		DebugBreak();

	}

	int firstCopySize;
	if (size < _bufferSize - head)
		firstCopySize = size;
	else
		firstCopySize = _bufferSize - head;

	memcpy(dest, _buffer + head, firstCopySize);

	int remainSize = size - firstCopySize;
	if (remainSize > 0)
	{
		memcpy(dest + firstCopySize, _buffer, remainSize);
	}

	return size;
}

void RingBuffer::MoveFront(int size)
{
	_head = (_head + size) % _bufferSize;
}

void RingBuffer::MoveRear(int size)
{
	_rear = (_rear + size) % _bufferSize;
}

void RingBuffer::ClearBuffer()
{
	_head = 0;
	_rear = 0;
}

void RingBuffer::PrintBuffer()
{
	std::cout << "[ Ring Buffer State ] _head: " << _head << ", _rear: " << _rear << std::endl;
	std::cout << "Data in Buffer: ";

	int useSize = GetUseSize();
	if (useSize == 0)
	{
		std::cout << "(empty)" << std::endl;
		return;
	}

	int index = _head;
	for (int i = 0; i < useSize; i++)
	{
		std::cout << (int)_buffer[index] << " ";
		index = (index + 1) % _bufferSize;  // 원형 구조 적용
	}

	std::cout << std::endl;
}

char* RingBuffer::GetFrontBufferPtr(void)
{
	return &_buffer[_head];
}

char* RingBuffer::GetRearBufferPtr(void)
{
	return &_buffer[_rear];
}

char* RingBuffer::GetBufferBasePtr(void)
{
	return _buffer;
}


int RingBuffer::DirectEnqueueSize(void)
{
	unsigned int head = _head;
	unsigned int rear = _rear;

	if (rear >= head)
	{
		if (head == 0)
			return _bufferSize - rear - 1; // full 버퍼 구분을 위해 한 칸 비워둠
		else
			return _bufferSize - rear;
	}
	else
	{
		return head - rear - 1;
	}

}

int RingBuffer::DirectDequeueSize(void)
{
	unsigned int head = _head;
	unsigned int rear = _rear;

	if (head <= rear)
		return rear - head;
	else
		return _bufferSize - head;

}
