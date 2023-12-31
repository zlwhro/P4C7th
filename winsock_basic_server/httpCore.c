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
void parse_entitiy(Dict* dict, char* line)
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
        addDict(dict, key, value);
    }
}

//NULL은 decode하지 않는다.
void urldecode(char* dest, char* src, int len)
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
void get_param(Dict *dict, char* arg_str)
{
    char* value = strchr(arg_str, '=');
    if (value == NULL || arg_str == value)
        return;
    *value = '\0';
    value += 1;
    addDict(dict, arg_str, value);

    urldecode(dict->last->value, value, strlen(value));
}

//get 혹은 post 데이터 읽기
//post의 경우 application/x-www-form-urlencoded 만 지원한다.
void parseData(Dict *dict, char* http_args)
{
    char* argstr = NULL, * next = NULL;
    argstr = strtok_s(http_args, "&", &next);

    while (argstr != NULL)
    {
        get_param(dict, argstr);
        argstr = strtok_s(NULL, "&", &next);
    }
}

//쿠키 읽기
void parseCookie(Dict* dict, char* http_args)
{
    char* cookieStr = NULL, * next = NULL;
    cookieStr = strtok_s(http_args, ";", &next);

    while (cookieStr != NULL)
    {
        while (cookieStr[0] == ' ')
            ++cookieStr;
        get_param(dict, cookieStr);
        cookieStr = strtok_s(NULL, ";", &next);
    }
}

//method, path, parameter, version
int http_mppv(char* line, HTTP_Request* req)
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
        parseData(&req->getData, parameters);
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
    int iResult = http_mppv(line, req);
    if (iResult == 0)
    {
        //한줄씩 읽기
        line = strtok_s(NULL, "\n", &next_line);
        //헤더 정보 읽기
        while (line != NULL && strlen(line) > 1)
        {
            parse_entitiy(&req->entities, line);
            DictData* cookie = findByKey(&req->entities, "Cookie");
            if (cookie != NULL)
                parseCookie(&req->cookies, cookie->value);
            
            line = strtok_s(NULL, "\n", &next_line);
        }
        //post data 읽기
        if (req->method == post)
        {
            //Content-length 확인
            int content_len = 0;
            DictData* clen = findByKey(&req->entities, "Content-Length");
            if (clen == NULL)
            {
                printf("read post data fail\n");
                return 1;
            }
            content_len = strtol(clen->value, NULL, 10);
            line = strtok_s(NULL, "\n", &next_line);

            //NULL 바이트 쓰기 지정한 크기보다 많이 읽는걸 방지
            line[content_len] = '\0';
            printf("line: %s\n",line);
            parseData(&(req->postData), line);
        }
    }
    //잘못된 요청
    else
        return 1;

    return 0;
}

//저장된 파일 그대로 보내기
int sendFile(char* path, SOCKET clientSocket)
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

    //슬래시는 백슬래시로 변경
    for (int i = 0; i < strlen(filepath); ++i)
        if (filepath[i] == '/')
            filepath[i] = '\\';

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


    //파일 크기 확인
    //int fseek(File* stream, long offset, int origin)
    //파일 끝으로 이동
    fseek(file, 0, SEEK_END);
    //파일 시작부터 현재 위치까지 바이트 수
    fileSize = ftell(file);
    //다시 처음으로 이동
    fseek(file, 0, SEEK_SET);

    //헤더 쓰기
    strcpy_s(buffer, sizeof(buffer), "HTTP/1.1 200 OK\r\n");

    addDate(buffer, sizeof(buffer));
    addMIME(buffer, sizeof(buffer), path);
    sprintf_s(strback(buffer), remain(buffer), "Content-Length: %ld\r\n", fileSize);
    strcat_s(buffer, sizeof(buffer), "Connection: close\r\n\r\n");
    send(clientSocket, buffer, strlen(buffer), 0);

    //4096바이트 씩 나누어 보내기
    int size = fread_s(buffer, sizeof(buffer), 1, 4096, file);
    while (size > 0)
    {
        send(clientSocket, buffer, size, 0);
        size = fread_s(buffer, sizeof(buffer), 1, 4096, file);
    }
    fclose(file);
    return 0;
}

