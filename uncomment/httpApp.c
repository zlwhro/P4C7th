#include "httpApp.h"

int sendResponse(HTTP_Request* req, SOCKET clientSocket)
{
    printf("path%s\n", req->path);
    if (req->path == strstr(req->path, "/static/"))
        sendFile(req->path, clientSocket);

    if (strcmp(req->path, "/") == 0)
        sendFile("template/index.html", clientSocket);

    else if (strcmp(req->path, "/drink") == 0)
    {
        Dict renderArgs;
        renderArgs.count = 0;

        DictData* name, * select;
        name = findByKey(&req->getData, "name");
        select = findByKey(&req->getData, "select");

        addDict(&renderArgs, "name", name != NULL ? name->value : "");
        addDict(&renderArgs, "select", select != NULL ? select->value : "juice");

        render("drink.html", &renderArgs, clientSocket);
        clearDict(&renderArgs);
    }
    else
        notFound(clientSocket);

    return 0;
}