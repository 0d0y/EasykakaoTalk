#include "EasykakaoTalkServer.h"

#pragma comment(lib, "ws2_32.lib")

EasykakaoTalkServer::EasykakaoTalkServer()
	: m_ThreadPool(4)
{
}

EasykakaoTalkServer::~EasykakaoTalkServer()
{
	m_ClientCheckThread.wait();

	Finalize();
}

void EasykakaoTalkServer::Run()
{
	if (!Initialize())
		return;

	m_ClientCheckThread = std::async(std::launch::async, &EasykakaoTalkServer::CheckClientEvent, this);

	CheckServerEvent();
}

int EasykakaoTalkServer::Initialize()
{
	WSADATA wsaData;
	SOCKET sock;
	SOCKADDR_IN serverAddress;
	int iResult = 0;

	//WSAStartup, Windows Socket API Startup Function
	//MAKEWORD(2, 2), Set Winsock Version 2.2
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (iResult != NO_ERROR)
	{
		std::cout << "WSAStartup() failed with error: " << iResult << std::endl;
		return -1;
	}
	else
	{
		std::cout << "WSAStartup() succeeded!" << std::endl;
	}

	//socket, Create Socket Function
	//Argument1, address family
	//AF_INET, IPv4
	//Argument2, Socket Type
	//SOCK_STREAM, use Internet Address Family, TCP
	//SOCK_DGRAM, use Internet Address Family, UDP
	//Argument3, Protocol
	//IPPROTO_TCP, TCP
	//IPPROTO_UDP, UDP
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		std::cout << "socket function failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return -1;
	}
	else
	{
		std::cout << "Socket created successfully." << std::endl;
	}

	// 서버 소켓을 위한 이벤트 객체 생성
	HANDLE serverEvent = WSACreateEvent();
	if (serverEvent == WSA_INVALID_EVENT)
	{
		std::cout << "WSACreateEvent() failed with error: " << WSAGetLastError() << std::endl;
		closesocket(sock);
		WSACleanup();
		return -1;
	}

	// FD_ACCEPT를 모니터링하기 위해 이벤트 객체를 서버 소켓과 연결
	if (WSAEventSelect(sock, serverEvent, FD_ACCEPT) == SOCKET_ERROR)
	{
		std::cout << "WSAEventSelect() failed with error: " << WSAGetLastError() << std::endl;
		closesocket(sock);
		WSACloseEvent(serverEvent);
		WSACleanup();
		return -1;
	}
	else
	{
		std::cout << "Server socket set to monitor FD_ACCEPT." << std::endl;
	}

	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(PORT);

	// ip와 port로 소켓 바인딩
	iResult = bind(sock, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "bind function failed with error: " << WSAGetLastError() << std::endl;

		iResult = closesocket(sock);
		if (iResult == SOCKET_ERROR)
			std::cout << "closesocket function failed with error: " << WSAGetLastError() << std::endl;

		WSACleanup();
		return -2;
	}
	else
	{
		std::cout << "Socket bound successfully." << std::endl;
	}


	// 소켓을 리슨 상태로 만듦
	if (listen(sock, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cout << "listen function failed with error: " << WSAGetLastError() << std::endl;
		return -3;
	}
	else
	{
		std::cout << "Socket listening on port " << PORT << std::endl;
	}

	// server socket info 저장
	m_ServerSocketInfo.sock = sock;
	m_ServerSocketInfo.event = serverEvent;
	m_ServerSocketInfo.name = "Server";
	m_ServerSocketInfo.addr = "0.0.0.0";

	if (sock == INVALID_SOCKET)
	{
		std::cerr << "Failed to initialize server. Exiting..." << std::endl;
		return -1;
	}
	else
	{
		std::cout << "Server initialized successfully." << std::endl;
	}

	return sock;
}

void EasykakaoTalkServer::Finalize()
{
	for (auto& sockInfo : m_ClientSocketInfoArray)
	{
		closesocket(sockInfo.sock);
		WSACloseEvent(sockInfo.event);
	}

	closesocket(m_ServerSocketInfo.sock);
	WSACloseEvent(m_ServerSocketInfo.event);

	WSACleanup();
}

void EasykakaoTalkServer::NotifyClients(std::string_view message)
{
	for (auto& socketInfo : m_ClientSocketInfoArray)
	{
		send(socketInfo.sock, message.data(), message.size(), 0);
	}
	std::cout << "Message sent to clients - " << message;
}

