#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <process.h>


#pragma comment(lib, "Ws2_32.lib") //라이브러리 파일 Ws2_32.lib에 연결되는지 확인한다.
#define DEFAULT_PORT "4444"
#define DEFAULT_BUFLEN 4096
#define MAX_THREADS 10

typedef struct ThreadArgs{
	SOCKET ClientSocket;
	SOCKADDR_IN clientAddr;
}ThreadArgs;

int threadNum = 0;
HANDLE  hThreads[MAX_THREADS] = { NULL };
BOOL isActive[MAX_THREADS] = { FALSE };
ThreadArgs tArgs[MAX_THREADS];

int requestHandler(int index)
{
	char recvbuf[DEFAULT_BUFLEN];
	int iSendResult;
	int recvbuflen = DEFAULT_BUFLEN;
	int iResult;
	SOCKET ClientSocket = tArgs[index].ClientSocket;
	char remoteAddr[20];

	inet_ntop(AF_INET, &tArgs[index].clientAddr.sin_addr, remoteAddr, sizeof(remoteAddr));
	printf("accept Client %s threadNum %d\n", remoteAddr, index);


	do {
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			printf("Bytes recevied: %d\n", iResult);
			iSendResult = send(ClientSocket, recvbuf, iResult, 0);
			if (iSendResult == SOCKET_ERROR) {
				printf("send failed: %d\n", WSAGetLastError());
				closesocket(ClientSocket);
				return 1;
			}
			printf("Bytes sent: %d\n", iSendResult);
		}
		else if (iResult == 0)
			printf("Connection closing...\n");
		else {
			printf("recv failed %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			return 1;
		}

	} while (iResult > 0);

	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return 1;
	}
	closesocket(ClientSocket);
	isActive[index] = FALSE;
	_endthread();

	return 0;
}

int main() {
	int iResult;
	WSADATA wsaData;
	int addrlen = sizeof(struct sockaddr);

	// Winsock 초기화
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	//서버에서 사용할 SOCKET 개체를 인스턴스화
	struct addrinfo* result = NULL, * ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET; //IPv4
	hints.ai_socktype = SOCK_STREAM; //stream socket
	hints.ai_protocol = IPPROTO_TCP; //TCP 프로토콜 사용
	hints.ai_flags = AI_PASSIVE; //

	// Resolve the local address and port to be used by the server
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	SOCKET ListenSocket = INVALID_SOCKET;

	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	if (ListenSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	freeaddrinfo(result);

	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
		printf("Listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}


	while (1)
	{
		int index = -1;
		for(int i=0; i<MAX_THREADS; i++)
			if (!isActive[i])
			{
				index = i;
				break;
			}
		if(index != -1)
		{
			tArgs[index].ClientSocket = INVALID_SOCKET;
			tArgs[index].ClientSocket = accept(ListenSocket, &(tArgs[index].clientAddr), &addrlen);

			if (tArgs[index].ClientSocket == INVALID_SOCKET)
				printf("accept failed: %d\n", WSAGetLastError());
			else
			{
				isActive[index] = TRUE;
				hThreads[index] =  _beginthread(requestHandler, 0, index);
			}
		}
		else //exceed max thread
		{
			WaitForMultipleObjects(10, hThreads, FALSE, 1000);
		}
	}
	closesocket(ListenSocket);
	WSACleanup();
	return 0;

}