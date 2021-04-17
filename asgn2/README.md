-----------
Authors:   Rory Glenn
Email:     romglenn@ucsc.edu
Quarter:   Fall 2020
Class:     CSE 130
Project:   asgn2
Professor: Faisal Nawab


-----------
DESCRIPTION

Runs a multi-threaded HTTP server. The server responds to
GET and PUT commands to read and write files. Use flags 
-N (number of threads) to set the number of threads and -r
to enable redundancy which reads or writes to three different 
files depending on whether a PUT or GET request is made

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

This file contains the code needed in order to compile and link the executable
for httpserver

-
DESIGN.pdf

This file contains the design documentation for the software

-----------
INSTRUCTIONS

This program is intended to be run in a linux environment from the terminal.
The most simplistic way to run the server is to input one call at a 
time (GET, PUT) by using the curl command.

If only the localhost (127.0.0.1) is given on the command line,
the server will start in default mode which sets the port number to 80, 
the number of threads to 4, and the redundancy flag disabled.
Use either of the two lines below to do this:

sudo ./httpserver (localhost)
sudo ./httpserver (127.0.0.1)


The server can be started with more arguments if desired. 
For example, port number may be given

./httpserver (localhost) (port)

The -N flag with the number of threads may be given.
If no number is given for the -N flag, the number
of threads defaults to four.

./httpserver (localhost) (port) -N 6
./httpserver (127.0.0.1) (port) -N 6

-r flag can be given to enable redundancy

./httpserver (localhost) (port) -N 6 -r
./httpserver (127.0.0.1) (port) -N 6 -r


On the client terminal use one of the following

For a GET request
curl http://localhost:(port)/(filename)
curl http://127.0.0.1:(port)/(filename)


For a PUT request
curl -T (filename) http://localhost:(port)/(newfilename)
curl -T (filename) http://127.0.0.1:(port)/(newfilename)
