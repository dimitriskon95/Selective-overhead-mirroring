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
#include <pthread.h>
#include <netdb.h>
#include <error.h>
#include <dirent.h>
#include <math.h>

#include "MirrorServer.h"
#include "readwriteFunctions.h"
#include "request.h"
#include "Queue.h"

using namespace std;

pthread_cond_t cond_alldone;
pthread_cond_t cond_nonempty;

pthread_mutex_t bmtx;
pthread_mutex_t filemtx;

void* manager_thread(void *ptr);           //Mirror Manager thread (used to ask from Content Servers the list of directories and files)
void* worker_thread(void *ptr);            //Mirror Manager thread (used to fetch a file from Content Servers or create a directory)

MirrorServer::MirrorServer(int portNumber, int threads, char* path)
{
    int size = strlen(path);
    port = portNumber;
    NumOfThreads = threads;

    dirname = new char[size+1];
    strcpy(dirname, path);
    dirname[size] = '\0';

    requestList = new RequestList;
    recordQueue = new Queue;

    pthread_mutex_init(&bmtx, NULL);
    pthread_mutex_init(&filemtx, NULL);
    cout << "A MirrorServer was constructed with Port: " << this->port << " and NumOfThreads: " << this->NumOfThreads << endl;
}

MirrorServer::~MirrorServer()
{
    delete [] dirname;
    delete requestList;
    delete recordQueue;
    pthread_mutex_destroy(&bmtx);
    pthread_mutex_destroy(&filemtx);
    cout << "A MirrorServer was destructed" << endl;
}

int MirrorServer::getPort()
{
    return this->port;
}

int MirrorServer::getNumOfThreads()
{
    return this->NumOfThreads;
}

//Doesnt need mutex because it is called in fetch_File function that it is locked by filemtx mutex
void MirrorServer::increase_FilesTransferred(int addbytes, int requestID)
{
    pthread_mutex_lock(&bmtx);
    int index = requestID-1;
    bytesTransferred += addbytes;
    bytesperDevice[index] += addbytes;
    filesTransferred += 1;
    pthread_mutex_unlock(&bmtx);
}

int MirrorServer::get_FilesTransferred()
{
    return filesTransferred;
}

int MirrorServer::get_BytesTransferred()
{
    int value;
    pthread_mutex_lock(&bmtx);
    value = bytesTransferred;
    pthread_mutex_unlock(&bmtx);
    return value;
}

void MirrorServer::masterThread()
{
    int sock, newsock, threadnum=this->getNumOfThreads();
    char buffer[1024];
    char* mirrorServer = NULL;
    bool flag;
    struct threadData* data;
    pthread_t *connection ,*workerid;

    struct sockaddr_in server, client;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct sockaddr *clientptr = (struct sockaddr*)&client;
    struct hostent *rem;

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)      /* Create socket */
    {
        perror("socket");
        exit(1);
    }

    server.sin_family = AF_INET;                           /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);                         /* The given port */

    if (bind(sock, serverptr, sizeof(server)) < 0)         /* Bind socket to address */
    {
        perror("bind");
        exit(1);
    }

    if (listen(sock, 5) < 0)                               /* Listen for connections */
    {
        perror("listen");
        exit(1);
    }
    printf("Listening for connections to port %d\n", port);

    //Create workers
    cout << "Creating " << threadnum << " workers..." << endl;
    workerid = new pthread_t[threadnum];
    for(int i=0; i< threadnum; i++)
    {
        data = (struct threadData*)malloc(sizeof(struct threadData));      //Allocate Memory for every data (different request/ same server)
        data->server = this;
        data->request = NULL;
        data->queue = recordQueue;
        data->initSocket = -1;
        pthread_create(&workerid[i], NULL, worker_thread, (void*)data);
        data = NULL;
    }

    while (1)
    {
        //Accept a connection from MirrorInitiator
        if ((newsock = accept(sock, clientptr, &clientlen)) < 0){
            perror("accept");
            exit(1);
        }
        printf("Accepted connection\n");

        //If flag is flase we run a Mirroring request else we terminate all servers
        flag = this->initiate_Mirroring(newsock);              //Initiate Mirroring and create requestList from received requests from MirrorInitiator

        //Create MirrorManager Threads
        connection = new pthread_t[this->managers];
        for(int i=0; i< this->managers; i++)
        {
            data = (struct threadData*)malloc(sizeof(struct threadData));      //Allocate Memory for every data (different request/ same server)
            data->server = this;
            data->request = requestList->getRequest(i+1);
            data->queue = recordQueue;
            data->initSocket = newsock;
            pthread_create(&connection[i], NULL, manager_thread, (void*)data);
            data = NULL;
        }

        //Wait for all MirrorManager to Finish
        for(int i=0; i< this->managers; i++)                                  //Wait for mirrorManager threads to finish
            pthread_join(connection[i], NULL);

        delete [] connection;

        this->terminate_Mirroring(newsock);

        if (flag) break;
    }

    for(int i=0; i< threadnum; i++)                                  //Wait for worker threads to finish
        pthread_join(workerid[i], NULL);

    delete [] workerid;
}


