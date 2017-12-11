#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <error.h>

using namespace std;

int read_data(int fd, char *buffer)                   // Read formated data
{
    int i=0, length=0;
  	char temp;
  	if (read(fd, &temp, 1) < 0)	                         //Get length of string
  	{
        cout << "Error1: Reading from socket " << fd << endl;
        exit(-1);
    }
  	length = temp;
  	while (i < length)	                                 //Read $length chars
    {
    		if (i < ( i+= read(fd, &buffer[i], length - i)))
    		{
            cout << "Error2: Reading from socket " << fd << endl;
            exit(-1);
        }
    }
  	return i;	                                           //Return size of string
}

int write_data(int fd, char* message)                //Write formated data
{
    int length = 0;
    char temp;
	  length = strlen(message) + 1;                      	// Find length of string
	  temp = length;
	  if (write (fd, &temp, 1) < 0)	                      // Send length first
    {
        cout << "Error: Writing from socket " << fd << endl;
        exit(-2);
    }
  	if( write (fd, message, length) < 0 )                // Send string
    {
        cout << "Error: Writing from socket " << fd << endl;
        exit(-2);
    }
	  return length;	                                  	// Return size of string
}
