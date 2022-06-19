#include "Manager.h"
int Manager::SendMsg(SOCKET sock, UPACKET& packet)
{
	char* pMsg = (char*)&packet;
	int iSendSize = 0;
	do {
		//이전에 전송한 길이 이후의 배열을 전송
		int iSendByte = send(sock, &pMsg[iSendSize],
			packet.ph.len - iSendSize, 0);
		if (iSendByte == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				return -1;
			}
		}
		iSendSize += iSendByte;
		//전송한 크기가 패킷의 길이보다 커질때까지 반복
	} while (iSendSize < packet.ph.len);
	return iSendSize;
}
int Manager::SendMsg(SOCKET sock, char* msg, WORD type)
{
	UPACKET packet;
	ZeroMemory(&packet, sizeof(packet));
	packet.ph.len = strlen(msg) + PACKET_HEADER_SIZE;
	packet.ph.type = type;
	memcpy(packet.msg, msg, strlen(msg));
	char* pMsg = (char*)&packet;
	int iSendSize = 0;
	do {
		int iSendByte = send(sock, &pMsg[iSendSize],
			packet.ph.len - iSendSize, 0);
		if (iSendByte == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				return -1;
			}
		}
		iSendSize += iSendByte;
	} while (iSendSize < packet.ph.len);
	return iSendSize;
}
int Manager::LogoutMsg(NetUser nUser, NetUser& user)
{
	Packet pac(PACKET_CHAT_MSG);
	pac << 1 << user.m_ChattName << (short)1 << " 님이 나갔습니다";
	SendMsg(nUser.m_Socket, pac.m_uPacket);
	return 0;
}
bool Manager::AddUser(SOCKET sock)
{
	SOCKADDR_IN clientAddr;
	int iLen = sizeof(clientAddr);
	SOCKET clientSock = accept(sock,
		(sockaddr*)&clientAddr, &iLen);
	u_long on = 1;
	ioctlsocket(clientSock, FIONBIO, &on);
	if (clientSock == SOCKET_ERROR)
	{
		int iError = WSAGetLastError();
		if (iError != WSAEWOULDBLOCK)
		{
			std::cout << "ErrorCode=" << iError << std::endl;
			return true;
		}
	}
	else
	{
		char i;
		cout
			<< "ip =" << inet_ntoa(clientAddr.sin_addr)
			<< "port =" << ntohs(clientAddr.sin_port)
			<< "  접속 요청중(Y/N)" << endl;
		cin >> i;
		if (i=='y'||i=='Y')
		{
			NetUser user;
			user.set(clientSock, clientAddr);
			WaitForSingleObject(m_Mutex, INFINITE);
			RecvUser(user);
			TChatMsg data;
			user.m_packetPool.front() >> data.index >> data.name >> data.damage >> data.message;
			memcpy(&user.m_ChattName,data.name, sizeof(data.name));
			UserList.push_back(user);
			std::cout << UserList.size() << " 명 접속중.." << std::endl;
			//RecvUser(user);
			list<NetUser>::iterator userIter;
			for (userIter = UserList.begin();
				userIter != UserList.end();)
			{
				int iRet = Broadcast(*userIter);
				if (iRet <= 0)
				{
					userIter = UserList.erase(userIter);
				}
				else
				{
					userIter++;
				}
			}
			ReleaseMutex(m_Mutex);
		}
		else
		{
			char Buffer[] = "접속 요청 거부";
			SendMsg(clientSock, Buffer, PACKET_SYSTEM_MSG);
			closesocket(clientSock);
			return true;
		}
	}
	return true;
}
int Manager::RecvUser(NetUser& user)
{
	char szRecvBuffer[1024] = { 0, };
	int iRecvByte = recv(user.m_Socket, szRecvBuffer, 1024, 0);
	if (iRecvByte == 0)
	{
		return 0;
	}
	if (iRecvByte == SOCKET_ERROR)
	{
		int iError = WSAGetLastError();
		if (iError != WSAEWOULDBLOCK)
		{
			return -1;
		}
		return 2;
	}
	user.DispatchRead(szRecvBuffer, iRecvByte);
	return 1;
}
int Manager::Broadcast(NetUser& user)
{
	if (user.m_packetPool.size() > 0)//패킷이 존재할때
	{
		list<Packet>::iterator iter;
		//패킷 풀의 내용들을 순회
		for (iter = user.m_packetPool.begin(); iter != user.m_packetPool.end();)
		{
			//유저 리스트를 순회
			for (NetUser& send : UserList)
			{
				//해당 유저의 소캣을 통해 패킷 전송
				int Ret = SendMsg(send.m_Socket, (*iter).m_uPacket);
				if (Ret <= 0)
				{
					send.m_Connect = false;
				}
			}
			//다음 전송을 위해 패킷 풀을 비움
			iter = user.m_packetPool.erase(iter);
		}
	}
	return 1;
}