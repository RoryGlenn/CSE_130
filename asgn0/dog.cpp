//---------------------------------------------------------------------------------
// Author:  Rory Glenn
// Class:   CSE 130 Fall 2020
// Teacher: Faisal Nawab
//
// dog.c
// Implementaion file for dog
//---------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 500

// prototype functions
void ReadWriteForever();
void ErrorCheck(int* fd, char* argv);

// start of program
int main(int argc, char* argv[])
{
	unsigned char buffer[BUFFER_SIZE];

	// if no command line arguments were given 
	if(argc == 1)									
	{
		ReadWriteForever();
	}

	// if a command line arg was given
	else if(argc > 1)					
	{
		for(int i = 1; i < argc; i++)
		{
			// if there is no dash in argument
			if(strcmp(argv[i], "-") != 0)			
			{
				int fd = open(argv[i], O_RDONLY);

				ErrorCheck(&fd, argv[i]);

				if(fd > 0)
				{
					ssize_t bytesRead = read(fd, &buffer, sizeof(buffer));
					
					while(bytesRead > 0)
					{
						if(bytesRead == -1)
						{
							warn("Error %d: file %s\n", errno, argv[i]);
						}
						else
						{
							if(write(STDOUT_FILENO, &buffer, bytesRead) != bytesRead)
							{
								// end of file has been reached
								break;				
							}
							bytesRead = read(fd, &buffer, sizeof(buffer));
						}

					}
					close(fd);
				}
			}
			// if there is a dash in argument
			else if(strcmp(argv[i], "-") == 0) 		
			{
				ReadWriteForever();
			}
		}
	}
}


// functions

void ReadWriteForever()
{
	unsigned char buffer[BUFFER_SIZE];

	// copy standard input to standard output
	while(read(STDIN_FILENO, buffer, 1) > 0)
	{
		if(write(STDOUT_FILENO, buffer, 1) == -1)
		{
			warn("Error %d: can't read input\n", errno);
		}
	}
}

void ErrorCheck(int* fd, char* argv)
{
	if(*fd == -1)
	{
		warn("Error %d: file %s\n", errno, argv);
		close(*fd);
	}
}