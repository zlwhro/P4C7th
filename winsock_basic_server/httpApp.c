#include "httpApp.h"

int sendResponse(HTTP_Request* req, SOCKET clientSocket)
{
    printf("path%s\n", req->path);
    if (req->path == strstr(req->path, "/static/"))
        sendFile(req->path, clientSocket);

    if (strcmp(req->path, "/") == 0)
        sendFile("template/index.html", clientSocket);
        //render("index.html", NULL, clientSocket);
    //템플릿 랜더링 테스트
    //url 파라미터에서 이름과 메뉴를 읽고 응답 페이지 생성
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