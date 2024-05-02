#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <list>
#include <queue>
#include <vector>
#include <future>
#include <mutex>
#include "ThreadPool.h"

constexpr const UINT PORT = 7777;

using UniqueID = int;

class EasykakaoTalkServer
{
private:
	struct SocketInfo
	{
		SOCKET sock;
		HANDLE event;
		std::string name;
		std::string addr;
		UniqueID uniqueID;
	};

public:
	EasykakaoTalkServer();
	~EasykakaoTalkServer();

public:
	void Run();

private:
	int Initialize();
	void Finalize();
	void AddClient();
	void ReadClient(SocketInfo& client);
	void RemoveClient(SocketInfo& client);
	void NotifyClients(std::string_view message);

	void ProcessServerEvent();
	void ProcessClientEvent(UniqueID uniqueID);
	void CheckServerEvent();
	void CheckClientEvent();

	int GenerateUniqueID();

private:
	SocketInfo m_ServerSocketInfo;
	std::vector<SocketInfo> m_ClientSocketInfoArray;

	std::future<void> m_ClientCheckThread;
	ThreadPool m_ThreadPool;

	std::mutex m_EventMutex;

};

