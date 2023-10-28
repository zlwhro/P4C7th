#include "httpCore.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>



# define strip(line) if((line)[strlen((line)) -1] == '\r') (line)[strlen((line))-1] = '\0'
# define isdigit(chr) ('0' <= (chr) && (chr) <= '9')
# define ctoi(chr) (chr)-'0'
# define remain(buffer) sizeof((buffer)) - strlen((buffer))
# define strback(buffer) &((buffer)[strlen((buffer))])
# define lastChr(buffer) (buffer)[strlen((buffer))-1]

// 헤더 항목 읽기
reqData* parse_entitiy(char* line)
{
    char* key = NULL, * value = NULL, * next=NULL;

    // ':' 기준 왼쪽이 key 오른쪽이 value
    key = strtok_s(line, ":", &next);
    
    // \r carrige return 문자는 지운다.
    value = strtok_s(NULL, "\r", &next);
    if (key != NULL && value != NULL)
    {
        //공백은 무시한다.
        while (*value == ' ')
            value++;
        reqData* entity = malloc(sizeof(reqData));
        if (entity == NULL)
            return NULL;

        strcpy_s(entity->key, sizeof(entity->key), line);
        strcpy_s(entity->value, sizeof(entity->value), value);
        entity->next = NULL;
        return entity;
    }
}

//NULL은 decode하지 않는다.
int urldecode(char* dest, char* src, int len)
{
    int i = 0;
    int size = 0;
    char s[5] = "0x00";
    while (i < len)
    {
        if (src[i] == '%' && i < len - 2)
        {
            s[2] = src[i + 1];
            s[3] = src[i + 2];

            char c = (char)strtol(s, NULL, 16);
            if (c != 0)
            {
                dest[size++] = c;
                i += 3;
            }
            else
                dest[size++] = src[i++];
        }
        else
            dest[size++] = src[i++];
    }
    dest[size] = '\0';
}

// '='의 왼쪽은 key 오른 쪽은 value 
reqData* get_param(char* arg_str)
{
    reqData* new_param = NULL;
    char* value = strchr(arg_str, '=');
    if (value == NULL || arg_str == value)
        return NULL;
    *value = '\0';
    value += 1;
    new_param = malloc(sizeof(reqData));
    if (new_param == NULL)
        return NULL;
    strcpy_s(new_param->key, 100, arg_str);

    urldecode(new_param->value, value, strlen(value));
    new_param->next = NULL;
    return new_param;
}

//get 혹은 post 데이터 읽기
//post의 경우 application/x-www-form-urlencoded 만 지원한다.
reqData* parseData(char* http_args)
{
    char* argstr = NULL, * next = NULL;
    reqData* first = NULL, * prev = NULL, * cur;
    argstr = strtok_s(http_args, "&", &next);

    while (argstr != NULL)
    {
        cur = NULL;
        cur = get_param(argstr);
        if (cur != NULL)
        {
            if (first == NULL)
            {
                first = cur;
                prev = cur;
            }
            else
                prev->next = cur;
            prev = cur;
        }
        argstr = strtok_s(NULL, "&", &next);
    }
    return first;
}

//쿠키 읽기
reqData* parseCookie(char* http_args)
{
    char* cookieStr = NULL, * next = NULL;
    reqData* first = NULL, * prev = NULL, * cur;
    cookieStr = strtok_s(http_args, ";", &next);

    while (cookieStr != NULL)
    {
        while (cookieStr[0] == ' ')
            ++cookieStr;
        cur = NULL;
        cur = get_param(cookieStr);
        if (cur != NULL)
        {
            if (first == NULL)
            {
                first = cur;
                prev = cur;
            }
            else
                prev->next = cur;
            prev = cur;
        }
        cookieStr = strtok_s(NULL, ";", &next);
    }
    return first;
}

