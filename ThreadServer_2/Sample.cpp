#include "Manager.h"

DWORD WINAPI RecvThread(LPVOID param)
{
	Manager* mgr = (Manager*)param;
	while (1)
	{
		//임계구역 생성, 임계구역해제가 이루어지기 전까지 다른 쓰레드는 대기상태가 된다
		WaitForSingleObject(mgr->m_Mutex, INFINITE);

		list<NetUser>::iterator userIter;
		for (userIter = mgr->UserList.begin();
			userIter != mgr->UserList.end();)
		{
			//유저리스트를 순회하며 패킷을 받아온다
			int iRet = mgr->RecvUser(*userIter);
			if (iRet <= 0)
			{
					for (NetUser& send : mgr->UserList)
					{
						mgr->LogoutMsg(send, *userIter);
					}
				userIter = mgr->UserList.erase(userIter);
			}
			else
			{
				userIter++;
			}
		}
		//임계구역 해제
		ReleaseMutex(mgr->m_Mutex);
	}
}
DWORD WINAPI SendThread(LPVOID param)
{
	Manager* mgr = (Manager*)param;
	while (1)
	{
		WaitForSingleObject(mgr->m_Mutex, INFINITE);
		list<NetUser>::iterator userIter;
		for (userIter = mgr->UserList.begin();
			userIter != mgr->UserList.end();)
		{
			int iRet = mgr->Broadcast(*userIter);
			if (iRet <= 0)
			{
				userIter = mgr->UserList.erase(userIter);
			}
			else
			{
				userIter++;
			}
		}

		ReleaseMutex(mgr->m_Mutex);

	}
}

int main()
{
	Manager mgr;
	mgr.m_Mutex = CreateMutex(NULL, FALSE, NULL);
	Network Net;
	Net.InitNetwork();
	Net.InitServer(SOCK_STREAM, 10000, 1);
	cout << "서버 가동" << endl;

	DWORD RecvThreadId;
	DWORD SendThreadId;
	//쓰레드 핸들 생성
	HANDLE RecvThreadHandle = ::CreateThread(0, 0, RecvThread, (LPVOID)&mgr, 0, &RecvThreadId);
	CloseHandle(RecvThreadHandle);
	HANDLE SendThreadHandle = ::CreateThread(0, 0, SendThread, (LPVOID)&mgr, 0, &SendThreadId);
	CloseHandle(SendThreadHandle);
	
	while (1)
	{
			mgr.AddUser(Net.m_netSock);

	}
}