#include "httpServer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

# define strip(line) if((line)[strlen((line)) -1] == '\r') (line)[strlen((line))-1] = '\0'
# define isdigit(chr) ('0' <= (chr) && (chr) <= '9')
# define ctoi(chr) (chr)-'0'

reqData* parse_entitiy(char* line)
{
    char* key = NULL, * value = NULL, * next=NULL;
    key = strtok_s(line, ":", &next);
    value = strtok_s(NULL, "\r", &next);
    if (key != NULL && value != NULL)
    {
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

reqData* parseCookie(char* http_args)
{
    return NULL;
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

    if(version != strstr(version,"HTTP/") && strlen(version) != 8)
        return -1;

    if (!isdigit(version[5]) || !isdigit(version[7]))
        return -1;


    parameters = strchr(path, '?');
    if (parameters != NULL)
    {
        *parameters = '\0';
        parameters += 1;
        req->getData = parseData(parameters);
    }

    if (strstr(path, "/../") != NULL)
        return -1;

    strcpy_s(req->path, sizeof(req->path), path);
    req->ver1 = ctoi(version[5]);
    req->ver2 = ctoi(version[7]);
    return 0;
}

int parseRequest(char* buffer, int dataLen, HTTP_Request *req)
{
    char* line = NULL;
    char* next_line = NULL;
    memset(req, 0, sizeof(HTTP_Request));

    line = strtok_s(buffer, "\n", &next_line);

    int iResult = http_mpp(line, req);
    if (iResult == 0)
    {
        reqData* cur = NULL, * prev = NULL;
        line = strtok_s(NULL, "\n", &next_line);
        while (line != NULL && strlen(line) > 1)
        {
            cur = parse_entitiy(line);
            if (cur != NULL)
            {
                if (strcmp(cur->key, "Cookie") == 0)
                    req->cookies = parseCookie(cur->value);
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
        if (req->method == post)
        {
            line = strtok_s(NULL, "\n", &next_line);
            req->postData = parseData(line);
        }
    }
    else
        return 1;

    return 0;
}
void printRequest(HTTP_Request* req)
{
    printf("method: %s\n", req->method == get ? "get" : "post");
    printf("path: %s\n", req->path);
    printf("version: %d.%d\n", req->ver1, req->ver2);
    if (req->method == get && req->getData != NULL)
    {
        printf("params\n");
        reqData* cur = req->getData;
        while (cur != NULL)
        {
            printf("\tkey: %s\n", cur->key);
            printf("\tvalue: %s\n", cur->value);
            cur = cur->next;
        }
    }

    if (req->method == post && req->postData != NULL)
    {
        printf("postData\n");
        reqData* cur = req->postData;
        while (cur != NULL)
        {
            printf("\tkey: %s\n", cur->key);
            printf("\tvalue: %s\n", cur->value);
            cur = cur->next;
        }
    }

    reqData* entity = req->entities;
    printf("\nentities\n");
    while (entity != NULL)
    {
        printf("\tkey: %s\n", entity->key);
        printf("\tvalue: %s\n", entity->value);
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