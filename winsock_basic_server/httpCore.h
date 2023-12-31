#pragma once

#define BUFSIZE 4096
#include <winsock2.h>


char* webroot;

//딕셔너리 항목
typedef struct DictData {
    char key[100];
    char value[300];
    struct DictData* next;
}DictData;
//딕셔너리 구조체
typedef struct Dict {
    DictData *first;
    DictData *last;
    int count;
}Dict;


typedef enum HTTP_Method
{
    get = 0,
    post
}HTTP_Method;

//클라이언트의 요청 정보 저장

typedef struct HTTP_Request {
    //GET or POST
    HTTP_Method method;
    //url path max 300
    char path[300];
    
    //HTTP version ex) HTTP/1.1
    short ver1;
    short ver2;

    //GET parmeter
    Dict getData;

    //POST data 
    //only Content-Type: application/x-www-form-urlencoded
    Dict postData;
    //쿠키
    Dict cookies;

    //헤더 항목
    Dict entities;
}HTTP_Request;

//랜더링 결과 저장
typedef struct RenderData {
    char* p;
    int size;
}RenderData;


int parseRequest(char* , int, HTTP_Request*);
void printRequest(HTTP_Request*);

void clearDict(Dict*);
void addDict(Dict*, char*, char*);
DictData* findByKey(Dict*, char*);
void freeRequest(HTTP_Request*);

int sendFile(char*, SOCKET);
void addDate(char*, int);
void addMIME(char*, int, char*);
void notFound(SOCKET);
void InternalError(SOCKET);
int render(char*, Dict*, SOCKET);

