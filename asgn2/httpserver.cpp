//---------------------------------------------------------------------------------
// Authors:   Rory Glenn, Jeremiah Chen
// Email:     romglenn@ucsc.edu, jchen202@ucsc.edu
// Quarter:   Fall 2020
// Class:     CSE 130
// Project:   asgn2
// Professor: Faisal Nawab
//
// httpserver.cpp
// Implementaion file for httpserver
//---------------------------------------------------------------------------------

#include "httpserver.h"


pthread_cond_t  globalConditionVariable    = PTHREAD_COND_INITIALIZER;               // allows our threads to wait for some condition to occur
pthread_mutex_t globalRecursiveLock        = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP; // recursive lock for the globalRequestQueue

// for each file that was created, create a new key,value pair (filename, fileLock) -> aka dictionary
std::map<std::string, pthread_mutex_t> globalMap;

// a global queue to hold our requests in FIFO order
std::queue<int>* globalRequestQueue;


int main(int argc, char **argv)
{
    HttpObject msg;
    msg.rFlag            = false;
    msg.NFlag            = false;
    msg.port             = DEFAULT_SERVER_PORT;
    msg.numberOfThreads  = DEFAULT_THREAD_POOL_SIZE;

    int                enable        = 1;
    int                serverSocket  = 0;
    char*              address       = argv[1];
    socklen_t          clientAddressLength;
    struct sockaddr_in serverAddress;
    struct sockaddr    clientAddress;
    ThreadHandler*     pThreadHandler;

    globalRequestQueue = new std::queue<int>();

    memset(&serverAddress, 0, sizeof(serverAddress));
    
    ReadCommandLine(argc, argv, &msg);

    // initialize pThreadHandler after we give ReadCommandLine() the number of threads
    pThreadHandler = new ThreadHandler[msg.numberOfThreads];
    
    serverAddress.sin_family      = AF_INET;
    serverAddress.sin_port        = htons(atoi(msg.port));
    serverAddress.sin_addr.s_addr = H_GetAddress(address); 

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) <= 0) { WriteErrorAndExit( (char*)"Error: socket()\n"); }

    // This allows you to avoid: 'Bind: Address Already in Use' error
    if ((setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) < 0) { WriteErrorAndExit( (char*)"Error: setsockopt()\n"); }

    // Bind/attach server address to socket that is open
    if ((bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress))) < 0) { WriteErrorAndExit( (char*)"Error: bind()\n"); }

    // Listen for incoming connections
    if ((listen(serverSocket, 500)) < 0) { WriteErrorAndExit( (char*)"Error: listen()\n"); }


    ScanExistingFiles(&msg);
    PushLocksToMap(&msg);
    // PrintFileNamesInDirectory(&msg);
    // PrintMap();

    // initialize worker threads
    Thread_Init(pThreadHandler, &msg);

    // main loop is only responsible for accepting new connections and pushing them to the queue
    while (true)
    {
        HttpObject message;
        message.hasSent     = false;
        message.rFlag       = msg.rFlag;
        memset(message.buffer,         0, sizeof(message.buffer));
        memset(message.afterExpect100, 0, sizeof(message.afterExpect100));

        // accept a new incoming connection
        AcceptConnection(&message, (struct sockaddr *)&clientAddress, (socklen_t *)&clientAddressLength, serverSocket);


        // after you accept the connection, add the request to the queue
        pthread_mutex_lock(&globalRecursiveLock);
        // ----------------------------------------------------- globalRequestQueue Start Critical Section ------------------------------------------------------------------
            globalRequestQueue->push(message.clientSocket);
            pthread_cond_signal(&globalConditionVariable);  // wakes up one of the waiting threads and tells it to get to work
        // ----------------------------------------------------- globalRequestQueue End Critical Section ------------------------------------------------------------------
        pthread_mutex_unlock(&globalRecursiveLock);

    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                                                           NEW FUNCTIONS FOR ASGN2 BELOW
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// prints out all the scanned files with or without redundancy
void PrintFileNamesInDirectory(HttpObject* pMessage)
{

    if(pMessage->rFlag)
    {
        int n = 0;

        for(unsigned long i = 0; i < pMessage->vecCopy1Files.size(); i++)
        {
            printf("file[%d]: ", n);
            std::cout << pMessage->vecCopy1Files[i] << std::endl;
            n++;
        }
        for(unsigned long i = 0; i < pMessage->vecCopy2Files.size(); i++)
        {
            printf("file[%d]: ", n);
            std::cout << pMessage->vecCopy2Files[i] << std::endl;
            n++;
        }
        for(unsigned long i = 0; i < pMessage->vecCopy3Files.size(); i++)
        {
            printf("file[%d]: ", n);
            std::cout << pMessage->vecCopy3Files[i] << std::endl;      
            n++;  
        }
    }
    else if(!pMessage->rFlag)
    {
        int n = 0;
        for(unsigned long i = 0; i < pMessage->vecFiles.size(); i++)
        {
            printf("file[%d]: ", n);
            std::cout << pMessage->vecFiles[i] << std::endl;    
            n++;     
        }
    }
}


bool IsFile(const char* path)
{
    struct stat statPath;
    stat(path, &statPath);
    return S_ISREG(statPath.st_size);
}


void ScanExistingFiles(HttpObject* pMessage)
{
    DIR* directory;
    struct dirent* entry;

    if(pMessage->rFlag)
    {
        const char* folders[3] = {"copy1", "copy2", "copy3"};

        for(int i = 0; i < NUMBER_OF_FOLDERS; i++)
        {
            if( (directory = opendir(folders[i])) != NULL)
            {
                // get all the files and directories within directory
                while( (entry = readdir(directory)) != NULL)
                {
                    // if it is a normal file and not a directory
                    if(strcmp(folders[i], "copy1") == 0)
                    {
                        if(IsFile(entry->d_name) == 0)
                        {
                            // prepend the name of the folder before the file name so every name will be unique
                            std::string tempName = "copy1/";
                            tempName += entry->d_name;
                            pMessage->vecCopy1Files.push_back(tempName);
                        }
                        
                    }
                    else if(strcmp(folders[i], "copy2") == 0)
                    {
                        if(IsFile(entry->d_name) == 0)
                        {
                            std::string tempName = "copy2/";
                            tempName += entry->d_name;
                            pMessage->vecCopy2Files.push_back(tempName);
                        }
                        
                    }
                    else if(strcmp(folders[i], "copy3") == 0)
                    {
                        if(IsFile(entry->d_name) == 0)
                        {
                            std::string tempName = "copy3/";
                            tempName += entry->d_name;
                            pMessage->vecCopy3Files.push_back(tempName);
                        }
                        
                    }

                }

                closedir(directory);
            }
            else
            {
                printf("Error: could not open directory %s\n", folders[i]);
                closedir(directory);
            }

        }

        
    }
    else if(!pMessage->rFlag)
    {
        if( (directory = opendir(".")) != NULL)
        {
            while( (entry = readdir(directory)) != NULL)
            {
                if(IsFile(entry->d_name) == 0)
                {
                    pMessage->vecFiles.push_back(entry->d_name);
                }
            }
        }
        else
        {
            printf("Error: could not open directory\n");
        }

        closedir(directory);
    }

}

// Takes all of the filenames that were scanned and stored into the vectors and creates a lock
// for each one of them because we call the function before creating any threads, we don't need to lock globalMap
void PushLocksToMap(HttpObject* pMessage)
{
    if(pMessage->rFlag)
    {
        for(unsigned long i = 0; i < pMessage->vecCopy1Files.size(); i++)
        {
            pthread_mutex_t fileLock;
            pthread_mutex_init(&fileLock, NULL);
            globalMap.insert(std::pair<std::string, pthread_mutex_t>(pMessage->vecCopy1Files[i], fileLock));
        }
        for(unsigned long i = 0; i < pMessage->vecCopy2Files.size(); i++)
        {
            pthread_mutex_t fileLock;
            pthread_mutex_init(&fileLock, NULL);
            globalMap.insert(std::pair<std::string, pthread_mutex_t>(pMessage->vecCopy2Files[i], fileLock));
        }
        for(unsigned long i = 0; i < pMessage->vecCopy3Files.size(); i++)
        {
            pthread_mutex_t fileLock;
            pthread_mutex_init(&fileLock, NULL);
            globalMap.insert(std::pair<std::string, pthread_mutex_t>(pMessage->vecCopy3Files[i], fileLock));
        }

    }
    else if(!pMessage->rFlag)
    {
        for(unsigned long i = 0; i < pMessage->vecFiles.size(); i++)
        {
            pthread_mutex_t fileLock;
            pthread_mutex_init(&fileLock, NULL);
            globalMap.insert(std::pair<std::string, pthread_mutex_t>(pMessage->vecFiles[i], fileLock));
        } 
    }
}

void PrintMap()
{
    int i = 0;
    for (auto iter = globalMap.begin(); iter != globalMap.end(); ++iter)
    {
        printf("globalMap[%d]: ", i);
        std::cout << (*iter).first << std::endl;
        i++;
    }
}

// for writing errors to the server console
void WriteErrorAndExit(char message[], char arg[])
{
    char errorMessage[BUFFER_SIZE];
    sprintf(errorMessage, message, arg);
    write(STDERR_FILENO, errorMessage, strlen(errorMessage)); 
    exit(EXIT_FAILURE);
}

// for writing errors to the server console with no formatting
void WriteErrorAndExit(char message[])
{
    write(STDERR_FILENO, message, strlen(message)); 
    exit(EXIT_FAILURE);
}

void WriteError(char message[], char arg[])
{
    char errorMessage[BUFFER_SIZE];
    // combine message and arg into one string
    sprintf(errorMessage, message, arg);
    // write it to console
    write(STDERR_FILENO, errorMessage, strlen(errorMessage)); 
}


void WriteError(char message[], int arg)
{
    char errorMessage[BUFFER_SIZE];
    sprintf(errorMessage, message, arg);
    write(STDERR_FILENO, errorMessage, strlen(errorMessage)); 
}

void WriteError(char message[])
{
    write(STDERR_FILENO, message, strlen(message)); 
}


// if filename is in map, return true, else return false
bool H_IsFilenameInMap(FileHandler* pFile)
{
    std::map<std::string, pthread_mutex_t>::iterator iter = globalMap.find(pFile->filePathAndName);

    if(iter != globalMap.end())
    {
        // found the filename
        return true;
    }

    // did NOT find the filename
    return false;
}


bool H_IsFilenameInMap(HttpObject* pMessage)
{
    std::map<std::string, pthread_mutex_t>::iterator iter = globalMap.find(pMessage->fileName);

    if(iter != globalMap.end())
    {
        // found the filename
        return true;
    }

    // did NOT find the filename
    return false;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                                                            Start Redundancy Functions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// return true of false based on file existence
bool H_CheckRedundancyFileExists(FileHandler* pFile)
{
    struct stat buffer;

    if(stat(pFile->filePathAndName, &buffer) == 0)
    {
        pFile->doesExist = true;
        return true;
    }
    else
    {
        pFile->doesExist = false;
        return false;
    }

}


// can be used for GET and PUT
void H_SetRedundancyFolderPermission(FileHandler* pFolder)
{
    struct stat statbuf;
    DIR* pDirectory = nullptr;

    // check if the file exists
    if(stat(pFolder->filePathAndName, &statbuf) == 0)
    {
        pFolder->doesExist = true;

        pDirectory = opendir(pFolder->filePathAndName);

        // opendir returns NULL if couldn't open directory 
        if (  pDirectory == NULL )  
        { 
            closedir(pDirectory);
            pFolder->hasReadPermission = false;
            return;
        } 
    }
    else
    {
        pFolder->doesExist = false;
        return;
    }
    
    closedir(pDirectory);
    pFolder->hasReadPermission = true;
}

void H_RedundancyTryPushingFileLockToMap(FileHandler* pFolder, FileHandler* pFile)
{
    // 1. if we have folder read permission
    // 2.   if the file exists
    // 3.       if the filename is not in the map
    // 4.           push filelock to map

    //pthread_mutex_t file1Lock;

    H_SetRedundancyFolderPermission(pFolder);

    // 1.
    if(pFolder->hasReadPermission)
    {
        // 2.
        if(H_CheckRedundancyFileExists(pFile))
        {
            pFile->doesExist = true;

            // 3.
            // if(!H_IsFilenameInMap(pFile))
            // {
            //     // if we DO NOT already have a lock for the files, create one and add it to the map
            //     pthread_mutex_init(&file1Lock, NULL);
            //     globalMap.insert(std::pair<std::string, pthread_mutex_t>(pFile->filePathAndName, file1Lock));
            // }
        }
    }    
}






void H_GetFileLock(FileHandler* pFolder, FileHandler* pFile)
{
    // 1. if we have folder read permission
    // 2.   if the file exists
    // 3.       if the filename is in the map
    // 4.           push filelock to map


    H_SetRedundancyFolderPermission(pFolder);

    // 1.
    if(pFolder->hasReadPermission)
    {
        // 2.
        if(H_CheckRedundancyFileExists(pFile))
        {
            pFile->doesExist = true;

            // 3.
            if(H_IsFilenameInMap(pFile))
            {
               // return true;
            }
        }
    }
}






// handles the read/write access differently than the one above
// it will not send an error message but will return either true of false if we have Read access
void H_SetRedundancyGETFileExistenceAndReadPermission(FileHandler* pFileHandler)
{
    struct stat statbuf;

    // check if the file exists
    if(stat(pFileHandler->filePathAndName, &statbuf) == 0)
    {
        pFileHandler->doesExist = true;

        // check read permission
        if( !(statbuf.st_mode & S_IROTH))
        {
            pFileHandler->hasReadPermission = false;
            return;
        } 
    }
    else
    {
        pFileHandler->doesExist = false;
        return;
    }
    
    pFileHandler->hasReadPermission = true;

}

// this helper function handles the read/write access differently than the one above
// it will not send an error message but will return either true of false if we have Read access
void H_SetRedundancyPUTFileExistenceAndWritePermission(FileHandler* pFileHandler)
{
    struct stat statbuf;

    // check if the file exists
    if(stat(pFileHandler->filePathAndName, &statbuf) == 0)
    {
        pFileHandler->doesExist = true;

        // others have write permission
        if( statbuf.st_mode & (S_IWUSR | S_IWOTH) )
        {
            pFileHandler->hasWritePermission = true;
        }
        else
        {
            pFileHandler->hasWritePermission = false;
        }
    }
    else
    {
        pFileHandler->doesExist = false;
    }
}


// if 2/3 folders don't exist, send a 404 Not Found error
void SendResponseBasedOnRedundancyFolderExistence(HttpObject* pMessage, FileHandler* pFolder1, FileHandler* pFolder2, FileHandler* pFolder3)
{
    if( (!pFolder1->doesExist && !pFolder2->doesExist) ||
        (!pFolder2->doesExist && !pFolder3->doesExist) || 
        (!pFolder1->doesExist && !pFolder3->doesExist) )
    {
        pMessage->statusCode = 404;
        pMessage->contentLength = 0;
        sscanf("Not Found", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
    }
}


// if 2/3 folders don't have read permissions, send a 403 Forbidden error
void SendResponseBasedOnRedundancyFolderPermission(HttpObject* pMessage, FileHandler* pFolder1, FileHandler* pFolder2, FileHandler* pFolder3)
{
    if( (!pFolder1->hasReadPermission && !pFolder2->hasReadPermission) ||
        (!pFolder2->hasReadPermission && !pFolder3->hasReadPermission) || 
        (!pFolder1->hasReadPermission && !pFolder3->hasReadPermission) )
    {
        pMessage->statusCode = 403;
        pMessage->contentLength = 0;
        sscanf("Forbidden", "%s", pMessage->statusText);
        H_SendResponse(pMessage);
    }

}

 // if all three files are different from each other, send a 500 error
void SendResponseBasedOnRedundancyGETFileDifference(HttpObject* pMessage, FileHandler* pFile1, FileHandler* pFile2, FileHandler* pFile3)
{
    if( (!pFile1->doesExist && !pFile2->hasReadPermission && (pFile3->doesExist && pFile3->hasReadPermission)) ||
        (!pFile1->doesExist && !pFile3->hasReadPermission && (pFile2->doesExist && pFile2->hasReadPermission)) ||
        (!pFile2->doesExist && !pFile1->hasReadPermission && (pFile3->doesExist && pFile3->hasReadPermission)) ||
        (!pFile2->doesExist && !pFile3->hasReadPermission && (pFile1->doesExist && pFile1->hasReadPermission)) ||
        (!pFile3->doesExist && !pFile1->hasReadPermission && (pFile2->doesExist && pFile2->hasReadPermission)) ||
        (!pFile3->doesExist && !pFile2->hasReadPermission && (pFile1->doesExist && pFile1->hasReadPermission)) )
    {
        pMessage->statusCode    = 500;
        pMessage->contentLength = 0;
        sscanf("Internal Server Error", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
        return;
    }   
}


// if 2/3 files don't exist, send a 404 Not Found error
void SendResponseBasedOnGETRedundancyFileExistence(HttpObject* pMessage, FileHandler* file1, FileHandler* file2, FileHandler* file3)
{
    if( (!file1->doesExist && !file2->doesExist) || 
        (!file2->doesExist && !file3->doesExist) ||
        (!file1->doesExist && !file3->doesExist) )
    {
        // No such file or directory
        pMessage->statusCode = 404;
        pMessage->contentLength = 0;
        sscanf("Not Found", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
    }

}


// if the files have no read access, fail the request
void SendResponseBasedOnGETRedundancyFileReadPermission(HttpObject* pMessage, FileHandler* file1, FileHandler* file2, FileHandler* file3)
{

    if( (!file1->hasReadPermission && !file2->hasReadPermission) || 
        (!file2->hasReadPermission && !file3->hasReadPermission) ||
        (!file1->hasReadPermission && !file3->hasReadPermission) )
    {
        // Read Permission denied
        pMessage->statusCode = 403;
        pMessage->contentLength = 0;
        sscanf("Forbidden", "%s", pMessage->statusText);
        H_SendResponse(pMessage);
    }

}

void FinalizeHandleGetRFlag(HttpObject* pMessage, FileHandler* pFile1, FileHandler* pFile2, FileHandler* pFile3)
{
    if( H_CheckIfGETRedundancyFilesAreIdentical(pFile1, pFile2) )
    {
        //printf("files %s %s are identical\n", pFile1->filePathAndName, pFile2->filePathAndName);
        strcpy(pMessage->fileName, pFile1->filePathAndName);
        // we bypassed this earlier because the correct file name was not set until now,
        // therefore it would have said that the filename did not exist
        H_ReadContentLength(pMessage);
        // we need to change pMessage->fileName to one of the new files before we can call HandleGET
        HandleGET(pMessage);
    }
    else if( H_CheckIfGETRedundancyFilesAreIdentical(pFile2, pFile3) )
    {
        //printf("files %s %s are identical\n", pFile2->filePathAndName, pFile3->filePathAndName);
        strcpy(pMessage->fileName, pFile2->filePathAndName);
        H_ReadContentLength(pMessage);
        HandleGET(pMessage);
    }
    else if( H_CheckIfGETRedundancyFilesAreIdentical(pFile1, pFile3) )
    {
        //printf("files %s %s are identical\n", pFile1->filePathAndName, pFile3->filePathAndName);
        strcpy(pMessage->fileName, pFile1->filePathAndName);
        H_ReadContentLength(pMessage);
        HandleGET(pMessage);
    }

}


void LockExistingFiles(FileHandler* pFile1, FileHandler* pFile2, FileHandler* pFile3)
{
    if(pFile1->doesExist)
    {
        if(H_IsFilenameInMap(pFile1))
        {
            pthread_mutex_lock(&globalMap[pFile1->filePathAndName]);
            pFile1->isLocked = true;
        }
    }

    if(pFile2->doesExist)
    {
        if(H_IsFilenameInMap(pFile2))
        {
            pthread_mutex_lock(&globalMap[pFile2->filePathAndName]);
            pFile2->isLocked = true;
        }

    }
    
    if(pFile3->doesExist)
    {
        if(H_IsFilenameInMap(pFile3))
        {
            pthread_mutex_lock(&globalMap[pFile3->filePathAndName]);
            pFile3->isLocked = true;
        }
    }

}


void UnlockExistingFiles(FileHandler* pFile1, FileHandler* pFile2, FileHandler* pFile3)
{
    if(pFile1->isLocked)
    {
        pthread_mutex_unlock(&globalMap[pFile1->filePathAndName]);
        pFile1->isLocked = false;
    }

    if(pFile2->isLocked)
    {
        pthread_mutex_unlock(&globalMap[pFile2->filePathAndName]);
        pFile2->isLocked = false;
    }
    
    if(pFile3->isLocked)
    {
        pthread_mutex_unlock(&globalMap[pFile3->filePathAndName]);
        pFile3->isLocked = false;
    }

}


// if we don't have write permission to 2/3 files or more, send a 403 message
void SendResponseBasedOnPUTRedundancyFileWritePermission(HttpObject* pMessage, FileHandler* file1, FileHandler* file2, FileHandler* file3)
{
    if( (!file1->hasWritePermission && !file2->hasWritePermission) ||
        (!file2->hasWritePermission && !file3->hasWritePermission) ||
        (!file1->hasWritePermission && !file3->hasWritePermission) )
    {
        // write Permission denied
        pMessage->statusCode = 403;
        pMessage->contentLength = 0;
        sscanf("Forbidden", "%s", pMessage->statusText);
        H_SendResponse(pMessage);
    }
   
}


// used only for a GET request with Redundancy
// if files are identical (1. both files exist, 2. filesizes are the same, 3. contents are the same), return true
bool H_CheckIfGETRedundancyFilesAreIdentical(FileHandler* pFile1, FileHandler* pFile2)
{

    int         fd1                = 0;
    int         fd2                = 0;
    int         totalBytesRead     = 0;
    int         file1ContentLength = 0;
    int         file2ContentLength = 0;
    char        file1Buffer[BUFFER_SIZE];
    char        file2Buffer[BUFFER_SIZE];
    ssize_t     bytesRead1 = 0;
    ssize_t     bytesRead2 = 0;
    struct stat statbuf1;
    struct stat statbuf2;

    pFile1->doesExist = true;
    pFile2->doesExist = true;

    // 1. check if files exist
    if(stat(pFile1->filePathAndName, &statbuf1) != 0) { pFile1->doesExist = false; }
    if(stat(pFile2->filePathAndName, &statbuf2) != 0) { pFile2->doesExist = false; }

    // if one of the files doesn't exist, return 0
    if(!pFile1->doesExist || !pFile2->doesExist) { return false; }

    file1ContentLength = statbuf1.st_size;
    file2ContentLength = statbuf2.st_size;

    // check if the files have the same content length
    if(statbuf1.st_size != statbuf2.st_size) { return false; }

    fd1 = open(pFile1->filePathAndName, O_RDONLY);
    fd2 = open(pFile2->filePathAndName, O_RDONLY);


    // if we can't open both files, then we can't check if they are identical
    if(fd1 < 0 || fd2 < 0)
    {
        close(fd1);
        close(fd2);
        return false;
    }

    // without the -1, we write over the null character and the buffers do not copy information correctly
    bytesRead1 = read( fd1, file1Buffer, BUFFER_SIZE - 1 );
    bytesRead2 = read( fd2, file2Buffer, BUFFER_SIZE - 1 );

    file1Buffer[bytesRead1] = '\0';
    file2Buffer[bytesRead1] = '\0';

    totalBytesRead += bytesRead1;

    // if our buffer grabbed all of the file contents then we don't need to loop and read again
    if(strcmp(file1Buffer, file2Buffer) != 0)
    {
        // files are not the same
        close(fd1);
        close(fd2);
        return false;
    }

    // if the file size is greater than our buffer of 4kb, we need to loop through the contents and read()
    while(totalBytesRead < statbuf1.st_size)
    {
        // 1. get a buffers worth of content
        // 2. call strcmp on the buffer
        // 3. if no diff, get another buffers worth and repeat the process

        bytesRead1      = read( fd1, file1Buffer, BUFFER_SIZE-1);
        bytesRead2      = read( fd2, file2Buffer, BUFFER_SIZE-1);
        totalBytesRead += bytesRead1;

        // if the files are not the same
        if(strcmp(file1Buffer, file2Buffer) != 0)
        {
            close(fd1);
            close(fd2);
            return false;
        }

    }

    close(fd1);
    close(fd2);
    return true;
}



void HandleGETRFlag(HttpObject* pMessage)
{

    FileHandler file1;
    FileHandler file2;
    FileHandler file3;

    FileHandler folder1;
    FileHandler folder2;
    FileHandler folder3;

    // set the folder name
    sprintf(folder1.filePathAndName, "%s", (char*)"copy1");
    sprintf(folder2.filePathAndName, "%s", (char*)"copy2");
    sprintf(folder3.filePathAndName, "%s", (char*)"copy3");

    // prepend the filepath to the fileName in order to create the file in directory "copy1/fileName"
    sprintf(file1.filePathAndName, "%s%s", (char*)"copy1/", pMessage->fileName);
    sprintf(file2.filePathAndName, "%s%s", (char*)"copy2/", pMessage->fileName);
    sprintf(file3.filePathAndName, "%s%s", (char*)"copy3/", pMessage->fileName);

    file1.hasReadPermission = false;
    file2.hasReadPermission = false;
    file3.hasReadPermission = false;
    
    file1.isLocked          = false;
    file2.isLocked          = false;
    file3.isLocked          = false;


    //pthread_mutex_lock(&globalRecursiveLock);
    // ----------------------------------------------------- globalMap Start Critical Section ------------------------------------------------------------------
        H_RedundancyTryPushingFileLockToMap(&folder1, &file1);
        H_RedundancyTryPushingFileLockToMap(&folder2, &file2);
        H_RedundancyTryPushingFileLockToMap(&folder3, &file3);
    // ----------------------------------------------------- globalMap End Critical Section ------------------------------------------------------------------
    //pthread_mutex_unlock(&globalRecursiveLock);


    H_SetRedundancyGETFileExistenceAndReadPermission(&file1);
    H_SetRedundancyGETFileExistenceAndReadPermission(&file2);
    H_SetRedundancyGETFileExistenceAndReadPermission(&file3);

    // 1. If 2/3 folder don't exist, send response and exit function
    // 2. If 2/3 folders can't be opened, send response and exit function
    // 3. If 2/3 files don't exist, send response and exit function
    // 4. If 3/3 files are different from each other, send 500 error
    // 5. If 2/3 files don't have read permission, send response and exit function

    // 1.
    SendResponseBasedOnRedundancyFolderExistence(pMessage, &folder1, &folder2, &folder3);
        if(pMessage->hasSent) { return; }

    // 2.
    SendResponseBasedOnRedundancyFolderPermission(pMessage, &folder1, &folder2, &folder3);
        if(pMessage->hasSent) { return; }

    // 3.
    SendResponseBasedOnGETRedundancyFileExistence(pMessage, &file1, &file2, &file3);
        if(pMessage->hasSent) { return; }

    // 4.
    SendResponseBasedOnRedundancyGETFileDifference(pMessage, &file1, &file2, &file3);
        if(pMessage->hasSent) { return; }

    // 5.
    SendResponseBasedOnGETRedundancyFileReadPermission(pMessage, &file1, &file2, &file3);
        if(pMessage->hasSent) { return; } 


    LockExistingFiles(&file1, &file2, &file3);
    // ----------------------------------------------------- pFile1, pFile2, pFile3 Start Critical Section ------------------------------------------------------------------
        FinalizeHandleGetRFlag(pMessage, &file1, &file2, &file3);
    // ----------------------------------------------------- pFile1, pFile2, pFile3 End Critical Section ------------------------------------------------------------------
    UnlockExistingFiles(&file1, &file2, &file3);

}


void HandlePutRFlag(HttpObject* pMessage)
{

    FileHandler folder1;
    FileHandler folder2;
    FileHandler folder3;
    
    FileHandler file1;
    FileHandler file2;
    FileHandler file3;

    // set the folder name
    sprintf(folder1.filePathAndName, "%s", (char*)"copy1");
    sprintf(folder2.filePathAndName, "%s", (char*)"copy2");
    sprintf(folder3.filePathAndName, "%s", (char*)"copy3");

    // prepend the filepath to the fileName in order to create the file in directory "copy1/fileName"
    sprintf(file1.filePathAndName, "%s%s", (char*)"copy1/", pMessage->fileName);
    sprintf(file2.filePathAndName, "%s%s", (char*)"copy2/", pMessage->fileName);
    sprintf(file3.filePathAndName, "%s%s", (char*)"copy3/", pMessage->fileName);

    file1.hasWritePermission = true;
    file2.hasWritePermission = true;
    file3.hasWritePermission = true;

    file1.isLocked           = false;
    file2.isLocked           = false;
    file3.isLocked           = false;


    //pthread_mutex_lock(&globalRecursiveLock);
    // ----------------------------------------------------- globalMap Start Critical Section ------------------------------------------------------------------

        H_RedundancyTryPushingFileLockToMap(&folder1, &file1);
        H_RedundancyTryPushingFileLockToMap(&folder2, &file2);
        H_RedundancyTryPushingFileLockToMap(&folder3, &file3);

    // ----------------------------------------------------- globalMap End Critical Section ------------------------------------------------------------------
    //pthread_mutex_unlock(&globalRecursiveLock);

    // 1. check if the folder exists
    // 2. Check if we can get into the folder
    // 3. Check if the file exists 
    // 4. Check if we have write permission to the file
    //      H_SetRedundancyPUTFileWritePermission does step 3 and step 4

    H_SetRedundancyPUTFileExistenceAndWritePermission(&file1);
    H_SetRedundancyPUTFileExistenceAndWritePermission(&file2);
    H_SetRedundancyPUTFileExistenceAndWritePermission(&file3);

    // 1. If 2/3 folders don't exist, send response and exit function
    // 2. If 2/3 folders can't be opened, send response and exit function
    // 3. If 2/3 files don't have write permission, send response and exit function

    // 1.
    SendResponseBasedOnRedundancyFolderExistence(pMessage, &folder1, &folder2, &folder3);
        if(pMessage->hasSent) { return; }

    // 2.
    SendResponseBasedOnRedundancyFolderPermission(pMessage, &folder1, &folder2, &folder3);
        if(pMessage->hasSent) { return; }

    // 3.
    SendResponseBasedOnPUTRedundancyFileWritePermission(pMessage, &file1, &file2, &file3);
        if(pMessage->hasSent) { return; }



    LockExistingFiles(&file1, &file2, &file3);
    // ----------------------------------------------------- file1, file2, file3 Start Critical Section ------------------------------------------------------------------

        // 1. Attempt to open file. If file exists, we will truncate it, if not we create a new file
        int fd1 = open(file1.filePathAndName, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        int fd2 = open(file2.filePathAndName, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        int fd3 = open(file3.filePathAndName, O_CREAT | O_WRONLY | O_TRUNC, 0644);

        // if 1 of the three file descriptors is not write accessable, close it and work on the other two, DO NOT SEND AN ERROR
        if     (fd1 < 0) { close(fd1); }
        else if(fd2 < 0) { close(fd2); }
        else if(fd3 < 0) { close(fd3); }

        ssize_t bytesRead         = 0;
        ssize_t bytesWritten      = 0;
        ssize_t totalBytesWritten = 0;

        ssize_t tempBytesWritten1 = 0;
        ssize_t tempBytesWritten2 = 0;
        ssize_t tempBytesWritten3 = 0;

        // if the file is 0 bytes (empty)
        if (pMessage->contentLength == 0)
        {
            close(fd1);
            close(fd2);
            close(fd3);

            UnlockExistingFiles(&file1, &file2, &file3);
            
            // DON'T recv() or read(), you will get stuck there if you do!!!
            pMessage->statusCode = 201;
            sscanf("Created", "%s", pMessage->statusText);
            H_SetResponse(pMessage);
            return;
        }

        // this if statement should only be triggered when H_GetContentAfterExpect100() stores something in afterExpect100
        if(strlen(pMessage->afterExpect100) > 0)
        {
            // Because all of the writes will be the same, we only need to keep track of one of the bytesWritten
            // write the extra contents of afterExpect100 to file
            tempBytesWritten1 = write(fd1, pMessage->afterExpect100, strlen(pMessage->afterExpect100));
            tempBytesWritten2 = write(fd2, pMessage->afterExpect100, strlen(pMessage->afterExpect100));
            tempBytesWritten3 = write(fd3, pMessage->afterExpect100, strlen(pMessage->afterExpect100));
            
            // if we have closed a file descriptor above, than any one of these could be bad
            // we need to keep track of the bytes written without catching the closed file descriptor from lines 1768-1770
            if     (tempBytesWritten1 != -1) { bytesWritten = tempBytesWritten1; }
            else if(tempBytesWritten2 != -1) { bytesWritten = tempBytesWritten2; }
            else if(tempBytesWritten3 != -1) { bytesWritten = tempBytesWritten3; }  

            // not sure if this is the best error check
            if( (tempBytesWritten1 < 0) && (tempBytesWritten2 < 0) && (tempBytesWritten3 < 0) )
            {
                close(fd1);
                close(fd2);
                close(fd3);

                UnlockExistingFiles(&file1, &file2, &file3);

                WriteError( (char*)"Error: can't write to file %s", pMessage->fileName);
                H_SetStatusOnError(pMessage);
                H_SendResponse(pMessage);
                return;
            }
        }

        totalBytesWritten += bytesWritten;

        if(pMessage->contentLength > BUFFER_SIZE - 1)
        {
            // previous recv is eating everything which is why we get stuck here
            if( (bytesRead = read(pMessage->clientSocket, pMessage->buffer, BUFFER_SIZE-1)) < 0 ) // <- stuck here if given empty file
            {
                close(fd1);
                close(fd2);
                close(fd3);

                UnlockExistingFiles(&file1, &file2, &file3);

                WriteError( (char*)"Error: can't read from client socket %d\n", pMessage->clientSocket);

                H_SetStatusOnError(pMessage);
                H_SendResponse(pMessage);
                return;
            }
        }

        tempBytesWritten1 = write(fd1, pMessage->buffer, bytesRead);
        tempBytesWritten2 = write(fd2, pMessage->buffer, bytesRead);
        tempBytesWritten3 = write(fd3, pMessage->buffer, bytesRead);

        if     (tempBytesWritten1 != -1) { bytesWritten = tempBytesWritten1; }
        else if(tempBytesWritten2 != -1) { bytesWritten = tempBytesWritten2; }
        else if(tempBytesWritten3 != -1) { bytesWritten = tempBytesWritten3; }                              

        totalBytesWritten += bytesWritten;
        
        while(totalBytesWritten < pMessage->contentLength)
        {
            if(bytesWritten < 0 || bytesRead < 0)
            {
                close(fd1);
                close(fd2);
                close(fd3);

                UnlockExistingFiles(&file1, &file2, &file3);

                pMessage->statusCode = 500; 
                pMessage->contentLength = 0;
                H_SetStatusOnError(pMessage);
                H_SendResponse(pMessage);
                return;
            }
            else
            {
                // 5. Read any remaining bytes from socket, and write to file
                bytesRead         = read(pMessage->clientSocket, pMessage->buffer, BUFFER_SIZE-1);

                tempBytesWritten1 = write(fd1, pMessage->buffer, bytesRead);
                tempBytesWritten2 = write(fd2, pMessage->buffer, bytesRead);
                tempBytesWritten3 = write(fd3, pMessage->buffer, bytesRead);

                if     (tempBytesWritten1 != -1) { bytesWritten = tempBytesWritten1; }
                else if(tempBytesWritten2 != -1) { bytesWritten = tempBytesWritten2; }
                else if(tempBytesWritten3 != -1) { bytesWritten = tempBytesWritten3; }    

                totalBytesWritten += bytesWritten;   
            }
        }


        // We will never need to send "OK" as a response after a PUT request
        // So the default will be "Created" if there are no errors    
        close(fd1);
        close(fd2);
        close(fd3);

    // ----------------------------------------------------- file1, file2, file3 End Critical Section ------------------------------------------------------------------
    UnlockExistingFiles(&file1, &file2, &file3);

    pMessage->statusCode = 201;
    sscanf("Created", "%s", pMessage->statusText);
    H_SetResponse(pMessage);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                                                            End Redundancy Functions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void ReadCommandLine(int argc, char **argv, HttpObject *pMessage)
{
    int result = 0;

    if(argc == 1)
    {
        WriteErrorAndExit( (char*)"Usage: ./httpserver [address] [port]\n" );
    } 
    else if(argc > 2)
    {
        // check for given address which should be the third argument on the command line
        pMessage->port = argv[2];

        // checks if valid input was given for number of threads
        for (unsigned int i = 0; i < strlen(pMessage->port); i++)
        {
            // checks that only numbers were given
            if (pMessage->port[i] < '0' || pMessage->port[i] > '9')
            {
                WriteErrorAndExit( (char*)"Error: invalid character for port number\n");
            }
        }
        
    }

    while ( ( result = getopt(argc, argv, ":N:r") ) != -1 )
    {
        switch (result)
        {
            case 'N':
            {
                pMessage->NFlag = true;

                // checks if valid input was given for number of threads
                for (unsigned int i = 0; i < strlen(optarg); i++)
                {
                    // checks that only numbers were given
                    if (optarg[i] < '0' || optarg[i] > '9')
                    {
                        WriteErrorAndExit( (char*)"Error: invalid character for number of threads\n");
                    }
                }
                
                pMessage->numberOfThreads = atoi(optarg);

                // our program won't work if we give it -N 0, it must be at least -N 1.
                if (pMessage->numberOfThreads <= 0)
                {
                    WriteErrorAndExit( (char*)"Error: invalid number of threads\n");
                }

                break;
            }
            case 'r':
            {
                pMessage->rFlag = true;
                break;
            }
            case '?':
            {
                WriteError( (char*)"Error: unknown arguments\n");
                break;
            }
            case ':':
            {
                WriteErrorAndExit( (char*)"For '%s' no value was given\n", argv[optind-1] );
            }
            default:
            {
                WriteErrorAndExit( (char*)"Unknown Error\n" );
            }
        }
    }

}


// initialize worker threads (aka thread pool)
void Thread_Init(ThreadHandler *pThreadHandler, HttpObject *pMessage)
{
    ThreadHandler tempThreadHandler;

    // initialize the threads with values given on the command line
    for (int i = 0; i < pMessage->numberOfThreads; i++)
    {
        tempThreadHandler.threadID        = i;
        tempThreadHandler.message.NFlag   = pMessage->NFlag;
        tempThreadHandler.message.rFlag   = pMessage->rFlag;
        tempThreadHandler.numberOfThreads = pMessage->numberOfThreads;

        pThreadHandler[i] = tempThreadHandler;

        // pthread_create() returns 0 on success
        if ( pthread_create(&pThreadHandler[i].thread, NULL, HandleRequests, (void *)&pThreadHandler[i]) != 0 )
        {
            WriteError( (char*)"Error creating thread %d\n", pThreadHandler[i].threadID );

            // print why the thread could not be created
            if(errno == EAGAIN)     { WriteErrorAndExit( (char*)"Insufficient resources to create another thread\n"); }
            else if(errno == EINVAL){ WriteErrorAndExit( (char*)"Invalid settings in attr\n"); }
            else if(errno == EPERM) { WriteErrorAndExit( (char*)"No permission to set the scheduling policy and parameters specified in attr\n"); }
        }
    }

}


void *HandleRequests(void *pData)
{
    
    while (true)
    {
        ThreadHandler* pThreadHandler = (ThreadHandler*)pData;

        // clear out the old buffers
        memset(pThreadHandler->message.buffer, 0, sizeof(pThreadHandler->message.buffer));
        memset(pThreadHandler->message.afterExpect100, 0, sizeof(pThreadHandler->message.afterExpect100));
        pThreadHandler->message.clientSocket = 0;
        pThreadHandler->message.hasSent      = false;

        pthread_mutex_lock(&globalRecursiveLock);
        // ----------------------------------------------------- globalRequestQueue Start Critical Section ------------------------------------------------------------------

            // if the request queue is empty, we will sleep the threads
            while( globalRequestQueue->empty() )
            {
                pthread_cond_wait(&globalConditionVariable, &globalRecursiveLock);
            }
            
            pThreadHandler->message.clientSocket = globalRequestQueue->front();
            globalRequestQueue->pop();
        
        // ----------------------------------------------------- globalRequestQueue End Critical Section ------------------------------------------------------------------
        pthread_mutex_unlock(&globalRecursiveLock);


        printf("Worker thread:\t[%d]\tHandling socket: %d\n", pThreadHandler->threadID, pThreadHandler->message.clientSocket);

        ReadHTTPRequest(&pThreadHandler->message);
        
        ProcessHTTPRequest(&pThreadHandler->message);
        
        ConstructHTTPResponse(&pThreadHandler->message);
        
        SendHTTPResponse(&pThreadHandler->message);
        
        close(pThreadHandler->message.clientSocket);

        printf("Worker thread [%d] closed socket %d\n\n\n", pThreadHandler->threadID, pThreadHandler->message.clientSocket);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                                                           HTTP METHODS FROM ASGN1 BELOW
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//   H_GetAddress returns the numerical representation of the address
//   identified by *name* as required for an IPv4 address represented
//   in a struct sockaddr_in.
unsigned long H_GetAddress(char *name)
{
    // Author of H_GetAddress(): Daniel Santos Ferreira Alves
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

// used only for a GET request
void H_CheckGETReadAccess(HttpObject* pMessage)
{
    // if the file exists
    if(H_CheckFileExists(pMessage))
    {
        // GET = 2
        if(H_CheckMethod(pMessage) == 2)
        {

            struct stat statbuf;

            // check if the file exists
            if(stat(pMessage->fileName, &statbuf) == 0)
            {
                pMessage->doesExist = true;

                // check read permission
                if( !(statbuf.st_mode & S_IROTH))
                {
                    pMessage->hasReadPermission = false;
                    return;
                } 
            }
            else
            {
                pMessage->doesExist = false;
                return;
            }
            
            pMessage->hasReadPermission = true;
        }
    }
}


int H_CheckMethod(HttpObject *pMessage)
{
    if (strcmp(pMessage->method, "PUT") == 0)
    {
        return 1;
    }
    else if (strcmp(pMessage->method, "GET") == 0)
    {
        return 2;
    }

    return 0;
}

void H_CallMethod(HttpObject *pMessage)
{

    if(pMessage->rFlag)
    {
        if (strcmp(pMessage->method, "PUT") == 0)
        {
            HandlePutRFlag(pMessage);
        }
        else if (strcmp(pMessage->method, "GET") == 0)
        {
            HandleGETRFlag(pMessage);
        }
        else 
        {
            // If the method does not match PUT, GET, then the request is a "Bad Request"
            pMessage->statusCode = 400;
            pMessage->contentLength = 0;
            sscanf("Bad Request", "%[^\t\n]", pMessage->statusText);
            H_SendResponse(pMessage);
            return;
        }
    }

    else if(!pMessage->rFlag)
    {
        if (strcmp(pMessage->method, "PUT") == 0)
        {
            // If a PUT request, check if the lock is in map, if not, initialize one and insert into map
            // pthread_mutex_t fileLock;
            
            // pthread_mutex_lock(&globalRecursiveLock);
            // // ----------------------------------------------------- globalMap Start Critical Section ------------------------------------------------------------------                
            //     if(!H_IsFilenameInMap(pMessage))
            //     {
            //         pthread_mutex_init(&fileLock, NULL);
            //         globalMap.insert(std::pair<std::string, pthread_mutex_t>(pMessage->fileName, fileLock));
            //     }

            // // ----------------------------------------------------- globalMap End Critical Section ------------------------------------------------------------------
            // pthread_mutex_unlock(&globalRecursiveLock);

            if (H_IsFilenameInMap(pMessage))
            {
                pthread_mutex_lock(&globalMap[pMessage->fileName]);
            }
            //pthread_mutex_lock(&globalMap[pMessage->fileName]);
            // ----------------------------------------------------- pMessage->fileName Start Critical Section ------------------------------------------------------------------                
                HandlePUT(pMessage);
            // ----------------------------------------------------- pMessage->fileName End Critical Section ------------------------------------------------------------------                
            if(H_IsFilenameInMap(pMessage))
            {
                pthread_mutex_unlock(&globalMap[pMessage->fileName]);
            }
            
        }

        else if (strcmp(pMessage->method, "GET") == 0)
        {
            // If a GET request, check if the lock is in map, if not, initialize one and insert into map
            // pthread_mutex_t fileLock;
            
            // pthread_mutex_lock(&globalRecursiveLock);
            // // ----------------------------------------------------- globalMap Start Critical Section ------------------------------------------------------------------                
            //     if(!H_IsFilenameInMap(pMessage))
            //     {
            //         pthread_mutex_init(&fileLock, NULL);
            //         globalMap.insert(std::pair<std::string, pthread_mutex_t>(pMessage->fileName, fileLock));
            //     }
            // // ----------------------------------------------------- globalMap End Critical Section ------------------------------------------------------------------
            // pthread_mutex_unlock(&globalRecursiveLock);


            if(H_IsFilenameInMap(pMessage))
            {
                pthread_mutex_lock(&globalMap[pMessage->fileName]);
            }
            //pthread_mutex_lock(&globalMap[pMessage->fileName]);
            // ----------------------------------------------------- pMessage->fileName Start Critical Section ------------------------------------------------------------------                
                HandleGET(pMessage);
            // ----------------------------------------------------- pMessage->fileName End Critical Section ------------------------------------------------------------------                
            //pthread_mutex_unlock(&globalMap[pMessage->fileName]);
            if(H_IsFilenameInMap(pMessage))
            {
                pthread_mutex_unlock(&globalMap[pMessage->fileName]);
            }
        
        }
        else 
        {
            // If the method does not match PUT, GET, then the request is a "Bad Request"
            pMessage->statusCode = 400;
            pMessage->contentLength = 0;
            sscanf("Bad Request", "%[^\t\n]", pMessage->statusText);
            H_SendResponse(pMessage);
            return;
        }
    }

}

int H_CheckFileName(HttpObject *pMessage)
{
    for (unsigned int i = 0; i < strlen(pMessage->fileName); i++)
    {
        // check if characters are NOT in alphabet, digits 0-9, dash, or underscore
        if (!((pMessage->fileName[i] >= 'A' && pMessage->fileName[i] <= 'Z') || (pMessage->fileName[i] >= 'a' && pMessage->fileName[i] <= 'z') ||
              (pMessage->fileName[i] >= '0' && pMessage->fileName[i] <= '9') || (pMessage->fileName[i] == '_') || (pMessage->fileName[i] == '-')))
        {
            // invalid character
            return 400;
        }
    }

    return 0;
}

void H_CheckHTTPVersion(HttpObject *pMessage)
{
    if (strcmp(pMessage->httpVersion, "HTTP/1.1") != 0)
    {
        pMessage->statusCode = 400;
        pMessage->contentLength = 0;
        sscanf("Bad Request", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
        return;
    }
}


void H_StoreContentLength(HttpObject *pMessage, char requestBuffer[])
{
    char tempBuffer[BUFFER_SIZE + 1];
    strcpy(tempBuffer, requestBuffer);
    tempBuffer[BUFFER_SIZE] = 0;

    // store content length from client request
    char *ptrSave = NULL;
    char *token = strtok_r(tempBuffer, "\r\n", &ptrSave);

    while (token != NULL)
    {
        char *pRet = strstr(token, "Content-Length: ");

        // if we found Content-Length in the string
        if (pRet != NULL)
        {
            sscanf(pRet, "%*s %d", &(pMessage->contentLength));
        }

        token = strtok_r(NULL, "\r\n", &ptrSave);
    }

}


// Fixes the problem of curl storing the contents of a file into the request buffer when making concurrent PUT requests. 
//  Check if the buffer ends with "Expect: 100-continue\r\n\r\n"
//  if it does, store all the information after that into a new buffer
//  add the counted bytes to totalBytesWritten in HandlePUT()
void H_GetContentAfterExpect100(HttpObject* pMessage, char requestBuffer[])
{
    const char* expectStr = "Expect: 100-continue\r\n\r\n";
    std::string tempStr(requestBuffer);
    std::string finalStr = tempStr.substr(tempStr.find(expectStr) + strlen(expectStr)); 
    strcpy( pMessage->afterExpect100, finalStr.c_str() );
}


void H_SetStatusOnError(HttpObject *pMessage)
{
    switch (errno)
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

void H_ReadContentLength(HttpObject *pMessage)
{

    struct stat statbuf;

    // if the file exists
    if (stat(pMessage->fileName, &statbuf) == 0) 
    {
        // file exists but is empty
        pMessage->contentLength = statbuf.st_size;
        pMessage->statusCode = 200;
        sscanf("OK", "%s", pMessage->statusText);
    }
    else // if file does not exist
    {
        pMessage->statusCode = 404;
        pMessage->contentLength = 0;
        sscanf("Not Found", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
        return;
    }

    H_SetResponse(pMessage);
    
}

bool H_CheckFileExists(HttpObject *pMessage)
{
    struct stat buffer;
    return (stat(pMessage->fileName, &buffer) == 0);
}

void H_SetResponse(HttpObject *pMessage)
{   
    sprintf(pMessage->response, "%s %d %s\r\nContent-Length: %d\r\n\r\n", pMessage->httpVersion, pMessage->statusCode, pMessage->statusText, pMessage->contentLength);
}

// if there is an error, we need to send the response early
void H_SendResponse(HttpObject* pMessage)
{
    if (!pMessage->hasSent)
    {
        H_SetResponse(pMessage);

        if( send(pMessage->clientSocket, pMessage->response, strlen(pMessage->response), 0) < 0 )
        {
            pMessage->contentLength = 0;
            H_SetStatusOnError(pMessage);
            return;
        }

        pMessage->hasSent = true;

        if (write(STDOUT_FILENO, pMessage->response, strlen(pMessage->response)) < 0)
        {
            pMessage->contentLength = 0;
            H_SetStatusOnError(pMessage);
            return;
        }
    }

}

void AcceptConnection(HttpObject* pMessage, struct sockaddr* client_addr, socklen_t* client_addrlen, int server_sockd)
{
    char waiting[] = "[+] server is waiting...\n";
    write(STDOUT_FILENO, waiting, strlen(waiting));

    // 1. Accept connection
    pMessage->clientSocket = accept( server_sockd, client_addr, client_addrlen ); 

    if(pMessage->clientSocket <= 0)
    {
        WriteError((char*) "Error: accept() failed\n");
        pMessage->contentLength = 0;
        pMessage->hasSent = true;
        return;
    }

}

// This function takes care of reading the message
void ReadHTTPRequest(HttpObject* pMessage)
{

    if (!pMessage->hasSent)
    {
        ssize_t bytesRead = 0;
        char    requestBuffer[BUFFER_SIZE];
        char    tempFileName[BUFFER_SIZE];

        // gets the clients HTTP request
        //  changing out pMessage->buffer to requestBuffer to make sure that pMessage->buffer does not contain any uneccessary information 
        if( (bytesRead = read( pMessage->clientSocket, requestBuffer, BUFFER_SIZE-1)) < 0 )
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

        H_GetContentAfterExpect100(pMessage, requestBuffer);

        // store client request into method, filename, httpversion
        char* ptr = requestBuffer;
        sscanf(ptr, "%s %s %s", pMessage->method, tempFileName, pMessage->httpVersion);

        // gets rid of the '/' in the fileName
        if(tempFileName[0] == '/')
        {
            memmove( tempFileName, tempFileName+1, strlen(tempFileName) );
        }

        // check if fileName is 10 and only 10 ascii characters long, else fail the request
        if (strlen(tempFileName) == 10)
        {   
            strcpy(pMessage->fileName, tempFileName);
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
            H_StoreContentLength(pMessage, requestBuffer);
        }

        // null terminate
        requestBuffer[bytesRead] = 0;

    }

}


// 3. Process Request
void ProcessHTTPRequest(HttpObject* pMessage)
{
    if (!pMessage->hasSent)
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
        if( H_CheckFileName(pMessage) != 0 )
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
        if( (H_CheckMethod(pMessage) == 2) && !pMessage->rFlag )
        {
            H_ReadContentLength(pMessage);
        }
    }

}


// 4. Construct Response
void ConstructHTTPResponse(HttpObject* pMessage)
{  
    if(!pMessage->hasSent)
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
        if(!pMessage->rFlag)
        {
            H_CheckGETReadAccess(pMessage);

            if(!pMessage->hasReadPermission)
            {
                // Read Permission denied
                pMessage->statusCode = 403;
                pMessage->contentLength = 0;
                sscanf("Forbidden", "%s", pMessage->statusText);
                H_SendResponse(pMessage);
                return;                
            }
        }

        H_SetResponse(pMessage);
    }

}

// 5. Send Response
void SendHTTPResponse(HttpObject* pMessage)
{
    if(!pMessage->hasSent)
    {
        if(H_CheckMethod(pMessage) != 1 && !pMessage->rFlag)
        {
            H_ReadContentLength(pMessage);
        }

        // Since we want to send a message to the console using write(), the message has to be formatted in a way like this
        // response only needs around 50 bytes but I just want to make sure our program doesn't crash due lack of space issues
        char response[200]; 
        sprintf(response, "[+] received %d bytes from client\n[+] response:\n", pMessage->contentLength);
        
        if( write(STDOUT_FILENO, response, strlen(response)) < 0)
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

void HandleGET(HttpObject* pMessage)
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
        H_SendResponse(pMessage); // this is supposed to be here

        // 4. Read from buffer
        ssize_t bytesRead = read(fd, pMessage->buffer, BUFFER_SIZE-1);

        if(bytesRead == -1)
        {
            close(fd);

            char writeError[60];
            sprintf(writeError, "Error: can't read from file %s\n", pMessage->fileName);
            write(STDERR_FILENO, writeError, strlen(writeError));
            
            H_SetStatusOnError(pMessage);
            H_SendResponse(pMessage);
            return;
        }

        ssize_t totalBytesWritten = 0;
        ssize_t bytesWritten      = 0;

        while(totalBytesWritten < pMessage->contentLength)
        {
            if(bytesRead == -1)
            {
                close(fd);
                H_SetStatusOnError(pMessage);
                H_SendResponse(pMessage);
                return;
            }
            else
            {
                // 5. Read any remaining bytes from socket, and write to file
                bytesWritten      = write(pMessage->clientSocket, pMessage->buffer, bytesRead);
                bytesRead         = read(fd, pMessage->buffer, BUFFER_SIZE-1);
                totalBytesWritten += bytesWritten;
            }
        }    
    }

    close(fd);
    H_ReadContentLength(pMessage);
}


// HandlePUT copies a file from client to server
void HandlePUT(HttpObject* pMessage)
{

    // 1. Attempt to open file. If file exists, we will rewrite, if not we create a new file
    int fd = open(pMessage->fileName, O_CREAT | O_RDWR | O_TRUNC, 0644);

    //  Check for errors (permissions, file doesn't exit, etc)
    if( fd < 0 )
    {
        close(fd);

        pMessage->statusCode = 403;
        pMessage->contentLength = 0;
        sscanf("Forbidden", "%s", pMessage->statusText);
        H_SendResponse(pMessage);
        return;
    }
    else
    {   
        ssize_t bytesRead         = 0;
        ssize_t bytesWritten      = 0;
        ssize_t totalBytesWritten = 0;

         // if the file is 0 bytes (empty)
        if (pMessage->contentLength == 0)
        {
            close(fd);
            
            // DON'T recv(), you will get stuck there if you do!!!
            pMessage->statusCode = 201;
            sscanf("Created", "%s", pMessage->statusText);
            H_SetResponse(pMessage);
            return;
        }

        // this if statement should only be triggered when H_GetContentAfterExpect100() stores something in afterExpect100
        if(strlen(pMessage->afterExpect100) > 0)
        {
            // write the extra contents to the file
            if( (bytesWritten = write(fd, pMessage->afterExpect100, strlen(pMessage->afterExpect100))) < 0 )
            {
                WriteError( (char*)"Error: can't write to file %s", pMessage->fileName);
                H_SetStatusOnError(pMessage);
                H_SendResponse(pMessage);
                return;
            }
        }

        totalBytesWritten += bytesWritten;

        // 4. Read from buffer
        if(pMessage->contentLength > BUFFER_SIZE-1)
        {
            // previous recv is eating everything which is why we get stuck here
            if( (bytesRead = read(pMessage->clientSocket, pMessage->buffer, BUFFER_SIZE-1)) < 0 ) // <- stuck here if given empty file
            {
                close(fd);
                WriteError( (char*)"Error: can't read from client socket %d\n", pMessage->clientSocket );
                H_SetStatusOnError(pMessage);
                H_SendResponse(pMessage);
                return;
            }
        }

        bytesWritten      = write(fd, pMessage->buffer, bytesRead);
        totalBytesWritten += bytesWritten;

        while(totalBytesWritten < pMessage->contentLength)
        {
            if(bytesWritten < 0 || bytesRead < 0)
            {
                close(fd);
                pMessage->statusCode = 500; 
                pMessage->contentLength = 0;
                H_SetStatusOnError(pMessage);
                H_SendResponse(pMessage);
                return;
            }
            else
            {
                // 5. Read any remaining bytes from socket, and write to file
                // very last read tries to get more content than what is in file
                bytesRead         = read(pMessage->clientSocket, pMessage->buffer, BUFFER_SIZE-1); // <- stuck here!!!
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