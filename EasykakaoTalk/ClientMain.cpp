#include "EasykakaoTalk.h"

constexpr const char* SERVER_IP = "172.16.1.56";
constexpr const UINT PORT = 7777;

int main()
{
	EasykakaoTalk client(SERVER_IP, PORT);

	client.Run();
}