//------------------------Mirror Manager thread------------------------------
void* manager_thread(void *ptr)
{
    int sock, contentServerPort, numOfDirorfiles=0, len, size;
    struct threadData* data = (struct threadData*)(ptr);
    char *contentServerAddress = NULL, *dirfilename;
    char buffer[1024], contentServerPath[1024];
    char type;
    Record* record;

    cout << "A Manager thread was created with id: " << pthread_self() << endl;
    cout << "Thread " << pthread_self() << ": "; (data->request)->print();

    contentServerAddress = (data->request)->getServerAddress();
    contentServerPort = (data->request)->getServerPort();

    struct sockaddr_in contentserver;
    struct sockaddr *contentserverptr = (struct sockaddr*)&contentserver;
    struct hostent *rem;

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)                /* Create socket */
    {
        perror("socket");
        exit(1);
    }

    if ((rem = gethostbyname(contentServerAddress)) == NULL)                /* Find server address */
    {
        perror("gethostbyname");
        sprintf(buffer, "Host %s is unreachable", contentServerAddress);
        write_data((data->initSocket), buffer);
        pthread_exit(NULL);
    }

    contentserver.sin_family = AF_INET;                              /* Internet domain */
    memcpy(&contentserver.sin_addr, rem->h_addr, rem->h_length);
    contentserver.sin_port = htons(contentServerPort);               /* Server port */

    if (connect(sock, contentserverptr, sizeof(contentserver)) < 0)  /* Initiate connection */
    {
        perror("connect");
        sprintf(buffer, "Host %s is unreachable", contentServerAddress);
        write_data((data->initSocket), buffer);
        pthread_exit(NULL);
    }

    //Send message
    if (strcmp((data->request)->getDirorfile(), "Termination") == 0)
    {
        strcpy(buffer, "Termination");
        write_data(sock, buffer);
        cout << "Thread " << pthread_self() << ": Closing connection (list)" << endl;
        close(sock);
        cout << "Thread " << pthread_self() << ": Terminated" << endl;
        free(data);
        pthread_exit(NULL);
    }

    //Create command LIST <ContentServerID> <delay> and Send it to Content Server
    sprintf(buffer, "LIST %d %d", (data->request)->getID(), (data->request)->getDelay());
    write_data(sock, buffer);

    read_data(sock, buffer);                   //Reading the number of files and directories names that it will received
    numOfDirorfiles = atoi(buffer);

    read_data(sock, contentServerPath);         //Reading the local path of the ContentServer

    for(int i=0; i<numOfDirorfiles; i++)
    {
        read_data(sock, buffer);                                                 //read a file or directory and save it at buffer ,like "D:/home/.../directoryX" or "F:/home/.../fileX"
        type = (data->server)->set_DirorfileName(dirfilename, buffer);           //Get dirorfilename and type
        if ((data->server)->match_Path(dirfilename, (data->request)->getDirorfile()))
        {
            record = (data->server)->create_Record(contentServerAddress, contentServerPort, buffer, (data->request)->getID(), contentServerPath);
            cout << "Thread " << pthread_self() << ": Push " << buffer << " on Queue" << endl;
            (data->queue)->push(record);
        }
        delete [] dirfilename;
    }

    cout << "Thread " << pthread_self() << ": Closing connection (list)" << endl;
    close(sock);
    cout << "Thread " << pthread_self() << ": Terminated" << endl;
    free(data);
    pthread_exit(NULL);
}

