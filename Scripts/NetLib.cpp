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
#include "CEchoServer.h"
#include "CEchoContent.h"
#include "Profiler.h"

#pragma comment (lib, "winmm.lib")
#include <locale.h>
#include <fcntl.h>
#include <io.h>


int main()
{
    timeBeginPeriod(1);

    _setmode(_fileno(stdin), _O_U16TEXT);   // 입력을 UTF-16으로 설정
    _setmode(_fileno(stdout), _O_U16TEXT);  // 출력을 UTF-16으로 설정

    CEchoServer server;
    CEchoContent content;

    server.AttachContent(&content);
    content.AttachServer(&server);

    bool isServerStart = server.Start(10, 4, false, false, 100);
    if (!isServerStart)
    {
        wprintf(L"[main] server start fail\n");
        return 0;
    }
    content.Init();

    bool prevS = false;

    while (1)
    {
        bool curS = (GetAsyncKeyState('Q'));

        if (curS && !prevS)
        {
            wprintf(L"서버 종료 절차 시작\n");
            server.Stop();
            content.Stop();

            break;
        }
        prevS = curS;

        Sleep(50);

    }


}