//템플릿 랜더링
int render(char* filename, Dict *renderArgs,SOCKET clientSocket)
{
    char buffer[BUFSIZE];
    char* tempBuff = NULL;
    RenderData renderData[100] = { 0 };
    char filepath[200];
    int bufferSize = 0, fileSize = 0, renderSize = 0;

    //랜더링에 사용할 인자가 없다면 파일을 그대로 보낸다.
    if (renderArgs == NULL)
    {
        strcpy_s(filepath, sizeof(filepath), "template\\");
        strcat_s(filepath, sizeof(filepath), filename);
        return sendFile(filepath, clientSocket);
    }

    //템플릿 파일 읽기
    strcpy_s(filepath, sizeof(filepath), webroot);
    if (lastChr(filepath) != '\\')
    {
        int len = strlen(filepath);
        filepath[len] = '\\';
        filepath[len + 1] = '\0';
    }
    strcat_s(filepath, sizeof(filepath), "template\\");
    strcat_s(filepath, sizeof(filepath), filename);
    
    errno_t err;
    FILE* file;
    err = fopen_s(&file, filepath, "rb");
    if (err != 0)
    {
        printf("The file %s was not opened\n", filename);
        InternalError(clientSocket);
        return -1;
    }


    //템플릿 크기 확인
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    bufferSize = fileSize + 100;
    fseek(file, 0, SEEK_SET);
    
    //템플릿 읽기
    tempBuff = malloc(bufferSize);
    if (tempBuff == NULL)
    {
        printf("malloc error\n");
        InternalError(clientSocket);
        return -1;
    }

    fileSize = fread_s(tempBuff, bufferSize, 1, bufferSize, file);
    if (fclose(file))
        printf("template file %s not closed\n", filename);

    //템플릿 랜더링
    int i = 0, j, idx = 0;
    renderData[0].p = tempBuff;
    renderData[0].size = 0;
    //${{key}} 를 찾고 value로 바꾸어 준다.
    while (i < fileSize)
    {
        if (memcmp(&tempBuff[i], "${{", 3)==0)
        {
            i += 3;
            for (j = i; j < fileSize; ++j)
                if (memcmp(&tempBuff[j], "}}", 2) == 0)
                    break;
            if (j < fileSize && j - i < 100)
            {
                tempBuff[j] = '\0';
                DictData* arg = findByKey(renderArgs, &tempBuff[i]);
                if (arg == NULL)
                {
                    printf("render error key %s is not found\n", &tempBuff[i]);
                    free(tempBuff);
                    InternalError(clientSocket);
                    return -1;
                }
                else {
                    renderData[idx + 1].p = arg->value;
                    renderData[idx + 1].size = strlen(arg->value);
                    idx += 2;
                    renderData[idx].p = &tempBuff[j + 2];
                    renderData[idx].size = 0;
                    i = j+2;
                }
            }
        }
        else
        {
            renderData[idx].size++;
            i++;
        }
    }
    idx += 1;
    for (i = 0; i < idx; ++i)
        renderSize += renderData[i].size;

    //헤더 쓰기
    strcpy_s(buffer, sizeof(buffer), "HTTP/1.1 200 OK\r\n");

    addDate(buffer, sizeof(buffer));
    addMIME(buffer, sizeof(buffer), NULL);
    sprintf_s(strback(buffer), remain(buffer), "Content-Length: %ld\r\n", renderSize);
    strcat_s(buffer, sizeof(buffer), "Connection: close\r\n\r\n");
    send(clientSocket, buffer, strlen(buffer), 0);

    //랜더링이 끝난 페이지 전송
    for (i = 0; i < idx; ++i)
        send(clientSocket, renderData[i].p, renderData[i].size, 0);

    free(tempBuff);
    return 0;
}

