//---------------------------------------------------------------------------------
// Authors:   Rory Glenn, Jeremiah Chen
// Email:     romglenn@ucsc.edu, jchen202@ucsc.edu
// Quarter:   Fall 2020
// Class:     CSE 130
// Project:   asgn3: Back-up and Recovery in the HTTP server
// Professor: Faisal Nawab
//
// httpserver.h
// definition file for httpserver
//---------------------------------------------------------------------------------


#ifndef HTTPSERVER_H_
#define HTTPSERVER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <iostream>
#include <ctime>
#include <vector>
#include <dirent.h>
#include <algorithm>


#define METHOD_SIZE      5
#define HTTPVERSION_SIZE 9
#define FILENAME_SIZE    11                     // all valid resource names must be 10 ascii characters long
#define STATUS_SIZE      21
#define RESPONSE_SIZE    4096
#define BUFFER_SIZE      4096                   // Header will not be larger than 4 KiB


typedef struct HttpObject
{
    int      clientSocket;                            // socket the client is on
    int      statusCode;                              // 200, 201, 400, 403, 404, 500
    char     buffer[BUFFER_SIZE];                     // Header will not be larger than 4 KiB
    char     method[METHOD_SIZE];                     // PUT or GET 
    char     fileName[FILENAME_SIZE];                 // what is the file we are worried about
    char     httpVersion[HTTPVERSION_SIZE];           // HTTP/1.1 should be the only version
    char     statusText[STATUS_SIZE];                 // "OK", "Bad Request", "Not Found", etc..
    char     response[RESPONSE_SIZE];                 // Contains the full response send to client
    bool     hasSentResponse;                         // checks whether the response has been sent
    long int contentLength;                           // size of the clients file
    
    // Backup() variables
    std::vector<std::string> vecBackupFiles;          // stores backup file names
    std::vector<std::string> vecBackupFolder;         // stores backup folder names

    // Recovery() and RecoverySpecial() variables    
    std::vector<std::string> vecBackups;              // used in Recovery_GetMostRecentBackup()
    std::vector<std::string> vecRecoveryFiles;        // holds files in recovery folder
    std::vector<std::string> vecRecoverySpecialFiles; // holds files in specific recovery folder
    std::string              recoveryFolder;          // most recent recovery folder
    std::string              recoverySpecialFolder;   // specific recovery folder

} HttpObject;


// asgn1 main functions
void AcceptConnection( HttpObject* pMessage, struct sockaddr* client_addr, socklen_t* client_addrlen, int server_sockd);
void ReadHTTPRequest( HttpObject* pMessage);
void ProcessHTTPRequest( HttpObject* pMessage);
void ConstructHTTPResponse( HttpObject* pMessage);
void SendHTTPResponse( HttpObject* pMessage);

// asgn1 helper functions
unsigned long H_GetAddress(char *name);
int  H_CheckFileName(char* name);
int  H_CheckMethod( HttpObject* pMessage);
void H_CheckReadWriteAccess(HttpObject* pMessage);
void H_CheckHTTPVersion( HttpObject* pMessage);
void H_CallMethod( HttpObject* pMessage);
void H_StoreContentLength( HttpObject* pMessage);
void H_SetStatusOnError( HttpObject* pMessage);
void H_ReadContentLength( HttpObject* pMessage);
bool H_CheckFileExists( HttpObject* pMessage);
void H_SetResponse( HttpObject* pMessage);
void H_SendResponse( HttpObject* pMessage);

void GET( HttpObject* pMessage);
void PUT( HttpObject* pMessage);


                                            // for asgn3
// error writing
void WriteErrorAndExit(char message[]);
void WriteError(char message[]);
void WriteError(char message[], int arg);
void WriteError(char message[], char arg[]);


// helper functions
std::string H_GetTimeStamp();
bool        H_CreateZeroByteFile(const char* fileName, const char* filePath);
bool        H_CreateZeroByteFile(const char* path);
bool        H_IsFile(const char* fileName);
bool        H_IsFolder(const char* fileName);


// Backup functions
void Backup(HttpObject* pMessage);
void Backup_ScanExistingFiles(HttpObject* pMessage);
void Backup_CopyFiles(HttpObject* pMessage);


// Primary Recovery functions
void Recovery(HttpObject* pMessage);
void Recovery_ScanBackupDirectories(HttpObject* pMessage);
bool Recovery_GetMostRecentBackup(HttpObject* pMessage);
void Recovery_ScanExistingFiles(HttpObject* pMessage);
void Recovery_CopyFiles(HttpObject* pMessage);


// Special Recovery functions
void RecoverySpecial(HttpObject* pMessage);
void RecoverySpecial_CopyFiles(HttpObject* pMessage);
void RecoverySpecial_ScanExistingFiles(HttpObject* pMessage);


// List functions
void                     List(HttpObject* pMessage);
std::vector<std::string> List_GetExistingDirectories();


#endif // HTTPSERVER_H_