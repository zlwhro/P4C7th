#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <process.h>
#include "httpServer.h"


#pragma comment(lib, "Ws2_32.lib") //라이브러리 파일 Ws2_32.lib에 연결되는지 확인한다.
#define DEFAULT_PORT "4444"
#define DEFAULT_BUFLEN 4096
#define MAX_THREADS 10



// 스레드가 사용할 구조체
// 소켓, 클라이언트 주소, 데이터 송수신에 사용할 버퍼
typedef struct ThreadArgs {
	SOCKET ClientSocket;
	SOCKADDR_IN clientAddr;
	char recvbuf[DEFAULT_BUFLEN];
}ThreadArgs;


HANDLE  hThreads[MAX_THREADS] = { NULL };

//스레드의 동작여부
BOOL isActive[MAX_THREADS] = { FALSE };

//스레드가 사용할 데이터
ThreadArgs tArgs[MAX_THREADS];


//클라언트의 요청 처리
void requestHandler(int index);

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

	////서버에서 사용할 SOCKET 개체를 인스턴스화
	struct addrinfo* result = NULL, * ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET; //IPv4 
	hints.ai_socktype = SOCK_STREAM; //stream socket
	hints.ai_protocol = IPPROTO_TCP; //TCP 프로토콜 사용
	hints.ai_flags = AI_PASSIVE; //Bind 함수 인자로 사용할 예정 

	// 서버가 사용할 주소와 포트 지정 주소에 NULL을 넣고 hints.ai_flags=AI_PASSIVE 이면 INADDR_ANY가 result에 들어간다. 이러면 서버에서 사용가능한 모든 주소를 사용할 수 있다.
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	//클라이언트의 연결 기다리기
	SOCKET ListenSocket = INVALID_SOCKET;
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	if (ListenSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	//지정한 주소와 포트에 바인딩하기 
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	//getaddrinfo에서 할당한 메모리 해제
	freeaddrinfo(result);

	//소켓을 대기 상태로 만든다.
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
		

		//생성된 스레드가 MAX_THREADS 이하 새로운 스레드 생성 가능
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
		// 사용가능한 스레드 개수 초과
		else
		{
			//스레드가 하나라도 종료될 때까지 기다린다.
			printf("exeed MAXTHREAD %d waiting ....\n", MAX_THREADS);
			WaitForMultipleObjects(10, hThreads, FALSE, 1000);
		}
	}

	//ListenSocket 종료
	closesocket(ListenSocket);

	//Windows 소켓 리소스 해제
	WSACleanup();
	return 0;

}

//클라언트의 요청 처리
void requestHandler(int index)
{
	int iSendResult;

	int iResult;
	SOCKET ClientSocket = tArgs[index].ClientSocket;
	char remoteAddr[20];
	char* recvbuf = tArgs[index].recvbuf;
	int recvbuflen = DEFAULT_BUFLEN;

	//클라이언트 IP주소 확인
	inet_ntop(AF_INET, &tArgs[index].clientAddr.sin_addr, remoteAddr, sizeof(remoteAddr));
	printf("accept Client %s threadNum %d\n", remoteAddr, index);


	do {
		// 데이터 수신
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		HTTP_Request req;
		if (iResult > 0) {
			if (parseRequest(recvbuf, iResult, &req) != 0)
				printf("bad request or server error\n");
			else
				printRequest(&req);
			freeRequest(&req);
		}
		//클라이언트 쪽에서 연결 종료
		else if (iResult == 0)
			printf("Connection closing...\n");
		else {
			printf("recv failed %d\n", WSAGetLastError());
		}

	} while (iResult > 0);

	//shutdown socket 소켓은 데이터 수신만 가능하고 송신을 불가능한 상태가 된다.
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return 1;
	}

	//소켓 닫기
	closesocket(ClientSocket);

	//스레드 종료하기
	isActive[index] = FALSE;
	_endthread();
}