void EasykakaoTalkServer::AddClient()
{
	SOCKADDR_IN addr;
	int len = 0;
	SOCKET accept_sock;

	if (m_ClientSocketInfoArray.size() + 1 == FD_SETSIZE)
		return;
	else {
		len = sizeof(addr);
		memset(&addr, 0, sizeof(addr));
		accept_sock = accept(m_ServerSocketInfo.sock, (SOCKADDR*)&addr, &len);

		HANDLE event = WSACreateEvent();

		SocketInfo clientSocketInfo;
		clientSocketInfo.event = event;
		clientSocketInfo.sock = accept_sock;
		clientSocketInfo.uniqueID = GenerateUniqueID();

		if (WSAEventSelect(accept_sock, event, FD_READ | FD_CLOSE) == SOCKET_ERROR)
		{
			std::cerr << "WSAEventSelect function failed with error: " << WSAGetLastError() << std::endl;
			closesocket(accept_sock);
			WSACloseEvent(event);
			return;
		}

		wchar_t ip[INET_ADDRSTRLEN];
		InetNtop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);

		char narrowIp[INET_ADDRSTRLEN];
		size_t convertedChars = 0;
		wcstombs_s(&convertedChars, narrowIp, INET_ADDRSTRLEN, ip, _TRUNCATE);
		clientSocketInfo.addr = narrowIp;

		std::cout << "New client connection (IP : " << narrowIp << ")" << std::endl;
		m_ClientSocketInfoArray.push_back(clientSocketInfo);

		//char msg[256];
		//sprintf_s(msg, ">> 신규 클라이언트 접속(IP : %s)\n", narrowIp);
		//NotifyClients(msg);
	}
}

void EasykakaoTalkServer::ReadClient(SocketInfo& client)
{
	char buffer[1024];
	int bytesRead = recv(client.sock, buffer, sizeof(buffer) - 1, 0);
	if (bytesRead > 0) {
		buffer[bytesRead] = '\0';
		if (client.name.empty()) {
			client.name = buffer;
			std::string message = client.name + " 님 출현\n";
			NotifyClients(message);
		}
		else
		{
			std::string fullMessage = client.name + " : " + std::string(buffer) + "\n";
			NotifyClients(fullMessage);
			std::cout << "Read data from client with name " << client.name << ". Bytes read: " << bytesRead << std::endl;
		}
	}
	else {
		std::cerr << "Error reading data from client with unique ID " << client.uniqueID << std::endl;
	}
}

void EasykakaoTalkServer::RemoveClient(SocketInfo& client)
{
	auto it = std::find_if(m_ClientSocketInfoArray.begin(), m_ClientSocketInfoArray.end(), [&client](const SocketInfo& socket) {
		return  socket.uniqueID == client.uniqueID;
		});

	if (it != m_ClientSocketInfoArray.end()) {
		closesocket(it->sock);
		WSACloseEvent(it->event);
		std::cout << "Client with name " << it->name << " removed." << std::endl;
		m_ClientSocketInfoArray.erase(it);
	}
}

void EasykakaoTalkServer::ProcessServerEvent()
{
	WSANETWORKEVENTS ev;
	if (WSAEnumNetworkEvents(m_ServerSocketInfo.sock, m_ServerSocketInfo.event, &ev) != SOCKET_ERROR) {
		if (ev.lNetworkEvents & FD_ACCEPT) {
			AddClient();
		}
	}
}

void EasykakaoTalkServer::ProcessClientEvent(UniqueID uniqueID)
{
	std::lock_guard<std::mutex> lock(m_EventMutex);

	auto it = std::find_if(m_ClientSocketInfoArray.begin(), m_ClientSocketInfoArray.end(), [uniqueID](const SocketInfo& client) {
		return client.uniqueID == uniqueID;
		});

	if (it != m_ClientSocketInfoArray.end()) {
		WSANETWORKEVENTS ev;
		if (WSAEnumNetworkEvents(it->sock, it->event, &ev) == SOCKET_ERROR)
		{
			std::cerr << "WSAEnumNetworkEvents failed with error: " << WSAGetLastError() << std::endl;
			return;
		}

		if (ev.lNetworkEvents & FD_READ) {
			ReadClient(*it);
		}
		else if (ev.lNetworkEvents & FD_CLOSE) {
			RemoveClient(*it);
		}
	}
}

void EasykakaoTalkServer::CheckServerEvent()
{
	std::cout << "Waiting for event..." << std::endl;

	while (true)
	{
		DWORD serverEventIndex = WSAWaitForMultipleEvents(1, &m_ServerSocketInfo.event, false, INFINITE, false);
		if (serverEventIndex == WAIT_FAILED) {
			std::cerr << "WaitForServerEvent failed" << std::endl;
			return;
		}

		m_ThreadPool.enqueue([this] {
			ProcessServerEvent();
			});
	}
}

void EasykakaoTalkServer::CheckClientEvent()
{
	while (true)
	{
		for (const auto& client : m_ClientSocketInfoArray)
		{
			DWORD clientEventIndex = WSAWaitForMultipleEvents(1, &client.event, false, 0, false);

			if (clientEventIndex == WAIT_OBJECT_0)
			{
				m_ThreadPool.enqueue([this, uniqueID = client.uniqueID] {
					ProcessClientEvent(uniqueID);
					});
			}
		}
	}
}

int EasykakaoTalkServer::GenerateUniqueID()
{
	static int id = 0;
	return id++;
}