//------------------------Mirror worker thread------------------------------
void* worker_thread(void* ptr)
{
    struct threadData* data = (struct threadData*)(ptr);
    int sock, contentServerPort, numOfDirorfiles=0, len, size;
    char* contentServerAddress = NULL;
    char buffer[1024];
    char type;
    Record* record;

    cout << "A Worker thread was created with id: " << pthread_self() << endl;

    while(1)
    {
        //Pop the first Record of the queue (if it is empty block)
        record = data->queue->pop();
        //If Record has termination as value
        if (strcmp(record->getContentServerAddress(), "Termination") == 0)
        {
            (data->queue)->decrease_RunningWorkers();
            delete record;
            break;
        }
        //If Record is referred to a Directory (not need to receive sth from ContentServer)
        if (record->getType() == 'D')                        //Check if it is a Directory
        {
            pthread_mutex_lock(&filemtx);
            (data->server)->create_Directory(record);
            pthread_mutex_unlock(&filemtx);
            (data->queue)->decrease_RunningWorkers();
            delete record;
            continue;
        }
        //If it is a file, use record data to connect to the ContentServer saved at Record
        contentServerAddress = record->getContentServerAddress();
        contentServerPort = record->getContentServerPort();

        struct sockaddr_in contentserver;
        struct sockaddr *contentserverptr = (struct sockaddr*)&contentserver;
        struct hostent *rem;

        if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)                // Create socket
        {
            perror("socket");
            exit(1);
        }

        if ((rem = gethostbyname(contentServerAddress)) == NULL)                // Find server address
        {
            perror("gethostbyname");
            exit(1);
        }

        contentserver.sin_family = AF_INET;                              // Internet domain
        memcpy(&contentserver.sin_addr, rem->h_addr, rem->h_length);
        contentserver.sin_port = htons(contentServerPort);               // Server port

        if (connect(sock, contentserverptr, sizeof(contentserver)) < 0)  // Initiate connection
        {
            perror("connect");
            exit(1);
        }

        //Sent FETCH request to Content Server
        sprintf(buffer, "FETCH %s %d", record->getDirorfilename(), record->getRequestID());
        write_data(sock, buffer);

        //Fetch file from Content Server
        pthread_mutex_lock(&filemtx);
        (data->server)->fetch_File(sock, record);
        pthread_mutex_unlock(&filemtx);

        (data->queue)->decrease_RunningWorkers();
        cout << "Worker thread " << pthread_self() << ": Closing connection (fetch)" << endl;
        close(sock);
    }

    cout << "Worker thread " << pthread_self() << ": Terminated" << endl;
    free(data);
    pthread_exit(NULL);
}

//Read bytes from Socket and write at a file
void MirrorServer::fetch_File(int sock, Record *record)
{
    int remain_data=0, bytesReceived=0, totalbytes=0;
    char recvBuff[256];                   //  memset(recvBuff, '0', sizeof(recvBuff));
    char *filename;
    FILE *received_file;

    read_data(sock, recvBuff);                      //Read fileSize
    remain_data = atoi(recvBuff);

    filename  = this->create_Filename(record);       //Get new filename
    cout << "Worker thread " << pthread_self() << ": Creating File: " << filename << endl;

    received_file = fopen(filename, "w");
    if (received_file != NULL)
    {
        while((bytesReceived = read(sock, recvBuff, 256)) > 0)
        {
            fwrite(recvBuff, 1, bytesReceived, received_file);
            totalbytes += bytesReceived;
            cout << "Worker thread " << pthread_self() << ": Fetching..." << endl;
        }
        cout << "Worker thread " << pthread_self() << ": File Transferred." << endl;
        this->increase_FilesTransferred(totalbytes, record->getRequestID());
        fclose(received_file);
    }
    else
    {
        perror("Failed to open file\n");
    }

    delete record;
    delete [] filename;
}

