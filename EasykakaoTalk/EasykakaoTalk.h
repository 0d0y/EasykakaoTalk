#pragma once

#include <string>
#include <winsock2.h>

class EasykakaoTalk {
public:
    EasykakaoTalk(const std::string& serverIP, int port);
    ~EasykakaoTalk();

    void Run();

private:
    std::string m_ServerIP;
    int m_Port;
    SOCKET m_ClientSocket;
    std::string m_Nickname;

    int Initialize();
    int ConnectToServer();
    void GetNickname();
    void MainLoop();

    void SendMessagesToServer();
    void ReceiveMessagesFromServer();
};