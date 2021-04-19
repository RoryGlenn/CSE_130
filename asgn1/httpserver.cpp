//---------------------------------------------------------------------------------
// Authors:   Rory Glenn
// Email:     romglenn@ucsc.edu
// Quarter:   Fall 2020
// Class:     CSE 130
// Project:   asgn1
// Professor: Faisal Nawab
//
// httpserver.cpp
// Implementaion file for httpserver
//---------------------------------------------------------------------------------

#include "httpserver.h"

int main(int argc, char** argv) 
{
    int                 enable          = 1;
    int                 server_sockd;
    int                 ret;
    char*               address         = argv[1];
    const char*         port            = (const char*)"80";
    socklen_t           client_addrlen;
    struct sockaddr_in  server_addr;
    struct sockaddr     client_addr;
    httpObject          msg;

    if(argc > 2)
    {
        port = argv[2];
    }
    else if(argc == 1 || argc > 3)
    {
        char socketError[] = "Usage: httpserver [address] [port]\n";
        write(STDERR_FILENO, socketError, strlen(socketError));
        exit(EXIT_FAILURE);
    }

    memset( &server_addr, 0, sizeof(server_addr) );

    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(atoi(port));
    server_addr.sin_addr.s_addr = GetAddress(address); //server_addr.sin_addr.s_addr = INADDR_ANY;

    if( (server_sockd = socket(AF_INET, SOCK_STREAM, 0) ) <= 0 )
    {
        char socketError[] = "Error: socket()\n";
        write(STDERR_FILENO, socketError, strlen(socketError));
        exit(EXIT_FAILURE);
    }

    // This allows you to avoid: 'Bind: Address Already in Use' error
    if( (ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) < 0 )
    {
        char setsockoptError[] = "Error: setsockopt()\n";
        write(STDERR_FILENO, setsockoptError, strlen(setsockoptError));
        exit(EXIT_FAILURE);
    }

    // Bind/attach server address to socket that is open
    if( (ret = bind(server_sockd, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0 )
    {
        char bindError[] = "Error: bind()\n";
        write(STDERR_FILENO, bindError, strlen(bindError));
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if( (ret = listen(server_sockd, 500)) < 0 )
    {
        char listenError[] = "Error: listen()\n";
        write(STDERR_FILENO, listenError, strlen(listenError));
        exit(EXIT_FAILURE);
    }
    
    // Connecting with a client
    while(true)
    {
        httpObject message;
        
        AcceptConnection( &message, (struct sockaddr*)&client_addr, (socklen_t*)&client_addrlen, server_sockd );
        
        ReadHTTPRequest(&message);

        ProcessHTTPRequest(&message);

        ConstructHTTPResponse(&message);

        SendHTTPResponse(&message);

        memset( &server_addr, 0, sizeof(server_addr) );
        close(message.client_sockd);
    }
    return 0;
}


//   GetAddress returns the numerical representation of the address
//   identified by *name* as required for an IPv4 address represented
//   in a struct sockaddr_in.
unsigned long GetAddress(char *name)
{
    // Author of GetAddress(): Daniel Santos Ferreira Alves
    // piazza link https://piazza.com/class/kfqgk8ox2mi4a1?cid=142

    unsigned long     res;
    struct addrinfo   hints;    // addrinfo: structure to contain information about address of a service provider.
    struct addrinfo*  info;

    memset( &hints, 0, sizeof(hints) );

    hints.ai_family = AF_INET;

    if ( getaddrinfo(name, NULL, &hints, &info) != 0 || info == NULL ) 
    {
        char msg[] = "getaddrinfo(): address identification error\n";
        write(STDERR_FILENO, msg, strlen(msg));
        exit(EXIT_FAILURE);
    }

    res = ((struct sockaddr_in*) info->ai_addr)->sin_addr.s_addr;

    freeaddrinfo(info);

    return res;
}


void H_CheckReadWriteAccess(httpObject* pMessage)
{
    // if the file exists
    if(H_CheckFileExists(pMessage))
    {
        // GET = 2
        if(H_CheckMethod(pMessage) == 2)
        {
            // Check read access
            access(pMessage->fileName, R_OK);

            if (errno == EACCES)
            {
                // Read Permission denied
                pMessage->statusCode = 403;
                pMessage->contentLength = 0;
                sscanf("Forbidden", "%s", pMessage->statusText);
                H_SendResponse(pMessage);
                return;
            }
        }
    }
}

int H_CheckMethod(httpObject* pMessage)
{
    if( strcmp(pMessage->method, "PUT") == 0 ) 
    {
        return 1;
    }
    else if( strcmp(pMessage->method, "GET") == 0 ) 
    {
        return 2;
    }
    else
    {
        return 0;
    }

    return 0;
}

void H_CallMethod(httpObject* pMessage)
{
    if( strcmp(pMessage->method, "PUT") == 0 )
    {
        HandlePUT(pMessage);
    }
    else if( strcmp(pMessage->method, "GET") == 0 )
    {
        HandleGET(pMessage);
    }
    else    // If the method does not match PUT or GET, then the request is a "Bad Request"
    {
        pMessage->statusCode = 400;
        pMessage->contentLength = 0;
        sscanf("Bad Request", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
        return;
    }
}

int H_CheckFileName(char* name)
{
    for(unsigned int i = 0; i < strlen(name); i++)
    {        
        // check if characters are NOT in alphabet, digits 0-9, dash, or underscore
        if( !((name[i] >= 'A' && name[i] <= 'Z') || 
            (name[i] >= 'a' && name[i] <= 'z') || 
            (name[i] >= '0' && name[i] <= '9') || 
            (name[i] == '_') || (name[i] == '-')) )
        {
            // invalid character
            return 400;
        }
    }

    return 0;
}

void H_CheckHTTPVersion(httpObject* pMessage)
{
    if( strcmp(pMessage->httpVersion, "HTTP/1.1") != 0)
    {
        pMessage->statusCode = 400; 
        pMessage->contentLength = 0;
        sscanf("Bad Request", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
        return;
    }
}

void H_StoreContentLength(httpObject* pMessage)
{
    // store content length from client request
    char* token = strtok(pMessage->buffer, "\r\n");
    while(token != NULL)
    {   
        char* pRet = strstr(token, "Content-Length: ");

        if(pRet != NULL)
        {
            sscanf( pRet, "%*s %ld", &(pMessage->contentLength) );
        }

        token = strtok(NULL, "\r\n");        
    }

}

void H_SetStatusOnError(httpObject* pMessage)
{
    switch(errno)
    {
        case 1:  
                // Operation not permitted
                pMessage->statusCode = 403;
                pMessage->contentLength = 0;
                sscanf("Forbidden", "%s", pMessage->statusText);
                H_SendResponse(pMessage);
                break; 
        case 2: 
                // No such file or directory 
                pMessage->statusCode = 404;
                pMessage->contentLength = 0;
                sscanf("Not Found", "%[^\t\n]", pMessage->statusText);
                H_SendResponse(pMessage);
                break; 
        case 9:  
                // Bad file descriptor
                pMessage->statusCode = 400;
                pMessage->contentLength = 0;
                sscanf("Bad Request", "%[^\t\n]", pMessage->statusText);
                H_SendResponse(pMessage);
                break; 
        case 13: 
                // Permission denied
                pMessage->statusCode = 403;
                pMessage->contentLength = 0;
                sscanf("Forbidden", "%s", pMessage->statusText);
                H_SendResponse(pMessage);
                break;
        default: 
                // Interval server error 
                pMessage->statusCode = 500;
                pMessage->contentLength = 0;
                sscanf("Internal Server Error", "%[^\t\n]", pMessage->statusText);
                H_SendResponse(pMessage);
                break;
    }
}

void H_ReadContentLength(httpObject* pMessage)
{
    struct stat statbuf;

    if( stat(pMessage->fileName, &statbuf ) == 0 )           // if the file exists
    {
        // file exists but is empty
        pMessage->contentLength = statbuf.st_size;
        pMessage->statusCode = 200;
        sscanf("OK", "%s", pMessage->statusText);
    }
    else                                                    // if file does not exist
    {
        pMessage->statusCode = 404;
        pMessage->contentLength = 0;
        sscanf("Not Found", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
        return;
    }
    
    H_SetResponse(pMessage);
}

bool H_CheckFileExists(httpObject* pMessage)
{
    struct stat buffer;
    return ( stat(pMessage->fileName, &buffer) == 0 );
}

void H_SetResponse(httpObject* pMessage)
{
    sprintf(pMessage->response, "%s %d %s\r\nContent-Length: %ld\r\n\r\n", pMessage->httpVersion, pMessage->statusCode, pMessage->statusText, pMessage->contentLength);
}

// if there is an error, we need to send the response early
void H_SendResponse(httpObject* pMessage)
{
    if (!pMessage->sentResponse)
    {
        H_SetResponse(pMessage);

        ssize_t sendResult = send(pMessage->client_sockd, pMessage->response, strlen(pMessage->response), 0);
        pMessage->sentResponse = true;

        size_t writeResult = write(STDOUT_FILENO, pMessage->response, strlen(pMessage->response));
        

        if(sendResult < 0)
        {
            pMessage->contentLength = 0;
            H_SetStatusOnError(pMessage);
            return;
        }

        if (writeResult < 0)
        {
            pMessage->contentLength = 0;
            H_SetStatusOnError(pMessage);
            return;
        }
    }
}

void AcceptConnection(httpObject* pMessage, struct sockaddr* client_addr, socklen_t* client_addrlen, int server_sockd)
{
    char waiting[] = "[+] server is waiting...\n";
    write(STDOUT_FILENO, waiting, strlen(waiting));

    // 1. Accept connection
    pMessage->client_sockd = accept( server_sockd, client_addr, client_addrlen ); // CHANGE THIS --------------------------

    if(pMessage->client_sockd < 0)
    {
        char acceptError[] = "Error: accept()\n";
        write(STDOUT_FILENO, acceptError, strlen(acceptError));
        pMessage->contentLength = 0;
        pMessage->sentResponse = true;
        return;
    }
}

// This function takes care of reading the message
void ReadHTTPRequest(httpObject* pMessage)
{
    if (!pMessage->sentResponse)
    {
        ssize_t bytesRead = 0;
        char tempBuf[BUFFER_SIZE];

        // if client didn't send any info 
        if( (bytesRead = recv( pMessage->client_sockd, pMessage->buffer, BUFFER_SIZE, 0)) < 0 )
        {
            //perror("Error: recv()\n");
            pMessage->contentLength = 0;
            H_SetStatusOnError(pMessage);
            H_SendResponse(pMessage);
            return;
        }
        // if client suddenly disconnected
        else if(bytesRead == 0)
        {
            //perror("client disconnected\n");
            pMessage->contentLength = 0;
            H_SetStatusOnError(pMessage);
            H_SendResponse(pMessage);
            return;
        }

        // store client request into method, filename, httpversion
        char* ptr = pMessage->buffer;
        sscanf(ptr, "%s %s %s", pMessage->method, tempBuf, pMessage->httpVersion);

        if(tempBuf[0] == '/')
        {
            memmove( tempBuf, tempBuf+1, strlen(tempBuf) );
        }

        // check if fileName is 10 and only 10 ascii characters long, else fail the request
        if (strlen(tempBuf) == 10)
        {   
            strcpy(pMessage->fileName, tempBuf);
        }
        else
        {
            pMessage->contentLength = 0;
            pMessage->statusCode = 400;
            sscanf("Bad Request", "%[^\t\n]", pMessage->statusText);
            H_SendResponse(pMessage);
            return;
        }
       
        // check if method = PUT
        if( H_CheckMethod(pMessage) == 1)
        {
            H_StoreContentLength(pMessage);
        }

        // null terminate
        pMessage->buffer[bytesRead] = 0;

    }

}

// 3. Process Request
void ProcessHTTPRequest(httpObject* pMessage)
{
    if (!pMessage->sentResponse)
    {
        // 1. Check if method name is valid
        if( H_CheckMethod(pMessage) == 0)
        {
            sscanf("Bad Request", "%[^\t\n]", pMessage->statusText);
            pMessage->statusCode = 400; 
            pMessage->contentLength = 0;
            H_SendResponse(pMessage);
            return;
        }

        // 2. Check if filename is valid
        if( H_CheckFileName(&pMessage->fileName[0]) != 0 )
        {
            pMessage->statusCode = 400; 
            sscanf("Bad Request", "%[^\t\n]", pMessage->statusText);
            pMessage->contentLength = 0;
            H_SendResponse(pMessage);
            return;
        }

        // 3. Check if HTTP version is valid
        H_CheckHTTPVersion(pMessage);

        // Figure out the content length for a GET request
        if(H_CheckMethod(pMessage) == 2)
        {
            H_ReadContentLength(pMessage);
        }
    }
}


// 4. Construct Response
void ConstructHTTPResponse(httpObject* pMessage)
{  
    if(!pMessage->sentResponse)
    {
        // Only a PUT request can create a file and set status test = "Created"
        if(H_CheckMethod(pMessage) == 1)                // put = 1
        {
            sscanf("Created", "%s", pMessage->statusText);
        }
        else if(H_CheckMethod(pMessage) == 2)            // get = 2
        {
            sscanf("OK", "%s", pMessage->statusText);
        }
        else
        {
            // invalid request
            sscanf("Bad Request", "%[^\t\n]", pMessage->statusText);
            pMessage->statusCode = 400;
            pMessage->contentLength = 0;
            H_SendResponse(pMessage);
            return;
        }

        // does file exist?
        if( H_CheckFileExists(pMessage) )
        {
            // keep in mind that this might need to change later down the line
            sscanf("OK", "%s", pMessage->statusText);
            pMessage->statusCode = 200;
        }

        // check for file permissions, if not granted access, respond with 403 error
        H_CheckReadWriteAccess(pMessage);

        H_SetResponse(pMessage);
    }
}

// 5. Send Response
void SendHTTPResponse(httpObject* pMessage)
{
    if(!pMessage->sentResponse)
    {
        if(H_CheckMethod(pMessage) != 1)
        {
            H_ReadContentLength(pMessage);
        }

        // Since we want to send a message to the console using write(), the message has to be formatted in a way like this
        // response only needs around 50 bytes but I just want to make sure our program doesn't crash due lack of space issues
        char response[200]; 
        sprintf(response, "[+] received %zu bytes from client\n[+] response:\n", pMessage->contentLength);
        
        if( write(STDERR_FILENO, response, strlen(response)) < 0)
        {
            pMessage->contentLength = 0;
            H_SetStatusOnError(pMessage);
            return;
        };

        H_CallMethod(pMessage);

        if(H_CheckMethod(pMessage) == 1)
        {
            // for a PUT request, we don't need to send content
            // length back to the client. So make it 0
            pMessage->contentLength = 0;
        }

        H_SendResponse(pMessage);
    }
}

void HandleGET(httpObject* pMessage)
{
    // does file exist?
    if( H_CheckFileExists(pMessage) )
    {
        sscanf("OK", "%s", pMessage->statusText);
        pMessage->statusCode = 200;
    }

    // 1. Attempt to open file
    int fd = open(pMessage->fileName, O_RDONLY);

    // 2. Check for errors (permissions, file doesn't exit, etc)    
    if(fd == -1)
    {
        close(fd);
        pMessage->contentLength = 0;
        H_SetStatusOnError(pMessage);
        H_SendResponse(pMessage);
        return;
    }
    else
    {  
        H_SendResponse(pMessage); 

        // 4. Read from buffer
        ssize_t bytesRead = read(fd, pMessage->buffer, sizeof(pMessage->buffer));

        if(bytesRead == -1)
        {
            warn("Error: can't read from file %s\n", pMessage->fileName);
            H_SetStatusOnError(pMessage);
            H_SendResponse(pMessage);
            return;
        }

        ssize_t totalBytesWritten = 0;
        ssize_t bytesWritten = 0;

        while(totalBytesWritten < pMessage->contentLength)
        {
            if(bytesRead == -1)
            {
                H_SetStatusOnError(pMessage);
                return;
            }
            else
            {
                bytesWritten = write(pMessage->client_sockd, pMessage->buffer, bytesRead);

                // 5. Read any remaining bytes from socket, and write to file
                bytesRead         = read(fd, pMessage->buffer, sizeof(pMessage->buffer));
                totalBytesWritten += bytesWritten;
            }
        }    
    }

    close(fd);
    H_ReadContentLength(pMessage);
}

// HandlePUT copies a file from client to server
void HandlePUT(httpObject* pMessage)
{
    // 1. Attempt to open file. If file exists, we will rewrite, if not we create a new file
    int fd = open(pMessage->fileName, O_CREAT | O_RDWR | O_TRUNC, 0644);

    // 2. Check for errors (permissions, file doesn't exit, etc)
    if(fd < 0)
    {
        close(fd);
        H_SetStatusOnError(pMessage);
        H_SendResponse(pMessage);
        return;
    }
    else        // 3. Make sure content-length: VALUE, was parsed from request
    {   
        ssize_t bytesRead = 0;

         // if the file is 0 bytes (empty)
        if (pMessage->contentLength == 0)
        {
            // DON'T recv(), you will get stuck there if you do!!!
            pMessage->statusCode = 201;
            sscanf("Created", "%s", pMessage->statusText);
            H_SetResponse(pMessage);
            close(fd);
            return;
        }

        // 4. Read from buffer
        bytesRead = read(pMessage->client_sockd, pMessage->buffer, sizeof(pMessage->buffer)); // <- stuck here if given empty file

        if(bytesRead == -1)
        {
            warn("Error: can't read from file %s\n", pMessage->fileName);
            H_SetStatusOnError(pMessage);
            H_SendResponse(pMessage);
            return;
        }

        ssize_t bytesWritten      = write(fd, pMessage->buffer, bytesRead);
        ssize_t totalBytesWritten = bytesWritten;

        while(totalBytesWritten < pMessage->contentLength)
        {
            if(bytesWritten < 0 || bytesRead < 0)
            {
                pMessage->statusCode = 500; 
                pMessage->contentLength = 0;
                H_SetStatusOnError(pMessage);
                H_SendResponse(pMessage);
                return;
            }
            else
            {
                // 5. Read any remaining bytes from socket, and write to file
                bytesRead         = read(pMessage->client_sockd, pMessage->buffer, sizeof(pMessage->buffer));
                bytesWritten      = write(fd, pMessage->buffer, bytesRead);
                totalBytesWritten += bytesWritten;   
            }
        }
        
    }

    // We will never need to send "OK" as a response after a PUT request
    // So the default will be "Created" if there are no errors    
    close(fd);

    pMessage->statusCode = 201;
    sscanf("Created", "%s", pMessage->statusText);
    H_SetResponse(pMessage);
}
