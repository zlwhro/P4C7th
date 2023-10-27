
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

typedef struct HTTP_Request {
    HTTP_Method method;
    char path[300];
    short ver1;
    short ver2;
    reqData* getData;
    reqData* postData;
    reqData* cookies;
    reqData* entities;
}HTTP_Request;


int parseRequest(char* , int, HTTP_Request*);
void printRequest(HTTP_Request*);
void freeRequest(HTTP_Request*);