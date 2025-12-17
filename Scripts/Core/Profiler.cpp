#include <iostream>
#include <windows.h>
#include <string>
#include <cstdio>
#include "Profiler.h"
#pragma comment(lib, "winmm.lib")
#include <time.h>
#include <unordered_map>

using namespace std;

//ProfileInfo profiler[MAXTAGCNT];

struct ThreadBucket
{
	DWORD _tid = 0;
	SRWLOCK _lock = SRWLOCK_INIT;
	unordered_map<wstring, ProfileInfo*> _tagMap;
};

static thread_local ThreadBucket* st_bucket = nullptr; // 정적 tls
unordered_map<DWORD, ThreadBucket*> profileThreads; // 스레드ID별 tag들 보관
SRWLOCK srwlProfileTh = SRWLOCK_INIT;


ThreadBucket* GetOrCreateBucket()
{
	if (st_bucket) return st_bucket;
	ThreadBucket* newBucket = new ThreadBucket();
	newBucket->_tid = GetCurrentThreadId();

	AcquireSRWLockExclusive(&srwlProfileTh);
	profileThreads.emplace(newBucket->_tid, newBucket);
	ReleaseSRWLockExclusive(&srwlProfileTh);

	st_bucket = newBucket;
	return newBucket;
}

void ProfileBegin(const wstring& tag)
{
	ThreadBucket* bucket = GetOrCreateBucket();


	ProfileInfo* info = nullptr;
	auto iter = bucket->_tagMap.find(tag);
	if (iter == bucket->_tagMap.end()) // 버킷 생성
	{
		info = new ProfileInfo();
		info->flag = true;
		info->inProgress = true;
		info->min = LLONG_MAX;
		QueryPerformanceCounter(&info->startTime);
		info->call = 1;

		bucket->_tagMap.emplace(tag, info);
	}
	else // 이미 있으면 갱신
	{
		info = iter->second;
		if (info->inProgress)
		{
			wprintf(L"[ProfileBegin] (%s) Begin End Not Matching\n", tag.c_str());
			return;
		}

		info->inProgress = true;
		info->call++;
		QueryPerformanceCounter(&info->startTime);
	}

}

void ProfileEnd(const wstring& tag)
{
	ThreadBucket* bucket = st_bucket;


	if (!bucket)
	{
		wprintf(L"[ProfileEnd] (%s) 정적 tls 못 찾음\n", tag.c_str());
		return;
	}

	auto iter = bucket->_tagMap.find(tag);
	if (iter == bucket->_tagMap.end())
	{
		wprintf(L"[ProfileEnd] (%s) tag랑 매치되는 ProfileInfo 못 찾음\n", tag.c_str());
		return;
	}

	ProfileInfo* info = iter->second;
	if (!info->inProgress)
	{
		wprintf(L"[ProfileEnd] (%s)Begin End Not Matching\n", tag.c_str());
		return;
	}

	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);


	__int64 elapse = end.QuadPart - info->startTime.QuadPart;

	info->totalTime += elapse;

	info->max = elapse > info->max ? elapse : info->max;
	info->min = elapse < info->min ? elapse : info->min;

	info->inProgress = false;
	return;

}

void ProfileDataOutText(const wchar_t* fileName)
{
	FILE* file = nullptr;
	errno_t err = _wfopen_s(&file, fileName, L"w, ccs=UNICODE");

	if (!file || err != 0)
	{
		wprintf(L"file open error!\n");
		return;
	}

	// 타이머 주파수
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	// 헤더
	fwprintf(file, L"%-10s| %-20s| %-15s| %-15s| %-15s| %-10s\n",
		L"ThreadID", L"Name", L"Average(µs)", L"Min(µs)", L"Max(µs)", L"Call");

	// 스레드 목록 순회
	AcquireSRWLockShared(&srwlProfileTh);
	for (const auto& [tid, bucket] : profileThreads)
	{
		if (!bucket) continue;

		AcquireSRWLockShared(&bucket->_lock);

		for (const auto& [tag, info] : bucket->_tagMap)
		{
			if (!info || !info->flag) continue;

			// 평균 계산 (Min, Max 제외)
			double average = info->call > 2
				? static_cast<double>(info->totalTime - info->min - info->max)
				/ (info->call - 2) * 1'000'000.0 / freq.QuadPart
				: 0.0;

			// 최대, 최소
			double minTime = static_cast<double>(info->min) * 1'000'000.0 / freq.QuadPart;
			double maxTime = static_cast<double>(info->max) * 1'000'000.0 / freq.QuadPart;

			// 출력
			fwprintf(file, L"%-10u| %-20s| %-15.4f| %-15.4f| %-15.4f| %-10lld\n",
				tid, tag.c_str(), average, minTime, maxTime, info->call);
		}

		ReleaseSRWLockShared(&bucket->_lock);
	}
	ReleaseSRWLockShared(&srwlProfileTh);

	fclose(file);
}

void ProfileReset()
{
	AcquireSRWLockExclusive(&srwlProfileTh);
	for (auto& [tid, bucket] : profileThreads)
	{
		AcquireSRWLockExclusive(&bucket->_lock);
		for (auto& infoIter : bucket->_tagMap)
		{
			ProfileInfo* info = infoIter.second;
			if (!info) continue;
			info->call = 0;
			info->totalTime = 0;
			info->max = 0;
			info->min = LLONG_MAX;
		}
		ReleaseSRWLockExclusive(&bucket->_lock);
	}
	ReleaseSRWLockExclusive(&srwlProfileTh);
}

void ProfileDataOut()
{
	time_t currentTime = time(NULL);
	struct tm localTime;
	wchar_t formatTime[100] = { 0 };

	if (localtime_s(&localTime, &currentTime) == 0)
	{
		// 문자열 조합
		swprintf_s(formatTime, L"PROFILE_%04d%02d%02d_%02d%02d%02d.txt",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec);

		wprintf(L"포맷된 시간: %s\n", formatTime);
	}
	else
	{
		fwprintf(stderr, L"시간 변환에 실패했습니다.\n");
	}


	ProfileDataOutText(formatTime);
}