//없는 경로로 요청
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
    sprintf_s(strback(buffer), remain(buffer), "Content-Length: %d\r\n", (int)strlen(errorPage));
    strcat_s(buffer, sizeof(buffer), "Connection: close\r\n\r\n");
    strcat_s(buffer, sizeof(buffer), errorPage);
    send(clientSocket, buffer, strlen(buffer), 0);
}

//서버 에러 
//템플릿 랜더링 실패시 보낸다
void InternalError(SOCKET clientSocket)
{
    char buffer[BUFSIZE];
    char* errorPage = "<!doctype html>\n"
        "<html>\n"
        "<title>500 Internal Server Error</title>\n"
        "<h1>Internal Server Error</h1>\n"
        "<p>Internal Server Error</p></html>\n";

    strcpy_s(buffer, sizeof(buffer), "HTTP/1.1 500 Internal Server Error\r\n");
    addDate(buffer, sizeof(buffer));
    addMIME(buffer, sizeof(buffer), NULL);
    sprintf_s(strback(buffer), remain(buffer), "Content-Length: %d\r\n", (int)strlen(errorPage));
    strcat_s(buffer, sizeof(buffer), "Connection: close\r\n\r\n");
    strcat_s(buffer, sizeof(buffer), errorPage);
    send(clientSocket, buffer, strlen(buffer), 0);
}

//요청을 제대로 읽었는지 테스트 용
//메소드, URL 파라미터, 쿠키, POST data 등을 읽는다.
void printRequest(HTTP_Request* req)
{
    printf("method: %s\n", req->method == get ? "get" : "post");
    printf("path: %s\n", req->path);
    printf("version: %d.%d\n", req->ver1, req->ver2);
    if(req->cookies.count >0)
    {
        printf("cookies\n");
        DictData* cur = req->cookies.first;
        while (cur != NULL)
        {
            printf("\t%s: %s\n", cur->key, cur->value);
            cur = cur->next;
        }
    }

    if (req->method == get && req->getData.count > 0)
    {
        printf("params\n");
        DictData *cur = req->getData.first;
        while (cur != NULL)
        {
            printf("\t%s: %s\n", cur->key, cur->value);
            cur = cur->next;
        }
    }

    if (req->method == post && req->postData.count > 0)
    {
        printf("postData\n");
        DictData* cur = req->postData.first;
        while (cur != NULL)
        {
            printf("\t%s: %s\n", cur->key, cur->value);
            cur = cur->next;
        }
    }

    DictData* entity = req->entities.first;
    printf("\nentities\n");
    while (entity != NULL)
    {
        printf("\t%s: %s\n", entity->key, entity->value);
        entity = entity->next;
    }
}

//딕셔너리에서 데이터 찾기
DictData* findByKey(Dict* dict, char* key)
{
    DictData* cur = dict->first;
    //키가 같으면 리턴
    while (cur != NULL)
        if (strcmp(cur->key, key) == 0)
            return cur;
        else
            cur = cur->next;

    //같은 키가 없으면 NULL
    return NULL;
}

//딕셔너리에 데이터 추가
void addDict(Dict* dict, char* key, char* value)
{
    DictData* newData = malloc(sizeof(DictData));
    strcpy_s(newData->key, sizeof(newData->key), key);
    strcpy_s(newData->value, sizeof(newData->value), value);

    //데이터가 하나도 없는 경우
    if (dict->count == 0)
        dict->first = newData;
    //last에 연결
    else
        dict->last->next = newData;

    dict->last = newData;
    dict->last->next = NULL;
    dict->count += 1;
}

//딕셔너리 데이터 비우기
//할당한 메모리 해제
void clearDict(Dict* dict)
{
    DictData* cur = dict->first;
    while (cur != NULL)
    {
        DictData* temp = cur;
        cur = cur->next;
        free(temp);
    }
}

//요청 읽기에 사용한 메모리 해제
void freeRequest(HTTP_Request* req)
{
    clearDict(&req->getData);
    clearDict(&req->postData);
    clearDict(&req->cookies);
    clearDict(&req->entities);
}

//응답 헤더에 날짜 추가
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

//응답헤더에 MiME 타입 추가
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