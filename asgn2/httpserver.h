//---------------------------------------------------------------------------------
// Authors:   Rory Glenn, Jeremiah Chen
// Email:     romglenn@ucsc.edu, jchen202@ucsc.edu
// Quarter:   Fall 2020
// Class:     CSE 130
// Project:   asgn2
// Professor: Faisal Nawab
//
// httpserver.h
// definition file for httpserver
//---------------------------------------------------------------------------------


#ifndef HTTPSERVER_H_
#define HTTPSERVER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h> // write
#include <string.h> // memset
#include <string>
#include <errno.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>  // atoi
#include <stdbool.h> // true false
#include <pthread.h> // thread time fun time
#include <ctype.h>
#include <queue>
#include <map>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <dirent.h>  // used for checking permissions on directories

#define NUMBER_OF_FOLDERS        3
#define DEFAULT_THREAD_POOL_SIZE 4
#define METHOD_SIZE              5                     
#define HTTPVERSION_SIZE         9
#define FILEPATH_SIZE            17
#define FILENAME_SIZE            18                  
#define STATUS_SIZE              22
#define D_NAME_SIZE              256  // store the file name read from the directory when using readdir(directory)
#define RESPONSE_SIZE            4096
#define BUFFER_SIZE              4096
#define DEFAULT_SERVER_PORT      (char*)"80"



typedef struct FileHandler
{
    bool doesExist;
    bool hasReadPermission;
    bool hasWritePermission;
    bool isLocked;
    char filePathAndName[FILEPATH_SIZE];
} FileHandler;


typedef struct HttpObject
{
    char     buffer[BUFFER_SIZE];           // Header will not be larger than 4 KiB
    char     method[METHOD_SIZE];           // PUT, GET
    char     fileName[FILENAME_SIZE];       // The file we are trying to create or read from
    char     httpVersion[HTTPVERSION_SIZE]; // HTTP/1.1
    char     statusText[STATUS_SIZE];       // "OK", "Bad Request", "Not Found", etc..
    char     response[RESPONSE_SIZE];       // Contains the full response send to client
    char     afterExpect100[BUFFER_SIZE];   // Grabs any extra file contents on first read() 
    char*    port;                          // port number given on command line
    int      contentLength;                 // size of the clients file
    int      clientSocket;                  // socket the client is on
    int      numberOfThreads;               // number of threads
    int      statusCode;                    // 200, 201, 400, 403, 404, 500
    bool     rFlag;                         // -l flag
    bool     NFlag;                         // -N flag
    bool     hasSent;                       // have we sent the response to the client
    bool     doesExist;                     // Does the file exist for a GET request
    bool     hasReadPermission;             // Do we have read permission for a GET request
    std::vector<std::string> vecFiles;      // when redundancy is not enabled, stores the filenames in the server folder
    std::vector<std::string> vecCopy1Files; // when redundancy is enabled, stores the filenames in folder "copy1"
    std::vector<std::string> vecCopy2Files; // when redundancy is enabled, stores the filenames in folder "copy2"
    std::vector<std::string> vecCopy3Files; // when redundancy is enabled, stores the filenames in folder "copy3"
} HttpObject;


typedef struct ThreadHandler
{
    int        threadID;
    int        numberOfThreads;
    bool       NFlag;
    pthread_t  thread;
    HttpObject message;
} ThreadHandler;


// functions for creating scanning files in directories and creating locks for each one
void ScanExistingFiles(HttpObject* pMessage);
bool IsFile(const char* path);
void PrintFileNamesInDirectory(HttpObject* pMessage);
void PushLocksToMap(HttpObject* pMessage);
void PrintMap();


// main functions
void AcceptConnection(HttpObject*      pMessage, struct sockaddr *client_addr, socklen_t *client_addrlen, int server_sockd);
void ReadHTTPRequest(HttpObject*       pMessage);
void ProcessHTTPRequest(HttpObject*    pMessage);
void ConstructHTTPResponse(HttpObject* pMessage);
void SendHTTPResponse(HttpObject*      pMessage);