//First Receive the number of Requests from Initiator and receive requests, request are added at the start of the RequestList
bool MirrorServer::initiate_Mirroring(int newsock)
{
    int numOfRequests;
    char buffer[1024];
    bool flag = false;
    Request* request;

    read_data(newsock, buffer);
    if (strcmp(buffer, "Termination") == 0)
    {
        flag = true;
        terminate_Workers();
    }

    read_data(newsock, buffer);       //Receive number of total requests
    numOfRequests = atoi(buffer);

    if (numOfRequests <= 0)
    {
        cout << "Error: Number Of Requests must be greater than zero" << endl;
        exit(1);
    }
    this->managers = numOfRequests;         //set Number of Mirror Managers = Number of TCP connections = Number of Requests
    bytesperDevice = new int[numOfRequests];
    bytesTransferred = 0;
    filesTransferred = 0;

    for(int i=0; i<numOfRequests; i++)
    {
        read_data(newsock, buffer);
        request = this->create_Request(buffer);
        requestList->add(request);
        bytesperDevice[i] = 0;
    }

    return flag;
}

//First Receive the number of Requests from Initiator and receive requests, request are added at the start of the RequestList
void MirrorServer::terminate_Workers()
{
    char buffer[1024];
    Record *record;

    strcpy(buffer, "Termination");
    for(int i=0; i<this->getNumOfThreads(); i++)
    {
        record = new Record(buffer, buffer, buffer, 0, buffer[0], 0);
        recordQueue->push(record);
    }
}

//First Receive the number of Requests from Initiator and receive requests, request are added at the start of the RequestList
void MirrorServer::terminate_Mirroring(int sock)
{
    double averageBytes=0, variance=0;
    char response[100];

    //Condition variable all done (wait signal)
    recordQueue->wait_requestCompletion();

    averageBytes = calculate_Average();
    variance = calculate_Variance(averageBytes);

    sprintf(response, "bytesTransferred: %d, filesTransferred: %d, averageBytes: %lf, variance: %lf", get_BytesTransferred(), get_FilesTransferred(), averageBytes, variance);
    write_data(sock, response);

    strcpy(response, "Termination");
    write_data(sock, response);

    requestList->clear();                 //clear request List in order MirrorServer is ready to receive another mirroring request
    delete [] bytesperDevice;             //free array that was allocated in initiate_Mirroring function

    cout << "Closing connection" << endl;
    close(sock);
}

double MirrorServer::calculate_Average()
{
    return (get_BytesTransferred()/managers);
}

double MirrorServer::calculate_Variance(double average)
{
    double variance=0;
    for(int i=0; i<managers; i++)
        variance += pow(bytesperDevice[i]-average, 2);
    variance = variance/managers;
    return variance;
}

