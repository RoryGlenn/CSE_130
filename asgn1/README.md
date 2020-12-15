-----------
Authors:   Rory Glenn, Jeremiah Chen
Email:     romglenn@ucsc.edu, jchen202@ucsc.edu
Quarter:   Fall 2020
Class:     CSE 130
Project:   asgn1
Professor: Faisal Nawab


-----------
DESCRIPTION

Runs a simple single-threaded HTTP server. The server responds to simple
GET, PUT commands to read and write files.
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
It can be run in multiple ways. The most simplistic way to run the server is
to input one call at a time (GET, PUT) by using the curl command

On the server terminal, use one of the four lines below

./httpserver (localhost) (port)
./httpserver (127.0.0.1) (port)
./httpserver (localhost)
./httpserver (127.0.0.1)

One the client terminal use on of the following

For a GET request
curl http://localhost:(port)/(filename)

For a PUT request
curl -T (filename) http://localhost:(port)/(newfilename)