// Handle functions
void HandleGET(HttpObject* pMessage);
void HandlePUT(HttpObject* pMessage);

// for writing errors to console
void WriteErrorAndExit(char message[], char arg[]);
void WriteErrorAndExit(char message[]);
void WriteError(char message[], char arg[]);
void WriteError(char message[], int arg);
void WriteError(char message[]);

// helper functions
int  H_CheckFileName(HttpObject* pMessage);
int  H_CheckMethod(HttpObject* pMessage);
void H_CheckGETReadAccess(HttpObject* pMessage);
void H_CheckHTTPVersion(HttpObject* pMessage);
void H_CallMethod(HttpObject* pMessage);
void H_StoreContentLength(HttpObject *pMessage, char requestBuffer[]);
void H_SetStatusOnError(HttpObject* pMessage);
void H_ReadContentLength(HttpObject* pMessage);
void H_SetResponse(HttpObject* pMessage);
void H_SendResponse(HttpObject* pMessage);
bool H_CheckFileExists(HttpObject* pMessage);
unsigned long H_GetAddress(char* name);

// new functions for asgn2
void ReadCommandLine(int argc, char **argv, HttpObject *pMessage);
void Thread_Init(ThreadHandler* pThreadHandler, HttpObject *pMessage);
void *HandleRequests(void *data);
void HandleGETRFlag(HttpObject* pMessage);
void HandlePutRFlag(HttpObject* pMessage);
void H_CountAfterExpect100(HttpObject *pMessage, char requestBuffer[]);
void H_GetContentAfterExpect100(HttpObject* pMessage, char requestBuffer[]);

// redundancy functions
bool H_CheckRedundancyFileExists(FileHandler* pFile);
bool H_IsFilenameInMap(FileHandler* pFile);
bool H_IsFilenameInMap(HttpObject* pMessage);
void H_SetRedundancyFolderPermission(FileHandler* pFileHandler);
void H_RedundancyTryPushingFileLockToMap(FileHandler* pFolder, FileHandler* pFile);
void H_SetRedundancyPUTFileExistenceAndWritePermission(FileHandler* pFileHandler);
void H_SetRedundancyGETFileExistenceAndReadPermission(FileHandler* pFileHandler);
bool H_CheckIfGETRedundancyFilesAreIdentical(FileHandler* pFile1, FileHandler* pFile2);
void LockExistingFiles(FileHandler* pFile1, FileHandler* pFile2, FileHandler* pFile3);
void UnlockExistingFiles(FileHandler* pFile1, FileHandler* pFile2, FileHandler* pFile3);
void SendResponseBasedOnRedundancyFolderExistence(HttpObject* pMessage, FileHandler* pFolder1, FileHandler* pFolder2, FileHandler* pFolder3);
void SendResponseBasedOnRedundancyFolderPermission(HttpObject* pMessage, FileHandler* file1, FileHandler* file2, FileHandler* file3);
void SendResponseBasedOnGETRedundancyFileReadPermission(HttpObject* pMessage, FileHandler* file1, FileHandler* file2, FileHandler* file3);
void SendResponseBasedOnPUTRedundancyFileWritePermission(HttpObject* pMessage, FileHandler* file1, FileHandler* file2, FileHandler* file3);
void SendResponseBasedOnRedundancyGETFileDifference(HttpObject* pMessage, FileHandler* pFile1, FileHandler* pFile2, FileHandler* pFile3);
void SendResponseBasedOnGETRedundancyFileExistence(HttpObject* pMessage, FileHandler* file1, FileHandler* file2, FileHandler* file3);
void FinalizeHandleGetRFlag(HttpObject* pMessage, FileHandler* pFile1, FileHandler* pFile2, FileHandler* pFile3);


#endif