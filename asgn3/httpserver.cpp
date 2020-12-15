//---------------------------------------------------------------------------------
// Authors:   Rory Glenn, Jeremiah Chen
// Email:     romglenn@ucsc.edu, jchen202@ucsc.edu
// Quarter:   Fall 2020
// Class:     CSE 130
// Project:   asgn3: Back-up and Recovery in the HTTP server
// Professor: Faisal Nawab
//
// httpserver.cpp
// Implementaion file for httpserver
//---------------------------------------------------------------------------------

#include "httpserver.h"


int main(int argc, char** argv) 
{
    int                 enable          = 1;
    int                 serverSocket;
    char*               address         = argv[1];
    const char*         port            = (const char*)"80";
    socklen_t           clientAddressLength;
    struct sockaddr_in  serverAddress;
    struct sockaddr     clientAddress;

    if(argc > 2) { port = argv[2]; }
    else if(argc == 1 || argc > 3) { WriteErrorAndExit( (char*)"Usage: httpserver [address] [port]\n"); }
    
    memset( &serverAddress, 0, sizeof(serverAddress) );

    serverAddress.sin_family      = AF_INET;
    serverAddress.sin_port        = htons(atoi(port));
    serverAddress.sin_addr.s_addr = H_GetAddress(address);

    if( (serverSocket = socket(AF_INET, SOCK_STREAM, 0) ) <= 0 ) { WriteErrorAndExit( (char*)"Error: socket()\n"); }

    // This allows you to avoid: 'Bind: Address Already in Use' error
    if( setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0 ) { WriteErrorAndExit( (char*)"Error: setsockopt()\n"); }

    // Bind/attach server address to socket that is open
    if( bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0 ) { WriteErrorAndExit( (char*)"Error: bind()\n"); }

    // Listen for incoming connections
    if( listen(serverSocket, 500) < 0 ) { WriteErrorAndExit( (char*)"Error: listen()\n"); }


    while(true)
    {
        HttpObject message;
        message.hasSentResponse = false;
        
        AcceptConnection( &message, (struct sockaddr*)&clientAddress, (socklen_t*)&clientAddressLength, serverSocket );
        ReadHTTPRequest(&message);
        ProcessHTTPRequest(&message);
        ConstructHTTPResponse(&message);
        SendHTTPResponse(&message);

        memset( &serverAddress, 0, sizeof(serverAddress) );
        close(message.clientSocket);
    }

    return 0;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                      NEW ASGN 3 FUNCTIONS 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// returns the number of seconds since 01-Jan-1970
std::string H_GetTimeStamp()
{
    std::time_t t = std::time(0);
    std::string time = std::to_string(t);
    return time;
}

// if the file we are trying to copy is zero bytes, don't read() from it
// just create a new file using open() and move on
bool H_CreateZeroByteFile(const char* fileName, const char* filePath)
{
    int fdBackupFile = 0;

    // get the content length of the file
    struct stat statbuf;
    stat(fileName, &statbuf);

    // don't read from the file if it is empty
    if(statbuf.st_size == 0)
    {

        std::string filePathAndName = filePath;
        filePathAndName             += "/";
        filePathAndName             += fileName;

        //printf("copying file %s to path: %s\n", fileName, filePath);

        // create the new backup file
        fdBackupFile = open(filePathAndName.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
        close(fdBackupFile);
        return true;
    }
    return false;
}



bool H_CreateZeroByteFile(const char* path)
{
    int fdBackupFile = 0;

    // get the content length of the file
    struct stat statbuf;
    stat(path, &statbuf);

    // don't read from the file if it is empty
    if(statbuf.st_size == 0)
    {
        //printf("copying files to path: %s\n", path);

        // create the new backup file
        fdBackupFile = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
        close(fdBackupFile);
        return true;
    }
    return false;
}



bool H_IsFile(const char* filename)
{
    DIR* directory;

    if( (directory = opendir(filename)) == NULL)
    {
        closedir(directory);
        return true;
    }

    return false;

}


bool H_IsFolder(const char* filename)
{
    DIR* directory;

    if( (directory = opendir(filename)) == NULL)
    {
        closedir(directory);
        return false;
    }

    return true;
}


                                                        /////////// Backup Functions ///////////

void Backup(HttpObject* pMessage)
{
    // Get all the file names in the folder
    Backup_ScanExistingFiles(pMessage);

    // Is it possible that this folder already exists? If so, do we truncate it, check for permissions, quit?
    std::string path = "./backup-" + H_GetTimeStamp(); 
    
    // try to create the directory
    if(!mkdir(path.c_str(), 0777))
    {
        pMessage->vecBackupFolder.push_back(path);
        printf("%s created\n", path.c_str());

        Backup_CopyFiles(pMessage);
        
        // reset vectors to empty
        pMessage->vecBackupFiles.clear();
        pMessage->vecBackupFolder.clear();

        // send response after we are finished
        pMessage->statusCode = 200;
        pMessage->contentLength = 0;
        sscanf("OK", "%s", pMessage->statusText);
        H_SendResponse(pMessage);
    }
    else
    {
        WriteError( (char*)"Error: directory %s not created\n", (char*)path.c_str() );

        pMessage->statusCode = 500;
        pMessage->contentLength = 0;
        sscanf("Internal Server Error", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
    }

}



void Backup_ScanExistingFiles(HttpObject* pMessage)
{
    DIR* directory;
    struct dirent* entry;

    if( (directory = opendir(".")) != NULL)
    {
        while( (entry = readdir(directory)) != NULL)
        {
            if(H_IsFile(entry->d_name))
            {
                // don't add the two files "." and ".."
                if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                {

                    // check for permissions
                    pMessage->vecBackupFiles.push_back(entry->d_name);
                }
            }
        }
    }
    else
    {
        printf("Error: could not open directory\n");
    }

    closedir(directory);
}




bool Recovery_GetMostRecentBackup(HttpObject* pMessage)
{
    std::string mostRecentBackup = "";
    std::vector<std::string> tempBackups;

    // convert all the backup names to an int
    for (unsigned long i = 0; i < pMessage->vecBackups.size(); i++)
    {
        // chop the string after "backup-"
        std::string backupNumber = pMessage->vecBackups[i].substr(7, pMessage->vecBackups[i].size() );
        tempBackups.push_back( backupNumber );
    }

    if(!tempBackups.empty())
    {
        auto maxElement = std::max_element(std::begin(tempBackups), std::end(tempBackups));
        pMessage->recoveryFolder = "./backup-" + *maxElement;
        return true;
    }

    return false;

}



// Copy all the files in the server directory into the new ./backup-[] folder 
void Backup_CopyFiles(HttpObject* pMessage)
{

    size_t ERROR = -1;

    // start from the top of the filename vector and copy each file one by one
    for (unsigned long i = 0; i < pMessage->vecBackupFiles.size(); i++)
    {
        char backupBuffer[BUFFER_SIZE];
        int fdServerFile            = 0;
        int fdBackupFile            = 0;
        size_t bytesRead            = 0;
        size_t bytesWritten         = 0;
        size_t totalBytesWritten    = 0;
        std::string fileNameandPath = pMessage->vecBackupFolder.back() + "/" + pMessage->vecBackupFiles[i];
        struct stat statbuf;
    
        stat(pMessage->vecBackupFiles[i].c_str(), &statbuf);

        // if the file is zero bytes, create it
        if(H_CreateZeroByteFile(pMessage->vecBackupFiles[i].c_str(), pMessage->vecBackupFolder.back().c_str())) { continue; }
  
        // open the file in server folder
        if ( (fdServerFile = open(pMessage->vecBackupFiles[i].c_str(), O_RDONLY)) != -1 )
        {
            // read from the file
            if( (bytesRead = read(fdServerFile, backupBuffer, BUFFER_SIZE-1)) != ERROR)
            {
                // create the new backup file
                fdBackupFile = open(fileNameandPath.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);

                if(( bytesWritten = write(fdBackupFile, backupBuffer, bytesRead)) != ERROR)
                {
                    totalBytesWritten = bytesWritten;

                    while (totalBytesWritten < (unsigned long)statbuf.st_size)
                    {
                        bytesRead          = read(fdServerFile, backupBuffer, BUFFER_SIZE-1);
                        bytesWritten       = write(fdBackupFile, backupBuffer, bytesRead);
                        totalBytesWritten += bytesWritten;

                        if(bytesRead == ERROR || bytesWritten == ERROR)
                        {
                            close(fdBackupFile);
                            close(fdServerFile);
                            continue;
                        }

                    }

                    // we are finished reading and writing
                    close(fdBackupFile);
                    close(fdServerFile);                    
                }
                else
                {
                    close(fdBackupFile);
                    close(fdServerFile);
                    continue;
                }
                
            }
            else
            {
                printf("Error: can't read from file %s\n", pMessage->vecBackupFiles[i].c_str());
                close(fdServerFile);
                continue;
            }
            
        }
        else
        {
            // if we can't read from the file, continue to the next file
            close(fdServerFile);
            continue;
        }

    }
    
}


                                                            /////////// Recovery Functions ///////////


// the ability to recover to an earlier backup.
void Recovery(HttpObject* pMessage)
{
    Recovery_ScanBackupDirectories(pMessage);

    if(Recovery_GetMostRecentBackup(pMessage))
    {
        std::cout << "most recent backup: " << pMessage->recoveryFolder << std::endl;
        Recovery_ScanExistingFiles(pMessage);
        Recovery_CopyFiles(pMessage);

        // send response after we are finished
        pMessage->statusCode = 200;
        pMessage->contentLength = 0;
        sscanf("OK", "%s", pMessage->statusText);
        H_SendResponse(pMessage);
    }
    else
    {
        // backup does not exist
        pMessage->statusCode = 404;
        pMessage->contentLength = 0;
        sscanf("Not Found", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
    }

}


// Copy all the files in the server directory into the new ./backup-[] folder 
void Recovery_CopyFiles(HttpObject* pMessage)
{
    // start from the top of the filename vector and copy each file one by one
    for (unsigned long i = 0; i < pMessage->vecRecoveryFiles.size(); i++)
    {
        int fdRecoveryFile           = 0;
        int fdBackupFile             = 0;
        size_t bytesRead             = 0;
        size_t bytesWritten          = 0;
        size_t totalBytesWritten     = 0;
        size_t ERROR                 = -1;
        std::string recoveryFilePath = pMessage->recoveryFolder + "/" + pMessage->vecRecoveryFiles[i];
        char backupBuffer[BUFFER_SIZE];
        struct stat statbuf;
    
        stat(recoveryFilePath.c_str(), &statbuf);

        // if the file is zero bytes, create it
        if(H_CreateZeroByteFile(recoveryFilePath.c_str())) { continue; }
  
        // open the file in server folder
        if ( (fdRecoveryFile = open(recoveryFilePath.c_str(), O_RDONLY)) != -1 )
        {
            // read from the file
            if( (bytesRead = read(fdRecoveryFile, backupBuffer, BUFFER_SIZE-1)) != ERROR)
            {
                // create the new backup file
                fdBackupFile = open(pMessage->vecRecoveryFiles[i].c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);

                if(( bytesWritten = write(fdBackupFile, backupBuffer, bytesRead)) != ERROR)
                {
                    totalBytesWritten = bytesWritten;

                    while (totalBytesWritten < (unsigned long)statbuf.st_size)
                    {
                        bytesRead          = read(fdRecoveryFile, backupBuffer, BUFFER_SIZE-1);
                        bytesWritten       = write(fdBackupFile, backupBuffer, bytesRead);
                        totalBytesWritten += bytesWritten;

                        if(bytesRead == ERROR || bytesWritten == ERROR)
                        {
                            close(fdBackupFile);
                            close(fdRecoveryFile);
                            continue;
                        }

                    }

                    // we are finished reading and writing
                    close(fdBackupFile);
                    close(fdRecoveryFile);                    
                }
                else
                {
                    close(fdBackupFile);
                    close(fdRecoveryFile);
                    continue;
                }
                
            }
            else
            {
                printf("Error: can't read from file %s\n", pMessage->vecRecoveryFiles[i].c_str());
                close(fdRecoveryFile);
                continue;
            }
            
        }
        else
        {
            // if we can't read from the file, continue to the next file
            close(fdRecoveryFile);
            continue;
        }

    }
    
}



void Recovery_ScanExistingFiles(HttpObject* pMessage)
{
    DIR* directory;
    struct dirent* entry;

    if( (directory = opendir(pMessage->recoveryFolder.c_str())) != NULL)
    {
        while( (entry = readdir(directory)) != NULL)
        {
            if(H_IsFile(entry->d_name))
            {
                // don't add the two files "." and ".."
                if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                {
                    pMessage->vecRecoveryFiles.push_back(entry->d_name);
                }
            }
        }
    }
    else
    {
        printf("Error: could not open directory\n");
    }

    closedir(directory);
}


void Recovery_ScanBackupDirectories(HttpObject* pMessage)
{
    DIR* directory;
    struct dirent* entry;

    if( (directory = opendir(".")) != NULL)
    {
        while( (entry = readdir(directory)) != NULL)
        {
            if(H_IsFolder(entry->d_name))
            {
                // don't add the two files "." and ".."
                if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                {
                    // make sure that it is a backup folder by checking the name
                    std::string temp = entry->d_name;
                    if( temp.substr(0, 7) == "backup-" )
                    {
                        pMessage->vecBackups.push_back(entry->d_name);
                    }

                }
            }
        }
    }
    else
    {
        printf("Error: could not open directory\n");
    }

    closedir(directory);
}



                                                        ///////////// Recovery Special ////////////////

// the ability to recover a specific backup
void RecoverySpecial(HttpObject* pMessage)
{
    // 1. get all the backup folder names and store them in the vector "vecBackups"
    // 2. search for specific backup folder
    // 3. once found, put the file names in the folder into a vector
    // 4. now copy all the contents from the backup to the server folder

    Recovery_ScanBackupDirectories(pMessage); // 1

    // 2
    if(std::find(pMessage->vecBackups.begin(), pMessage->vecBackups.end(), pMessage->recoverySpecialFolder.c_str()) != pMessage->vecBackups.end())
    {
        printf("Restoring: %s\n", pMessage->recoverySpecialFolder.c_str());
        RecoverySpecial_ScanExistingFiles(pMessage); // 3
        RecoverySpecial_CopyFiles(pMessage);         // 4

        // send response after we are finished
        pMessage->statusCode = 200;
        pMessage->contentLength = 0;
        sscanf("OK", "%s", pMessage->statusText);
        H_SendResponse(pMessage);
    }
    else
    {
        // backup does not exist
        pMessage->statusCode = 404;
        pMessage->contentLength = 0;
        sscanf("Not Found", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
    }
    
}



// Copy all the files in the server directory into the new ./backup-[] folder 
void RecoverySpecial_CopyFiles(HttpObject* pMessage)
{
    // start from the top of the filename vector and copy each file one by one
    for (unsigned long i = 0; i < pMessage->vecRecoverySpecialFiles.size(); i++)
    {
        int fdRecoveryFile           = 0;
        int fdBackupFile             = 0;
        size_t bytesRead             = 0;
        size_t bytesWritten          = 0;
        size_t totalBytesWritten     = 0;
        size_t ERROR                 = -1;
        std::string recoveryFilePath = pMessage->recoverySpecialFolder + "/" + pMessage->vecRecoverySpecialFiles[i];
        char backupBuffer[BUFFER_SIZE];
        struct stat statbuf;
    
        stat(recoveryFilePath.c_str(), &statbuf);

        // if the file is zero bytes, create it
        if(H_CreateZeroByteFile(recoveryFilePath.c_str())) { continue; }
  
        // open the file in server folder
        if ( (fdRecoveryFile = open(recoveryFilePath.c_str(), O_RDONLY)) != -1 )
        {
            // read from the file
            if( (bytesRead = read(fdRecoveryFile, backupBuffer, BUFFER_SIZE-1)) != ERROR)
            {
                // create the new backup file
                fdBackupFile = open(pMessage->vecRecoverySpecialFiles[i].c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);

                if(( bytesWritten = write(fdBackupFile, backupBuffer, bytesRead)) != ERROR)
                {
                    totalBytesWritten = bytesWritten;

                    while (totalBytesWritten < (unsigned long)statbuf.st_size)
                    {
                        bytesRead          = read(fdRecoveryFile, backupBuffer, BUFFER_SIZE-1);
                        bytesWritten       = write(fdBackupFile, backupBuffer, bytesRead);
                        totalBytesWritten += bytesWritten;

                        if(bytesRead == ERROR || bytesWritten == ERROR)
                        {
                            close(fdBackupFile);
                            close(fdRecoveryFile);
                            continue;
                        }

                    }

                    // we are finished reading and writing
                    close(fdBackupFile);
                    close(fdRecoveryFile);                    
                }
                else
                {
                    close(fdBackupFile);
                    close(fdRecoveryFile);
                    continue;
                }
                
            }
            else
            {
                printf("Error: can't read from file %s\n", pMessage->vecRecoverySpecialFiles[i].c_str());
                close(fdRecoveryFile);
                continue;
            }
            
        }
        else
        {
            // if we can't read from the file, continue to the next file
            close(fdRecoveryFile);
            continue;
        }

    }
    
}





void RecoverySpecial_ScanExistingFiles(HttpObject* pMessage)
{
    DIR* directory;
    struct dirent* entry;

    if( (directory = opendir(pMessage->recoverySpecialFolder.c_str())) != NULL)
    {
        while( (entry = readdir(directory)) != NULL)
        {
            if(H_IsFile(entry->d_name))
            {
                // don't add the two files "." and ".."
                if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                {
                    pMessage->vecRecoverySpecialFiles.push_back(entry->d_name);
                }
            }
        }
    }
    else
    {
        printf("Error: could not open directory\n");
    }

    closedir(directory);
}



                                                            ///////////// List Functions ////////////////

void List(HttpObject* pMessage)
{
    // scan the backup directories
    std::vector<std::string> vecDirectories = List_GetExistingDirectories();
    std::string directoryList = "";
    pMessage->contentLength = 0;


    if(!vecDirectories.empty())
    {
        // store all directory names in one string with a newline after each one
        std::cout << "\n";
        for (unsigned long i = 0; i < vecDirectories.size(); i++)
        {
            std::cout << vecDirectories[i] << "\n";
            directoryList += vecDirectories[i] + "\n";
        }

        std::cout << "\n";

        // send "OK" response
        pMessage->contentLength = directoryList.size();
        pMessage->statusCode = 200;
        sscanf("OK", "%s", pMessage->statusText);
        H_SendResponse(pMessage);

        send(pMessage->clientSocket, directoryList.c_str(), strlen(directoryList.c_str()), 0);

    }    
    else if(vecDirectories.empty())
    {
        WriteError( (char*)"No backups found\n" );
    
        // backup does not exist
        pMessage->statusCode = 404;
        pMessage->contentLength = 0;
        sscanf("Not Found", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
    }
    else
    {
        WriteError( (char*)"Error unknown: List()\n" );
    
        // Interval server error 
        pMessage->statusCode = 500;
        pMessage->contentLength = 0;
        sscanf("Internal Server Error", "%[^\t\n]", pMessage->statusText);
        H_SendResponse(pMessage);
    }
    

}



std::vector<std::string> List_GetExistingDirectories()
{
    DIR* directory;
    struct dirent* entry;
    std::vector<std::string> vecDirectories;

    if( (directory = opendir(".")) != NULL)
    {
        while( (entry = readdir(directory)) != NULL)
        {
            if(H_IsFolder(entry->d_name))
            {
                // don't add the two files "." and ".."
                if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                {
                    // make sure that it is a backup folder by checking the name
                    std::string temp = entry->d_name;
                    if( temp.substr(0, 7) == "backup-" )
                    {
                        vecDirectories.push_back(entry->d_name);
                    }

                }
            }

        }
    }
    else
    {
        printf("Error: could not open directory\n");
    }

    closedir(directory);
    return vecDirectories;
}



                                    ///////////// Writing errors to console /////////////


// for writing errors to the server console with no formatting
void WriteErrorAndExit(char message[])
{
    write(STDERR_FILENO, message, strlen(message)); 
    exit(EXIT_FAILURE);
}


void WriteError(char message[], int arg)
{
    char errorMessage[BUFFER_SIZE];
    sprintf(errorMessage, message, arg);
    write(STDERR_FILENO, errorMessage, strlen(errorMessage)); 
}


void WriteError(char message[], char arg[])
{
    char errorMessage[BUFFER_SIZE];
    sprintf(errorMessage, message, arg); // combine message and arg into one string
    write(STDERR_FILENO, errorMessage, strlen(errorMessage)); // write it to console
}


void WriteError(char message[])
{
    write(STDERR_FILENO, message, strlen(message)); 
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                      ASGN 1 FUNCTIONS 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


unsigned long H_GetAddress(char *name)
{
    unsigned long     res;
    struct addrinfo   hints;    
    struct addrinfo*  info;

    memset( &hints, 0, sizeof(hints) );

    hints.ai_family = AF_INET;

    if ( getaddrinfo(name, NULL, &hints, &info) != 0 || info == NULL ) { WriteErrorAndExit( (char*)"Error: address identification error\n"); }

    res = ((struct sockaddr_in*) info->ai_addr)->sin_addr.s_addr;

    freeaddrinfo(info);

    return res;
}


void H_CheckReadWriteAccess(HttpObject* pMessage)
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

int H_CheckMethod(HttpObject* pMessage)
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

void H_CallMethod(HttpObject* pMessage)
{
    if( strcmp(pMessage->method, "PUT") == 0 )
    {
        PUT(pMessage);
    }
    else if( strcmp(pMessage->method, "GET") == 0 )
    {
        GET(pMessage);
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

void H_CheckHTTPVersion(HttpObject* pMessage)
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

void H_StoreContentLength(HttpObject* pMessage)
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

void H_SetStatusOnError(HttpObject* pMessage)
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

void H_ReadContentLength(HttpObject* pMessage)
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

bool H_CheckFileExists(HttpObject* pMessage)
{
    struct stat buffer;
    return ( stat(pMessage->fileName, &buffer) == 0 );
}

void H_SetResponse(HttpObject* pMessage)
{
    sprintf(pMessage->response, "%s %d %s\r\nContent-Length: %ld\r\n\r\n", pMessage->httpVersion, pMessage->statusCode, pMessage->statusText, pMessage->contentLength);
}

// if there is an error, we need to send the response early
void H_SendResponse(HttpObject* pMessage)
{
    if (!pMessage->hasSentResponse)
    {
        H_SetResponse(pMessage);

        ssize_t sendResult = send(pMessage->clientSocket, pMessage->response, strlen(pMessage->response), 0);
        pMessage->hasSentResponse = true;

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

void AcceptConnection(HttpObject* pMessage, struct sockaddr* clientAddress, socklen_t* clientAddressLength, int serverSocket)
{
    char waiting[] = "[+] server is waiting...\n";
    write(STDOUT_FILENO, waiting, strlen(waiting));

    // 1. Accept connection
    pMessage->clientSocket = accept( serverSocket, clientAddress, clientAddressLength );

    if(pMessage->clientSocket < 0)
    {
        char acceptError[] = "Error: accept()\n";
        write(STDOUT_FILENO, acceptError, strlen(acceptError));
        pMessage->contentLength = 0;
        pMessage->hasSentResponse = true;
        return;
    }
}

// This function takes care of reading the message
void ReadHTTPRequest(HttpObject* pMessage)
{
    if (!pMessage->hasSentResponse)
    {
        ssize_t bytesRead = 0;
        char tempBuf[BUFFER_SIZE];

        // if client didn't send any info 
        if( (bytesRead = recv( pMessage->clientSocket, pMessage->buffer, BUFFER_SIZE, 0)) < 0 )
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

        std::string specialRecovery = tempBuf;
        
        // check for backup call
        if( strcmp(pMessage->method, "GET") == 0 && strcmp(tempBuf, "b") == 0 )
        {
            Backup(pMessage);
        }

        // check for 1st recovery call
        else if( strcmp(pMessage->method, "GET") == 0 && strcmp(tempBuf, "r") == 0 )
        {
            Recovery(pMessage);
        }

        // check for 2nd recovery call
        else if( strcmp(pMessage->method, "GET") == 0 && specialRecovery.substr(0, 2) == "r/" )
        {
            pMessage->recoverySpecialFolder = tempBuf;
            pMessage->recoverySpecialFolder = pMessage->recoverySpecialFolder.substr(2, pMessage->recoverySpecialFolder.size());
            RecoverySpecial(pMessage);
        }

        // check for list call
        else if( strcmp(pMessage->method, "GET") == 0 && strcmp(tempBuf, "l") == 0 )
        {
            List(pMessage);
        }
        else
        {
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

}

// 3. Process Request
void ProcessHTTPRequest(HttpObject* pMessage)
{
    if (!pMessage->hasSentResponse)
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
void ConstructHTTPResponse(HttpObject* pMessage)
{  
    if(!pMessage->hasSentResponse)
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
void SendHTTPResponse(HttpObject* pMessage)
{
    if(!pMessage->hasSentResponse)
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

void GET(HttpObject* pMessage)
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

        WriteError( (char*)"Error: can't open file %s\n", pMessage->fileName);
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
            //warn("Error: can't read from file %s\n", pMessage->fileName);
            WriteError( (char*)"Error: can't read file %s\n", pMessage->fileName);
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
                WriteError( (char*)"Error: something went wrong in GET()\n");
                H_SetStatusOnError(pMessage);
                return;
            }
            else
            {
                bytesWritten = write(pMessage->clientSocket, pMessage->buffer, bytesRead);

                // 5. Read any remaining bytes from socket, and write to file
                bytesRead         = read(fd, pMessage->buffer, sizeof(pMessage->buffer));
                totalBytesWritten += bytesWritten;
            }
        }    
    }

    close(fd);
    H_ReadContentLength(pMessage);
}

// PUT copies a file from client to server
void PUT(HttpObject* pMessage)
{
    // 1. Attempt to open file. If file exists, we will rewrite, if not we create a new file
    int fd = open(pMessage->fileName, O_CREAT | O_RDWR | O_TRUNC, 0644);

    // 2. Check for errors (permissions, file doesn't exit, etc)
    if(fd < 0)
    {
        close(fd);
        WriteError( (char*)"Error: can't open file %s\n", pMessage->fileName);
        H_SetStatusOnError(pMessage);
        H_SendResponse(pMessage);
        return;
    }
    else
    {   
        ssize_t bytesRead = 0;

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

        // 4. Read from buffer
        bytesRead = read(pMessage->clientSocket, pMessage->buffer, sizeof(pMessage->buffer)); // <- stuck here if given empty file

        if(bytesRead == -1)
        {
            close(fd);
            // warn("Error: can't read from client socket %s\n", pMessage->fileName);
            WriteError( (char*)"Error: can't read from client socket %d\n", pMessage->clientSocket);
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
                bytesRead         = read(pMessage->clientSocket, pMessage->buffer, sizeof(pMessage->buffer));
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