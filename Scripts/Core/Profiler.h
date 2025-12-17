#pragma once
#define MAXTAGCNT 15

struct ProfileInfo
{
	bool flag = false; // 사용 유무
	bool inProgress = false; // begin-end 짝 맞는지 확인용
	const wchar_t* tag = nullptr; // 이름
	LARGE_INTEGER startTime = {};
	__int64 call = 0; // 함수 호출 횟수
	__int64 totalTime = 0;
	__int64 max = 0;
	__int64 min = 0;
};

void ProfileBegin(const std::wstring& tag);
void ProfileEnd(const std::wstring& tag);
void ProfileDataOutText(const wchar_t* fileName);
void ProfileReset();
void ProfileDataOut();