-----------
Name:      Rory Glenn
Email:     romglenn@ucsc.edu
Quarter:   Fall 2020
Project:   asgn0
Professor: Faisal Nawab
-----------
DESCRIPTION

This program accepts files from the command line and reads them in reverse order.
-----------
FILES

-
dog.c

This file is the implementation code used for creating dog

-
Makefile

This file contains the code needed in order to compile and link the executable
for dog

-
DESIGN.pdf

This file contains the design documentation for the software

-----------
INSTRUCTIONS

This program is intended to be run in a linux environment from the terminal.
It can be run in multiple ways. The most simplistic way to run dog is to input
one file name and have the contents of the file be displayed to screen.

./dog [filename]

If the user inputs a dash as an argument, the program will continue to read 
standard input and display it to standard output just like cat does

./dog "-"

The user can also inut a dash and a file name in any order they wish. 
This will read the arguments in the order the were given and display
them accordingly.

./dog [filename] "-"

or

./dog "-" [filename]


The user can also have multiple files displayed to the screen
in addition to using a dash

./dog [file1] [file2] "-"

