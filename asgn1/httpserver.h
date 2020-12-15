//---------------------------------------------------------------------------------
// Authors:   Rory Glenn, Jeremiah Chen
// Email:     romglenn@ucsc.edu, jchen202@ucsc.edu
// Quarter:   Fall 2020
// Class:     CSE 130
// Project:   asgn1
// Professor: Faisal Nawab
//
// httpserver.h
// definition file for httpserver
//---------------------------------------------------------------------------------

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>    // write()
#include <string.h>    // memset()
#include <errno.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>    // atoi
#include <stdbool.h>   // true/false
#include <iostream>

#define BUFFER_SIZE      4096                   // Header will not be larger than 4 KiB
#define METHOD_SIZE      5                      // change this to 3 only
#define FILENAME_SIZE    11                     // all valid resource names must be 10 ascii characters long
#define HTTPVERSION_SIZE 9
#define STATUS_SIZE      21
#define RESPONSE_SIZE    4096

typedef struct httpObject
{
    int      client_sockd;                      // socket the client is on
    int      statusCode;                        // 200, 201, 400, 403, 404, 500
    char     buffer[BUFFER_SIZE + 1];           // Header will not be larger than 4 KiB
    char     method[METHOD_SIZE];               // PUT or GET 
    char     fileName[FILENAME_SIZE];           // what is the file we are worried about
    char     httpVersion[HTTPVERSION_SIZE];     // HTTP/1.1 should be the only version
    char     statusText[STATUS_SIZE];           // "OK", "Bad Request", "Not Found", etc..
    char     response[RESPONSE_SIZE];           // Contains the full response send to client
    bool     sentResponse = false;              // checks whether the response has been sent
    long int contentLength;                     // size of the clients file
} httpObject;

void AcceptConnection( httpObject* pMessage, struct sockaddr* client_addr, socklen_t* client_addrlen, int server_sockd);
void ReadHTTPRequest( httpObject* pMessage);
void ProcessHTTPRequest( httpObject* pMessage);
void ConstructHTTPResponse( httpObject* pMessage);
void SendHTTPResponse( httpObject* pMessage);

unsigned long GetAddress(char *name);

int  H_CheckFileName(char* name);
int  H_CheckMethod( httpObject* pMessage);
void H_CheckReadWriteAccess(httpObject* pMessage);
void H_print( httpObject* pMessage);
void H_CheckHTTPVersion( httpObject* pMessage);
void H_CallMethod( httpObject* pMessage);
void H_StoreContentLength( httpObject* pMessage);
void H_SetStatusOnError( httpObject* pMessage);
void H_ReadContentLength( httpObject* pMessage);
bool H_CheckFileExists( httpObject* pMessage);
void H_SetResponse( httpObject* pMessage);
void H_SendResponse( httpObject* pMessage);

void HandleGET( httpObject* pMessage);
void HandlePUT( httpObject* pMessage);
void HandleHEAD( httpObject* pMessage);