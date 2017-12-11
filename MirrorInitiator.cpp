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

#include "readwriteFunctions.h"
#define BUFSIZE 1024

using namespace std;

void initiate_Mirroring(int, char*);
void terminate_Servers(int, char*);
void receive_Response(int);

int main(int argc, char *argv[])
{
    int sock, port=-1;
    char buffer[BUFSIZE];
    char *contentServers, *serverHost = NULL;
    bool exit_flag = false;

    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;

    //----Read the arguments from argv[]-----
    for(int i=1; i < argc; i++)
    {
        if (strcmp(argv[i], "-n") == 0)
        {
            serverHost = argv[i+1];
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            port = atoi(argv[i+1]);
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            contentServers = argv[i+1];
        }
        else if (strcmp(argv[i], "-exit") == 0)
        {
            exit_flag = true;
        }
    }

    if (port == -1 || serverHost == NULL || argc < 7)
    {
        cout << "Error: You gave invalid or less arguments" << endl;
        cout << "Must be: -n <MirrorServerAddress> -p <MirrorServerPort> -s <ContentServerAddress1:ContentServerPort1:...>" << endl;
        exit(1);
    }

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)                   // Create socket
    {
        perror("socket");
        exit(1);
    }

    if ((rem = gethostbyname(serverHost)) == NULL)                      //Find server address
    {
        perror("gethostbyname");
        exit(1);
    }

    server.sin_family = AF_INET;                                        //Internet domain
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(port);                                      //Server port

    // Initiate connection
    if (connect(sock, serverptr, sizeof(server)) < 0)
    {
        perror("connect");
        exit(1);
    }
    printf("Connecting to %s port %d\n", serverHost, port);             //MirrorInitiator is now connected to MirrorServer

    if (exit_flag)
        terminate_Servers(sock, contentServers);                        //Send termiantion Message to MirrorServer
    else
        initiate_Mirroring(sock, contentServers);                       //Now we have to send Request (contentServer buffer)

    receive_Response(sock);                                             //Read from Mirror Server

    close(sock);                                                        //Close socket
}

void initiate_Mirroring(int sock, char* message)
{
    int size = strlen(message), numOfRequests=1, count=0, tmpcount=0, tmpsize=0;
    char buffer[BUFSIZE], buff[10];
    message[size] = '\0';

    strcpy(buffer, "Mirroring");
    write_data(sock, buffer);

    //Count the number of ContentServer Requests
    for(int i=0; i <= size; i++)
        if (message[i] == ',')
            numOfRequests++;

    sprintf(buff, "%d", numOfRequests);
    write_data(sock, buff);

    for(int i=0; i<numOfRequests; i++)
    {
        tmpcount=0;
        while(message[count] != ',' && message[count] != '\0')
        {
            buffer[tmpcount] = message[count];
            tmpcount++;  count++;
        }
        buffer[tmpcount] = '\0';
        count+=1;                  //pass the ','

        cout << "Sending to MirrorServer: <" << buffer << ">" << endl;
        write_data(sock, buffer);
    }
}

void terminate_Servers(int sock, char* message)
{
    int size = strlen(message), numOfRequests=1, count=0, tmpcount=0, tmpsize=0;
    char buffer[BUFSIZE], buff[10];
    message[size] = '\0';

    strcpy(buffer, "Termination");
    write_data(sock, buffer);

    //Count the number of ContentServer Requests
    for(int i=0; i <= size; i++)
        if (message[i] == ',')
            numOfRequests++;

    sprintf(buff, "%d", numOfRequests);
    write_data(sock, buff);

    for(int i=0; i<numOfRequests; i++)
    {
        tmpcount=0;
        while(message[count] != ',' && message[count] != '\0')
        {
            buffer[tmpcount] = message[count];
            tmpcount++;  count++;
        }
        buffer[tmpcount] = '\0';
        count+=1;                  //pass the ','

        cout << "Sending to MirrorServer: <" << buffer << ">" << endl;
        write_data(sock, buffer);
    }
}

void receive_Response(int sock)
{
    char buffer[BUFSIZE];
    cout << "Waiting to get Response from MirrorServer" << endl;
    while(1)
    {
        read_data(sock, buffer);
        if (strcmp(buffer, "Termination") == 0)
            break;
        cout << "Reading from Mirror Server: " << buffer << endl;
    }
    cout << "Closing Mirror Initiator" << endl;
}
