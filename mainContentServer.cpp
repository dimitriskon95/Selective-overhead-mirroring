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

#include "ContentServer.h"

using namespace std;

int main(int argc, char *argv[])
{
    int port=-1;
    char *dirorfilename;
    ContentServer *contentServer;

    //----Read the arguments from argv[]-----
    for(int i=1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            port = atoi(argv[i+1]);
        }
        else if (strcmp(argv[i], "-d") == 0)
        {
            dirorfilename = argv[i+1];
        }
    }

    if (port < 0 || argc != 5)
    {
        cout << "Error: You gave invalid or less arguments" << endl;
        cout << "Must be: -p <port> -d <dirorfilename>" << endl;
        exit(1);
    }

    contentServer = new ContentServer(port,dirorfilename);

    contentServer->masterThread();

    delete contentServer;

    cout << "Exiting..." << endl;

    return 0;
}
