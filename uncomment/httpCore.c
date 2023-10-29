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


void parse_entitiy(Dict* dict, char* line)
{
    char* key = NULL, * value = NULL, * next = NULL;
    key = strtok_s(line, ":", &next);
    value = strtok_s(NULL, "\r", &next);
    if (key != NULL && value != NULL)
    {
        while (*value == ' ')
            value++;
        addDict(dict, key, value);
    }
}


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


void get_param(Dict* dict, char* arg_str)
{
    char* value = strchr(arg_str, '=');
    if (value == NULL || arg_str == value)
        return;
    *value = '\0';
    value += 1;
    addDict(dict, arg_str, value);

    urldecode(dict->last->value, value, strlen(value));
}


void parseData(Dict* dict, char* http_args)
{
    char* argstr = NULL, * next = NULL;
    argstr = strtok_s(http_args, "&", &next);

    while (argstr != NULL)
    {
        get_param(dict, argstr);
        argstr = strtok_s(NULL, "&", &next);
    }
}


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


    if (strcmp(method, "GET") == 0)
        req->method = get;
    else if (strcmp(method, "POST") == 0)
        req->method = post;
    else
        return -1;


    path = strtok_s(NULL, " ", &next);
    if (path == NULL || path != NULL && path[0] != '/')
        return -1;


    version = strtok_s(NULL, "\r", &next);
    if (version == NULL)
        return -1;

    if (version != strstr(version, "HTTP/") && strlen(version) != 8)
        return -1;

    if (!isdigit(version[5]) || !isdigit(version[7]))
        return -1;


    parameters = strchr(path, '?');
    if (parameters != NULL)
    {
        *parameters = '\0';
        parameters += 1;
        parseData(&req->getData, parameters);
    }


    if (strstr(path, "..") != NULL)
        return -1;

    strcpy_s(req->path, sizeof(req->path), path);
    req->ver1 = ctoi(version[5]);
    req->ver2 = ctoi(version[7]);
    return 0;
}

int parseRequest(char* buffer, int dataLen, HTTP_Request* req)
{

    char* line = NULL;
    char* next_line = NULL;
    memset(req, 0, sizeof(HTTP_Request));

    line = strtok_s(buffer, "\n", &next_line);


    int iResult = http_mppv(line, req);
    if (iResult == 0)
    {
        line = strtok_s(NULL, "\n", &next_line);
        while (line != NULL && strlen(line) > 1)
        {
            parse_entitiy(&req->entities, line);
            DictData* cookie = findByKey(&req->entities, "Cookie");
            if (cookie != NULL)
                parseCookie(&req->cookies, cookie->value);

            line = strtok_s(NULL, "\n", &next_line);
        }

        if (req->method == post)
        {
            int content_len = 0;
            DictData* clen = findByKey(&req->entities, "Content-Length");
            if (clen == NULL)
            {
                printf("read post data fail\n");
                return 1;
            }
            content_len = strtol(clen->value, NULL, 10);
            line = strtok_s(NULL, "\n", &next_line);

            line[content_len] = '\0';
            printf("line: %s\n", line);
            parseData(&(req->postData), line);
        }
    }
    else
        return 1;

    return 0;
}

int sendFile(char* path, SOCKET clientSocket)
{
    char buffer[BUFSIZE];
    char filepath[200];
    strcpy_s(filepath, sizeof(filepath), webroot);
    if (lastChr(filepath) != '\\')
    {
        int len = strlen(filepath);
        filepath[len] = '\\';
        filepath[len + 1] = '\0';
    }

    strcat_s(filepath, sizeof(filepath), path);

    for (int i = 0; i < strlen(filepath); ++i)
        if (filepath[i] == '/')
            filepath[i] = '\\';

    long fileSize = 0;
    errno_t err;
    FILE* file;
    err = fopen_s(&file, filepath, "rb");
    if (err != 0)
    {
        printf("The file %s was not opened\n", path);
        notFound(clientSocket);
        return -1;
    }



    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
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

int render(char* filename, Dict* renderArgs, SOCKET clientSocket)
{
    char buffer[BUFSIZE];
    char* tempBuff = NULL;
    RenderData renderData[100] = { 0 };
    char filepath[200];
    int bufferSize = 0, fileSize = 0, renderSize = 0;

    if (renderArgs == NULL)
    {
        strcpy_s(filepath, sizeof(filepath), "template\\");
        strcat_s(filepath, sizeof(filepath), filename);
        return sendFile(filepath, clientSocket);
    }

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

    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    bufferSize = fileSize + 100;
    fseek(file, 0, SEEK_SET);


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

    int i = 0, j, idx = 0;
    renderData[0].p = tempBuff;
    renderData[0].size = 0;
    while (i < fileSize)
    {
        if (memcmp(&tempBuff[i], "${{", 3) == 0)
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
                    i = j + 2;
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

    strcpy_s(buffer, sizeof(buffer), "HTTP/1.1 200 OK\r\n");

    addDate(buffer, sizeof(buffer));
    addMIME(buffer, sizeof(buffer), NULL);
    sprintf_s(strback(buffer), remain(buffer), "Content-Length: %ld\r\n", renderSize);
    strcat_s(buffer, sizeof(buffer), "Connection: close\r\n\r\n");
    send(clientSocket, buffer, strlen(buffer), 0);

    for (i = 0; i < idx; ++i)
        send(clientSocket, renderData[i].p, renderData[i].size, 0);

    free(tempBuff);
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
    sprintf_s(strback(buffer), remain(buffer), "Content-Length: %d\r\n", (int)strlen(errorPage));
    strcat_s(buffer, sizeof(buffer), "Connection: close\r\n\r\n");
    strcat_s(buffer, sizeof(buffer), errorPage);
    send(clientSocket, buffer, strlen(buffer), 0);
}

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

void printRequest(HTTP_Request* req)
{
    printf("method: %s\n", req->method == get ? "get" : "post");
    printf("path: %s\n", req->path);
    printf("version: %d.%d\n", req->ver1, req->ver2);
    if (req->cookies.count > 0)
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
        DictData* cur = req->getData.first;
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

DictData* findByKey(Dict* dict, char* key)
{
    DictData* cur = dict->first;
    while (cur != NULL)
        if (strcmp(cur->key, key) == 0)
            return cur;
        else
            cur = cur->next;

    return NULL;
}

void addDict(Dict* dict, char* key, char* value)
{
    DictData* newData = malloc(sizeof(DictData));
    strcpy_s(newData->key, sizeof(newData->key), key);
    strcpy_s(newData->value, sizeof(newData->value), value);

    if (dict->count == 0)
        dict->first = newData;
    //last¿¡ ¿¬°á
    else
        dict->last->next = newData;

    dict->last = newData;
    dict->last->next = NULL;
    dict->count += 1;
}

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

void freeRequest(HTTP_Request* req)
{
    clearDict(&req->getData);
    clearDict(&req->postData);
    clearDict(&req->cookies);
    clearDict(&req->entities);
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

    if (path == NULL)
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