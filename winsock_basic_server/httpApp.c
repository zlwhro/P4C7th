#include "httpApp.h"

int sendResponse(HTTP_Request* req, SOCKET clientSocket)
{
    printf("path%s\n", req->path);
    if(req->path == strstr(req->path,"/static/"))
        sendFile(req->path, 200, clientSocket);

    if (strcmp(req->path, "/") == 0)
    {
        sendFile("index.html", 200, clientSocket);
    }
    else
        notFound(clientSocket);
}