-----------
Authors:   Rory Glenn
Email:     romglenn@ucsc.edu
Quarter:   Fall 2020
Class:     CSE 130
Project:   asgn3: Back-up and Recovery in the HTTP server
Professor: Faisal Nawab


-----------
DESCRIPTION

Runs a simple single-threaded HTTP server. The server responds to simple
GET, PUT commands to read and write files. Project is similar to asgn1
except that you can backup the server folder, recover a backup, and 
list the current backups available.

-----------
FILES

-
httpserver.cpp

This file is the implementation code used for creating the server

-
httpserver.h

This file is the definition code used for creating the server

-
Makefile

This file contains the code needed in order to compile and link the 
executable for httpserver

-
DESIGN.pdf

This file contains the design documentation for the software


-----------
INSTRUCTIONS

This program is intended to be run in a linux environment from the terminal.

If only the localhost (127.0.0.1) is given on the command line,
the server will start in default mode which sets the port number to 80.
Use either of the two lines below to do this:
sudo ./httpserver (localhost)
sudo ./httpserver (127.0.0.1)

The server can be started with more arguments if desired.
For example, port number may be given
./httpserver (localhost) (port)

For a GET request
curl http://localhost:(port)/(filename)
curl http://127.0.0.1:(port)/(filename)

For a PUT request
curl -T (filename) http://localhost:(port)/(newfilename)
curl -T (filename) http://127.0.0.1:(port)/(newfilename)

To backup the server files
curl http://localhost:(port)/b

To recover the most recent backup
curl http://localhost:(port)/r

To recover a specific backup
curl http://localhost:(port)/r/backup_number

To list the current backups
curl http://localhost:(port)/l
