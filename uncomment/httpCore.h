#pragma once

#define BUFSIZE 4096
#include <winsock2.h>


char* webroot;

typedef struct DictData {
    char key[100];
    char value[300];
    struct DictData* next;
}DictData;

typedef struct Dict {
    DictData* first;
    DictData* last;
    int count;
}Dict;


typedef enum HTTP_Method
{
    get = 0,
    post
}HTTP_Method;


typedef struct HTTP_Request {
    HTTP_Method method;
    char path[300];
    short ver1;
    short ver2;

    Dict getData;
    Dict postData;
    Dict cookies;
    Dict entities;
}HTTP_Request;


typedef struct RenderData {
    char* p;
    int size;
}RenderData;


int parseRequest(char*, int, HTTP_Request*);
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