//method, path, parameter
int http_mpp(char* line, HTTP_Request* req)
{
    char* method = NULL;
    char* path = NULL;
    char* version = NULL;
    char* next = NULL;
    char* parameters = NULL;

    method = strtok_s(line, " ", &next);
    if (method == NULL)
        return -1;
    
    //method 확인
    if (strcmp(method, "GET") == 0)
        req->method = get;
    else if (strcmp(method, "POST") == 0)
        req->method = post;
    else
        return -1;

    //경로 확인
    path = strtok_s(NULL, " ", &next);
    if (path == NULL || path != NULL && path[0] != '/')
        return -1;

    //버전 확인
    version = strtok_s(NULL, "\r", &next);
    if (version == NULL)
        return -1;

    if(version != strstr(version,"HTTP/") && strlen(version) != 8)
        return -1;

    if (!isdigit(version[5]) || !isdigit(version[7]))
        return -1;

    // get parameter 읽기
    parameters = strchr(path, '?');
    if (parameters != NULL)
    {
        *parameters = '\0';
        parameters += 1;
        req->getData = parseData(parameters);
    }

    //하위 디렉터리로 진입 방지
    if (strstr(path, "..") != NULL)
        return -1;

    strcpy_s(req->path, sizeof(req->path), path);
    req->ver1 = ctoi(version[5]);
    req->ver2 = ctoi(version[7]);
    return 0;
}
//클라이언트 요청 읽기
int parseRequest(char* buffer, int dataLen, HTTP_Request *req)
{
    
    char* line = NULL;
    char* next_line = NULL;
    memset(req, 0, sizeof(HTTP_Request));
    //한줄씩 읽기
    line = strtok_s(buffer, "\n", &next_line);

    //method, path, urlparameter version 읽기
    int iResult = http_mpp(line, req);
    if (iResult == 0)
    {
        //한줄씩 읽기
        reqData* cur = NULL, * prev = NULL;
        line = strtok_s(NULL, "\n", &next_line);

        //헤더 정보 읽기
        while (line != NULL && strlen(line) > 1)
        {
            
            cur = parse_entitiy(line);
            if (cur != NULL)
            {
                //쿠키 읽기
                if (strcmp(cur->key, "Cookie") == 0)
                {
                    req->cookies = parseCookie(cur->value);
                    freeReqData(cur);
                }
                else if (req->entities == NULL)
                {
                    req->entities = cur;
                    prev = cur;
                }
                else
                {
                    prev->next = cur;
                    prev = cur;
                }
            }
            line = strtok_s(NULL, "\n", &next_line);
        }
        //post data 읽기
        if (req->method == post)
        {
            line = strtok_s(NULL, "\n", &next_line);
            req->postData = parseData(line);
        }
    }
    //잘못된 요청
    else
        return 1;

    return 0;
}


int sendFile(char* path, int status, SOCKET clientSocket)
{
    char buffer[BUFSIZE];
    char filepath[200];
    strcpy_s(filepath, sizeof(filepath), webroot);
    if (lastChr(filepath) != '\\')
    {
        int len = strlen(filepath);
        filepath[len] = '\\';
        filepath[len+1] = '\0';
    }

    strcat_s(filepath, sizeof(filepath), path);

    long fileSize = 0;
    errno_t err;
    FILE* file;
    err = fopen_s(&file, filepath, "rb");
    if(err != 0)
    {
        printf("The file %s was not opened\n", path);
        notFound(clientSocket);
        return -1;
    }



    fseek(file, 0, SEEK_END); // seek to end of file
    fileSize = ftell(file); // get current file pointer
    fseek(file, 0, SEEK_SET);

    strcpy_s(buffer, sizeof(buffer), "HTTP/1.1 200 OK\r\n");

    addDate(buffer, sizeof(buffer));
    addMIME(buffer, sizeof(buffer), path);
    sprintf_s(strback(buffer), remain(buffer), "Content-Length: %ld\r\n", fileSize);
    strcat_s(buffer, sizeof(buffer), "Connection: close\r\n\r\n");
    send(clientSocket, buffer, strlen(buffer), 0);

    int size = fread_s(buffer, sizeof(buffer), 1, 4096, file);
    while (size > 0)
    {
        send(clientSocket, buffer, size, 0);
        size = fread_s(buffer, sizeof(buffer), 1, 4096, file);
    }
    fclose(file);
    return 0;
}

