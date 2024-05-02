#include "EasykakaoTalk.h"
#include <iostream>
#include <future>
#include <string>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

EasykakaoTalk::EasykakaoTalk(const std::string& serverIP, int port)
	: m_ServerIP(serverIP), m_Port(port), m_ClientSocket(INVALID_SOCKET)
{
}

EasykakaoTalk::~EasykakaoTalk()
{
	WSACleanup();
}

void EasykakaoTalk::Run()
{
	if (Initialize() != 0) {
		std::cerr << "Failed to initialize client. Exiting...\n";
		return;
	}

	if (ConnectToServer() != 0) {
		std::cerr << "Failed to connect to server. Exiting...\n";
		return;
	}

	GetNickname();

	MainLoop();
}

int EasykakaoTalk::Initialize()
{
	WSADATA wsaData;
	return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

int EasykakaoTalk::ConnectToServer()
{
	m_ClientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_ClientSocket == INVALID_SOCKET) {
		return -1;
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	//serverAddr.sin_addr.s_addr = inet_addr(m_ServerIP.c_str());
	inet_pton(AF_INET, m_ServerIP.c_str(), &serverAddr.sin_addr); //inet_addr 대신에 더 안전한 inet_pton 사용
	serverAddr.sin_port = htons(m_Port);

	if (connect(m_ClientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
		closesocket(m_ClientSocket);
		return -1;
	}

	return 0;
}

void EasykakaoTalk::GetNickname()
{
	std::cout << "Enter your nickname: ";
	std::getline(std::cin, m_Nickname);
}

void EasykakaoTalk::MainLoop()
{
	send(m_ClientSocket, m_Nickname.c_str(), m_Nickname.size(), 0);

	auto senderTask = std::async(std::launch::async, &EasykakaoTalk::SendMessagesToServer, this);
	auto receiverTask = std::async(std::launch::async, &EasykakaoTalk::ReceiveMessagesFromServer, this);

	senderTask.get();
	receiverTask.get();
}

void EasykakaoTalk::SendMessagesToServer()
{
	std::string message;
	while (true)
	{
		std::getline(std::cin, message);

		if (message.empty()) {
			continue;
		}

		send(m_ClientSocket, message.c_str(), message.size(), 0);
	}
}

void EasykakaoTalk::ReceiveMessagesFromServer()
{
	char buffer[1024];
	int bytesReceived;
	while (true)
	{
		bytesReceived = recv(m_ClientSocket, buffer, sizeof(buffer) - 1, 0);
		if (bytesReceived > 0) {
			buffer[bytesReceived] = '\0';
			std::cout << buffer;
		}
		else if (bytesReceived == 0) {
			std::cerr << "Server disconnected." << std::endl;
			break;
		}
		else {
			std::cerr << "Error receiving data from server." << std::endl;
			break;
		}
	}
}

