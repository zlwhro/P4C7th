#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <process.h>
#include "httpApp.h"

#pragma comment(lib, "Ws2_32.lib")
#define DEFAULT_PORT "4444"
#define DEFAULT_BUFLEN 4096
#define MAX_THREADS 10


typedef struct ThreadArgs {
	SOCKET ClientSocket;
	SOCKADDR_IN clientAddr;
}ThreadArgs;


HANDLE  hThreads[MAX_THREADS] = { NULL };


BOOL isActive[MAX_THREADS] = { FALSE };


ThreadArgs tArgs[MAX_THREADS];



void requestHandler(int index);

int main(int argc, char* argv[]) {
	int iResult;
	WSADATA wsaData;
	int addrlen = sizeof(struct sockaddr);
	char* address = NULL;
	char* port = DEFAULT_PORT;
	webroot = NULL;

	if (argc > 2)
	{
		int i = 1;
		while (i < argc - 1)
		{
			if (strcmp(argv[i], "-i") == 0)
				address = argv[i + 1];
			else if (strcmp(argv[i], "-p") == 0)
				port = argv[i + 1];
			else if (strcmp(argv[i], "-d") == 0)
				webroot = argv[i + 1];
			else
			{
				printf("Usage\nserver.exe -i {IP address} -p {port} -d {work Directory}\n");
				return 1;
			}
			i += 2;
		}
	}
	if (webroot == NULL)
	{
		printf("please input work Directory\n");
		printf("Usage\nserver.exe -i {IP address} -p {port} -d {work Directory}\n");
		return 1;
	}



	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}


	struct addrinfo* result = NULL, * ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;


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


	if (listen(ListenSocket, MAX_THREADS) == SOCKET_ERROR) {
		printf("Listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}


	while (1)
	{
		int index = -1;

		for (int i = 0; i < MAX_THREADS; i++)
			if (!isActive[i])
			{
				index = i;
				break;
			}



		if (index != -1)
		{
			tArgs[index].ClientSocket = INVALID_SOCKET;
			tArgs[index].ClientSocket = accept(ListenSocket, &(tArgs[index].clientAddr), &addrlen);

			if (tArgs[index].ClientSocket == INVALID_SOCKET)
				printf("accept failed: %d\n", WSAGetLastError());
			else
			{
				isActive[index] = TRUE;
				hThreads[index] = _beginthread(requestHandler, 0, index);
			}
		}

		else
		{

			printf("exeed MAXTHREAD %d waiting ....\n", MAX_THREADS);
			WaitForMultipleObjects(10, hThreads, FALSE, 1000);
		}
	}


	closesocket(ListenSocket);


	WSACleanup();
	return 0;

}


void requestHandler(int index)
{
	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN];
	int iResult;
	SOCKET ClientSocket = tArgs[index].ClientSocket;
	char remoteAddr[20];


	inet_ntop(AF_INET, &tArgs[index].clientAddr.sin_addr, remoteAddr, sizeof(remoteAddr));
	printf("accept Client %s threadNum %d\n", remoteAddr, index);


	do {

		iResult = recv(ClientSocket, recvbuf, sizeof(recvbuf), 0);
		HTTP_Request req;
		if (iResult > 0) {
			if (parseRequest(recvbuf, iResult, &req) != 0)
				printf("bad request or server error\n");
			else
			{

				iResult = sendResponse(&req, ClientSocket);
			}
			freeRequest(&req);
		}

		else if (iResult == 0)
			printf("Connection closing...\n");
		else {
			printf("recv failed %d\n", WSAGetLastError());
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
}