//Reads the string received from Initiator and generate a Request to a ContentServer
Request* MirrorServer::create_Request(char* buffer)
{
    int port, delay, count=0, tmpcount=0, identifier;
    char *address, *dirfile;
    char tmpBuffer[256];
    Request* request;

    while(buffer[count] != ':' && buffer[count] != '\0')
    {
        tmpBuffer[tmpcount] = buffer[count];
        tmpcount++;  count++;
    }
    tmpBuffer[tmpcount] = '\0';
    address = new char[tmpcount+1];
    strcpy(address, tmpBuffer);
    address[tmpcount] = '\0';

    tmpcount=0; count+=1;
    while(buffer[count] != ':' && buffer[count] != '\0')
    {
        tmpBuffer[tmpcount] = buffer[count];
        tmpcount++;  count++;
    }
    tmpBuffer[tmpcount] = '\0';
    port = atoi(tmpBuffer);

    tmpcount=0; count+=1;
    while(buffer[count] != ':' && buffer[count] != '\0')
    {
        tmpBuffer[tmpcount] = buffer[count];
        tmpcount++;  count++;
    }
    tmpBuffer[tmpcount] = '\0';
    dirfile = new char[tmpcount+1];
    strcpy(dirfile, tmpBuffer);
    dirfile[tmpcount] = '\0';

    tmpcount=0; count+=1;
    while(buffer[count] != '\0')
    {
        tmpBuffer[tmpcount] = buffer[count];
        tmpcount++;  count++;
    }
    tmpBuffer[tmpcount] = '\0';
    delay = atoi(tmpBuffer);

    identifier = requestList->getNumOfRequests()+1;

    request = new Request(address, port, dirfile, delay, identifier);
    return request;
}

Record* MirrorServer::create_Record(char* address, int port, char* buffer, int id, char* cspath)
{
    int size, count=0, tmpcount=0;
    char rectype;
    char dirfile[1024];
    Record* record;

    rectype = buffer[0];

    count+=2; tmpcount=0;
    while(buffer[count] != '\0')
    {
        dirfile[tmpcount] = buffer[count];
        tmpcount++; count++;
    }
    dirfile[tmpcount] = '\0';

    record = new Record(dirfile, address, cspath, port, rectype, id);
    return record;
}

//buffer is the result of reading from Content Server and requestDirorfile from Mirror Initiator
bool MirrorServer::match_Path(char* fpath, char* requestDirorfile)     //bool     //fpath = fullpath
{
    int fcount=0, tmpcount=0, subcount=0, fpathsize = strlen(fpath), subpathsize=0, rsize = strlen(requestDirorfile);
    char *subpath;

    subpathsize = fpathsize;
    subpath = new char[subpathsize+1];
    strcpy(subpath, fpath);
    subpath[subpathsize] = '\0';

    while(subpathsize >= rsize)
    {
        if (strncmp(subpath, requestDirorfile, rsize) == 0)
        {
            delete [] subpath;
            return true;
        }

        delete [] subpath;

        while(fpath[fcount] != '/' && fpath[fcount] != '\0')
            fcount++;

        if (fpath[fcount] == '/')
            fcount+=1;                     //fcount points to the first character after the first '/'

        if (fcount == fpathsize)      //cout << "read all the string" << endl;
            return false;

        subcount=0;
        subpathsize = strlen(fpath) - fcount;
        subpath = new char[subpathsize+1];
        tmpcount = fcount;
        while(fpath[tmpcount] != '\0')
        {
            subpath[subcount] = fpath[tmpcount];
            subcount++; tmpcount++;
        }
        subpath[subcount] = '\0';
    }
    delete [] subpath;
    return false;
}


char MirrorServer::set_DirorfileName(char*& path, char* buffer)
{
    int count=0,tmpcount=0,size;
    char type = buffer[0];

    while(buffer[count] != '/')
        count++;

    size = strlen(buffer) - count;
    path = new char[size+1];
    while(buffer[count] != '\0')
    {
        path[tmpcount] = buffer[count];
        tmpcount++; count++;
    }
    path[tmpcount] = '\0';
    return type;
}

