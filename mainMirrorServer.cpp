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

#include "MirrorServer.h"

using namespace std;

int main(int argc, char *argv[])
{
    int port=-1, threadnum=-1;
    char *dirname;
    MirrorServer *server;

    //----Read the arguments from argv[]-----
    for(int i=1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            port = atoi(argv[i+1]);
        }
        else if (strcmp(argv[i], "-m") == 0)
        {
            dirname = argv[i+1];
        }
        else if (strcmp(argv[i], "-w") == 0)
        {
            threadnum = atoi(argv[i+1]);
        }
    }

    if (port < 0 || threadnum < 0 || argc != 7)
    {
        cout << "Error: You gave invalid or less arguments" << endl;
        cout << "Attributes must be: -p <port> -m <dirname> -w <threadnum>" << endl;
        exit(1);
    }

    server = new MirrorServer(port,threadnum,dirname);

    server->masterThread();

    delete server;

    cout << "Exiting..." << endl;

    return 0;
}