void notFound(SOCKET clientSocket)
{
    char buffer[BUFSIZE];
    char* errorPage = "<!doctype html>\n"
        "<html>\n"
        "<title>404 Not Found</title>\n"
        "<h1>Not Found</h1>\n"
        "<p>The requested URL was not found on the server.If you entered the URL manually please check your spelling and try again. </p></html>\n";

    strcpy_s(buffer, sizeof(buffer), "HTTP/1.1 404 NOT FOUND\r\n");
    addDate(buffer, sizeof(buffer));
    addMIME(buffer, sizeof(buffer), NULL);
    sprintf_s(strback(buffer), remain(buffer), "Content-Length: %d\r\n", strlen(errorPage));
    strcat_s(buffer, sizeof(buffer), "Connection: close\r\n\r\n");
    strcat_s(buffer, sizeof(buffer), errorPage);
    send(clientSocket, buffer, strlen(buffer), 0);
}


void printRequest(HTTP_Request* req)
{
    printf("method: %s\n", req->method == get ? "get" : "post");
    printf("path: %s\n", req->path);
    printf("version: %d.%d\n", req->ver1, req->ver2);
    if(req->cookies != NULL)
    {
        printf("cookies\n");
        reqData* cur = req->cookies;
        while (cur != NULL)
        {
            printf("\t%s: %s\n", cur->key, cur->value);
            cur = cur->next;
        }
    }

    if (req->method == get && req->getData != NULL)
    {
        printf("params\n");
        reqData* cur = req->getData;
        while (cur != NULL)
        {
            printf("\t%s: %s\n", cur->key, cur->value);
            cur = cur->next;
        }
    }

    if (req->method == post && req->postData != NULL)
    {
        printf("postData\n");
        reqData* cur = req->postData;
        while (cur != NULL)
        {
            printf("\t%s: %s\n", cur->key, cur->value);
            cur = cur->next;
        }
    }

    reqData* entity = req->entities;
    printf("\nentities\n");
    while (entity != NULL)
    {
        printf("\t%s: %s\n", entity->key, entity->value);
        entity = entity->next;
    }
}



void freeReqData(reqData* data)
{
    reqData* cur = data;
    while (cur != NULL)
    {
        reqData* temp = cur;
        cur = cur->next;
        free(temp);
    }
}

void freeRequest(HTTP_Request* req)
{
    freeReqData(req->getData);
    freeReqData(req->postData);
    freeReqData(req->cookies);
    freeReqData(req->entities);
}

void addDate(char* dest, int buf_size)
{
    time_t t;
    time(&t);
    char buffer[26];
    char buffer2[50];
    char* next = NULL;
    struct tm gmt;
    gmtime_s(&gmt, &t);
    asctime_s(buffer, 26, &gmt);
    char* weekDay = strtok_s(buffer, " ", &next);
    char* month = strtok_s(NULL, " ", &next);
    char* day = strtok_s(NULL, " ", &next);
    char* time = strtok_s(NULL, " ", &next);
    char* year = strtok_s(NULL, " ", &next);
    year[4] = '\0';
    sprintf_s(buffer2, 50, "Date: %s, %s %s %s %s GMT\r\n", weekDay, day, month, year, time);
    strcat_s(dest, buf_size, buffer2);
}

void addMIME(char* buffer, int buf_size, char* path) {
    
    if(path == NULL)
        strcat_s(buffer, buf_size, "Content-Type: text/html; charset=utf-8\r\n");
    else
    {
        int pathLen = strlen(path);
        if (strcmp(&path[pathLen - 5], ".html") == 0)
            strcat_s(buffer, buf_size, "Content-Type: text/html; charset=utf-8\r\n");
        else if (strcmp(&path[pathLen - 4], ".css") == 0)
            strcat_s(buffer, buf_size, "Content-Type: text/css\r\n");
        else if (strcmp(&path[pathLen - 3], ".js") == 0)
            strcat_s(buffer, buf_size, "Content-Type: text/javascript\r\n");
        else if (strcmp(&path[pathLen - 4], ".jpg") == 0 || strcmp(&path[pathLen - 5], ".jpeg") == 0)
            strcat_s(buffer, buf_size, "Content-Type: image/jpeg\r\n");
        else if (strcmp(&path[pathLen - 4], ".png"))
            strcat_s(buffer, buf_size, "Content-Type: image/png\r\n");
        else if (strcmp(&path[pathLen - 4], ".gif"))
            strcat_s(buffer, buf_size, "Content-Type: image/gif\r\n");
        else if (strcmp(&path[pathLen - 5], ".webp"))
            strcat_s(buffer, buf_size, "Content-Type: image/webp\r\n");
        else
            strcat_s(buffer, buf_size, "Content-Type: application/octet-stream\r\n");
    }
}