char* MirrorServer::create_Filename(Record* record)
{
    char *cspath = record->getContentServerPath();              //Path of ContentServer files and directories
    char buffer[1024];
    char *recordFilename = record->getDirorfilename();
    char *newfilename;

    int count = strlen(cspath)+1, tmpcount=0, size;

    while(recordFilename[count] != '\0')
    {
        buffer[tmpcount] = recordFilename[count];
        tmpcount++; count++;
    }
    buffer[tmpcount] = '\0';

    size = strlen(this->dirname) + 1 + strlen(buffer);
    newfilename = new char[size+1];
    strcpy(newfilename, this->dirname);
    strcat(newfilename, "/");
    strcat(newfilename, buffer);
    newfilename[size] = '\0';

    count = strlen(this->dirname)+1;                       //Index at the start of the directories that will be created (after DataStore)
    int lastslash = count-1;

    while(newfilename[count] != '\0')
    {
        if (newfilename[count] == '/')
            lastslash = count;
        count++;
    }

    count = strlen(this->dirname)+1;                       //Index at the start of the directories that will be created (after DataStore)

    DIR* dir;
    while(count <= lastslash)
    {
        if (newfilename[count] == '/')
        {
            strncpy(buffer, newfilename, count);
            buffer[count] = '\0';
            cout << "Opening Directory : " << buffer << endl;
            dir = opendir(buffer);
            if (dir)
            {
                closedir(dir);
                cout << "Closing Directory: " << buffer << endl;
            }
            else
            {
                mkdir(buffer, 0773);
                cout << "Create Directory: " << buffer << endl;
            }
        }
        count++;
    }
    return newfilename;
}


void MirrorServer::create_Directory(Record* record)
{
    char *cspath = record->getContentServerPath();              //Path of ContentServer files and directories
    char buffer[1024];
    char *recordDirname = record->getDirorfilename();
    char *subdirectory;
    struct stat st = {0};

    int count = strlen(cspath)+1, tmpcount=0, size;

    while(recordDirname[count] != '\0')
    {
        buffer[tmpcount] = recordDirname[count];
        tmpcount++; count++;
    }
    buffer[tmpcount] = '\0';

    size = strlen(this->dirname) + 1 + strlen(buffer);
    subdirectory = new char[size+1];
    strcpy(subdirectory, this->dirname);
    strcat(subdirectory, "/");
    strcat(subdirectory, buffer);
    subdirectory[size] = '\0';

    count = strlen(this->dirname)+1;                       //Index at the start of the directories that will be created (after DataStore)
    int lastslash = count-1;

    while(subdirectory[count] != '\0')
    {
        if (subdirectory[count] == '/')
            lastslash = count;
        count++;
    }
    count = strlen(this->dirname)+1;                       //Index at the start of the directories that will be created

    DIR* dir;
    while(count <= lastslash)
    {
        if (subdirectory[count] == '/')
        {
            strncpy(buffer, subdirectory, count);
            buffer[count] = '\0';

            cout << "Opening Directory : " << buffer << endl;
            dir = opendir(buffer);
            if (dir)
            {
                closedir(dir);
                cout << "Closing Directory: " << buffer << endl;
            }
            else
            {
                mkdir(buffer, 0773);
                cout << "Create Directory: " << buffer << endl;
            }
        }
        count++;
    }
    delete [] subdirectory;
}



/*
int remain_data=0, bytesReceived=0;
char recvBuff[256];          //  memset(recvBuff, '0', sizeof(recvBuff));
char *filename;
FILE *received_file;

//Read fileSize
read_data(sock, recvBuff);
remain_data = atoi(recvBuff);

filename  = (data->server)->create_File(tmpRecord);       //Get new filename
cout << "Worker thread " << pthread_self() << ": Creating File: " << filename << endl;
delete tmpRecord;

pthread_mutex_lock(&filemtx);
received_file = fopen(filename, "w");
if (received_file == NULL)
{
    cout << "Failed to open file " << filename << endl;
    delete [] filename;
    continue;
}

while((bytesReceived = read(sock, recvBuff, 256)) > 0)
{
    fwrite(recvBuff, 1, bytesReceived, received_file);
    cout << "Worker thread " << pthread_self() << ": Fetching..." << endl;
    (data->server)->increase_BytesTransferred(bytesReceived);
}
cout << "Worker thread " << pthread_self() << ": File Transferred." << endl;
(data->server)->increase_FilesTransferred();

fclose(received_file);
pthread_mutex_unlock(&filemtx);

cout << "Worker thread " << pthread_self() << ": Closing connection (fetch)" << endl;
close(sock);




(data->queue)->decreaseWorkers();

delete [] filename;

*/
