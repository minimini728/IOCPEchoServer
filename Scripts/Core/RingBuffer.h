#pragma once
class RingBuffer
{
public:
	unsigned int _bufferSize;
	char* _buffer;
	volatile unsigned int _head;
	volatile unsigned int _rear;

public:
	RingBuffer();
	~RingBuffer();
	RingBuffer(int bufferSize);

	int GetBufferSize();
	int GetUseSize();
	int GetFreeSize();
	int Enqueue(const char* data, int size);
	int Dequeue(char* dest, int size);
	int Peek(char* dest, int size);
	void MoveFront(int size);
	void MoveRear(int size);
	void ClearBuffer();
	void PrintBuffer();
	char* GetFrontBufferPtr(void);
	char* GetRearBufferPtr(void);
	char* GetBufferBasePtr(void);
	int DirectEnqueueSize(void);
	int DirectDequeueSize(void);
};