#define BUFSIZE 4096
#include <winsock2.h>

char* webroot;
//URL �Ķ����, POST ������, ��� ������ ������ ����ü
typedef struct reqData {
    char key[100];
    char value[300];
    struct reqData* next;
}reqData;

typedef enum HTTP_Method
{
    get = 0,
    post
}HTTP_Method;

//Ŭ���̾�Ʈ�� ��û ���� ����

typedef struct HTTP_Request {
    //GET or POST
    HTTP_Method method;
    //url path max 300
    char path[300];
    
    //HTTP version ex) HTTP/1.1
    short ver1;
    short ver2;

    //GET parmeter
    reqData* getData;

    //POST data 
    //only Content-Type: application/x-www-form-urlencoded
    reqData* postData;
    //��Ű
    reqData* cookies;

    //��� �׸�
    reqData* entities;
}HTTP_Request;


int parseRequest(char* , int, HTTP_Request*);
void printRequest(HTTP_Request*);
void freeReqData(reqData*);
void freeRequest(HTTP_Request*);
int sendFile(char*, int, SOCKET);
void addDate(char*, int);
void addMIME(char*, int, char*);
void notFound(SOCKET);