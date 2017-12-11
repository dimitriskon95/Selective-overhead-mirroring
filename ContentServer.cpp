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
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#include <dirent.h>

#include "ContentServer.h"
#include "readwriteFunctions.h"
#include "fileFunctions.h"

using namespace std;

void* list_thread(void* ptr);
void* fetch_thread(void* ptr);

ContentServer::ContentServer(int portNumber, char* path)
{
    int size = strlen(path);
    dirorfilename = new char[size+1];
    strcpy(dirorfilename, path);
    dirorfilename[size] = '\0';

    port = portNumber;
    dnode = NULL;

    pthread_mutex_init(&mtx, NULL);
    cout << "A ContentServer was constructed with Port: " << this->port << endl;
}

ContentServer::~ContentServer()
{
    struct delayNode *current, *delnode;
    current = dnode;
    while(current != NULL)
    {
        delnode = current;
        current = current->next;
        free(delnode);
    }
    delete [] dirorfilename;
    pthread_mutex_destroy(&mtx);
    cout << "A ContentServer was destructed" << endl;
}

char* ContentServer::getDirorfile()
{
    return dirorfilename;
}

int ContentServer::getPort()
{
    return port;
}

void ContentServer::masterThread()
{
    int sock, newsock, option, commandsize, numOfThreads=0;
    char buffer[1024];
    char* restcommand;
    char* contentServer = NULL;
    pthread_t *threadID;
    threadContentData *data;

    struct sockaddr_in server, mirrorserver;
    socklen_t mirrorserverlen = sizeof(struct sockaddr_in);
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct sockaddr *mirrorserverptr = (struct sockaddr*)&mirrorserver;
    struct hostent *rem;

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)      /* Create socket */
    {
        perror("socket");
        exit(1);
    }

    server.sin_family = AF_INET;                           /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(getPort());                         /* The given port */

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

    threadID = (pthread_t*)malloc(sizeof(pthread_t));

    while (1)
    {
        if ((newsock = accept(sock, mirrorserverptr, &mirrorserverlen)) < 0)          /* accept connection */
        {
            perror("accept");
            exit(1);
        }
        printf("Accepted connection\n");

        data = (struct threadContentData*)malloc(sizeof(struct threadContentData));      //Allocate Memory

        read_data(newsock, buffer);

        option = parseCommand(buffer, restcommand);                                         //command string has the rest of command

        data->server = this;
        data->sock = newsock;
        data->command = restcommand;

        if (option == 0)
            pthread_create(&threadID[numOfThreads], NULL, list_thread, (void*)data);                  //Create Thread
        else if (option == 1)
            pthread_create(&threadID[numOfThreads], NULL, fetch_thread, (void*)data);                //Create Thread
      	else
      	{
      	    cout << "Terminated Message was Received" << endl;
      	    delete [] (data->command);
      	    free(data);
      	    break;
      	}

        numOfThreads++;
	      threadID = (pthread_t*)realloc(threadID, sizeof(pthread_t)*(numOfThreads+1));

        data = NULL;
    }

    for(int i=0; i < numOfThreads; i++)
        pthread_join(threadID[i], NULL);

    free(threadID);
}


void* list_thread(void* ptr)
{
    cout << "A list Worker thread was created with id: " << pthread_self() << endl;
    int count=0, tmpcount=0, id, delay, size;
    char idBuffer[10], delayBuffer[10];
    struct threadContentData* data = (struct threadContentData*)(ptr);
    char *command = data->command;
    char buf[256];
    char *buffer;

    //Get id and delay
    while(command[count] != ' ')
    {
        idBuffer[tmpcount] = command[count];
        tmpcount++;  count++;
    }
    idBuffer[tmpcount] = '\0';
    id = atoi(idBuffer);

    tmpcount=0;  count++;
    while(command[count] != '\0')
    {
        delayBuffer[tmpcount] = command[count];
        tmpcount++;  count++;
    }
    delayBuffer[tmpcount] = '\0';
    delay = atoi(delayBuffer);

    cout << "List thread " << pthread_self() << ": LIST " << id << " " << delay << endl;

    //Create delayNode for this request
    struct delayNode* delaynode = (struct delayNode*)malloc(sizeof(struct delayNode));      //Allocate Memory
    delaynode->nodeID = id;
    delaynode->delayTime = delay;
    delaynode->next = NULL;
    (data->server)->add_delayNode(delaynode);                                    //Add DelayNode to the common list using mutex

    FileList* flist = new FileList;

    (data->server)->add_dir_content((data->server)->getDirorfile(), flist);

    cout << "List thread " << pthread_self() << ": Printing List :" << endl;
    flist->print();

    cout << "List thread " << pthread_self() << ": Writing data to MirrorServer" << endl;
    sprintf(buf, "%d", flist->getListSize());
    write_data(data->sock, buf);

    //Send the path that all files/ dirs are under at Content Server
    write_data(data->sock, (data->server)->getDirorfile());

    FileNode *current, *tmpnode;
    while(flist->getListSize() > 0)                                             //Send every file or directory
    {
        tmpnode = flist->removeNode();

        size = strlen(tmpnode->getDirorfile())+2;   //D:/home/.../dir1 or F:/home/.../file1
        buffer = new char[size+1];
        if (tmpnode->isFile())
            strcpy(buffer, "F:");
        else
            strcpy(buffer, "D:");
        strcat(buffer, tmpnode->getDirorfile());

        write_data(data->sock, buffer);

        delete [] buffer;
        delete tmpnode;
    }
    delete flist;

    cout << "List thread " << pthread_self() << ": Closing connection" << endl;
    close(data->sock);	                                                          // Close socket
    delete [] (data->command);
    free(data);
    pthread_exit(NULL);
}


void* fetch_thread(void* ptr)
{
    cout << "A fetch Worker thread was created with id: " << pthread_self() << endl;
    struct threadContentData* data = (struct threadContentData*)(ptr);
    int count=0, tmpcount=0, id, size, delay;
    char *filename;
    char tmpbuffer[1024], idbuffer[10];
    char *command = data->command;

    while(command[count] != ' ' && command[count] != '\0')
    {
        tmpbuffer[tmpcount] = command[count];
        tmpcount++; count++;
    }
    tmpbuffer[tmpcount] = '\0';

    size = strlen(tmpbuffer);
    filename = new char[size+1];
    strcpy(filename, tmpbuffer);
    filename[size] = '\0';

    count+=1; tmpcount=0;
    while(command[count] != '\0')
    {
        idbuffer[tmpcount] = command[count];
        tmpcount++; count++;
    }
    idbuffer[tmpcount] = '\0';
    id = atoi(idbuffer);

    delay = (data->server)->getDelayTime(id);
    cout << "Fetch thread " << pthread_self() << ": Delaying for " << delay << " seconds" << endl;
    sleep(delay);

    ssize_t len;
    FILE* fp;
    int fd;
    char filesize[256];
    struct stat file_stat;
    off_t offset = 0;

    fp = fopen(filename, "r");
    fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        cout << "Error opening file\n" << endl;
        exit(EXIT_FAILURE);
    }

    //Get file stats
    if (fstat(fd, &file_stat) < 0)
    {
        cout << "Error fstat\n" << endl;
        exit(EXIT_FAILURE);
    }

    sprintf(filesize, "%ld", file_stat.st_size);
    write_data(data->sock, filesize);

    while(1)
    {
        char buff[256]={0};
        int nread = fread(buff, 1, 256, fp);

        if (nread > 0)
        {
            write(data->sock, buff, nread);
        }
        if (nread < 256)
        {
            if (feof(fp))
            {
                cout << "Fetch thread " << pthread_self() << ": File transfer completed" << endl;
            }
            if (ferror(fp))
                cout << "Error reading" << endl;

            break;
        }
    }

    delete [] filename;
    if (fp != NULL)
    {
        cout << "Closing File" << endl;
        fclose(fp);
    }

    cout << "Fetch thread " << pthread_self() << ": Closing connection" << endl;
    close(data->sock);	  /* Close socket */
    delete [] (data->command);
    free(data);
    pthread_exit(NULL);
}

int ContentServer::parseCommand(char* buffer, char*& command)
{
    int count=0, tmpcount=0, size=0;
    char bufferComm[256], restCommand[256];

    while(buffer[count] != ' ' && buffer[count] != '\0')
    {
        bufferComm[tmpcount] = buffer[count];
        tmpcount++;  count++;
    }
    bufferComm[tmpcount] = '\0';

    if (strcmp(bufferComm, "Termination") == 0)
    {
    	  size = strlen(bufferComm);
       	command = new char[size+1];
       	strcpy(command, bufferComm);
       	command[size] = '\0';
        return -1;
    }

    count++;  tmpcount=0;
    while(buffer[count] != '\0')
    {
        restCommand[tmpcount] = buffer[count];
        tmpcount++;  count++;
    }
    restCommand[tmpcount] = '\0';

    size = strlen(restCommand);
    command = new char[size+1];
    strcpy(command, restCommand);
    command[size] = '\0';

    if (strcmp(bufferComm, "LIST") == 0)
        return 0;
    else if (strcmp(bufferComm, "FETCH") == 0)
        return 1;
}

void ContentServer::add_dir_content(char* path, FileList* flist)
{
    int size;
    DIR* d = opendir(path);
    if (d == NULL) return;
    struct dirent *dir;
    FileNode* fnode;
    char *fname;
    while((dir = readdir(d)) != NULL)
    {
        if (dir->d_type != DT_DIR)
        {
            size = strlen(path)+strlen(dir->d_name)+1;
            fname = new char[size+1];
            strcpy(fname, path);
            strcat(fname, "/");
            strcat(fname, dir->d_name);
            fname[size] = '\0';

            fnode = new FileNode(fname, true);
            flist->addNode(fnode);
        }
        else if ((dir->d_type == DT_DIR) && (strcmp(dir->d_name,".") != 0) && (strcmp(dir->d_name, "..") != 0))
        {
            char newpath[1024];
            sprintf(newpath, "%s/%s", path, dir->d_name);

            size = strlen(path)+strlen(dir->d_name)+1;
            fname = new char[size+1];
            strcpy(fname, path);
            strcat(fname, "/");
            strcat(fname, dir->d_name);
            fname[size] = '\0';

            fnode = new FileNode(fname, false);
            flist->addNode(fnode);

            add_dir_content(newpath, flist);
        }
    }
    closedir(d);
}

void ContentServer::add_delayNode(struct delayNode* addnode)
{
    pthread_mutex_lock(&mtx);
    struct delayNode* current = dnode;
    if (dnode == NULL)
        dnode = addnode;
    else
    {
        addnode->next = dnode;
        dnode = addnode;
    }
    pthread_mutex_unlock(&mtx);
}

int ContentServer::getDelayTime(int id)
{
    delayNode* current = dnode;
    while(current != NULL)
    {
        if (current->nodeID == id)
            return current->delayTime;
        current = current->next;
    }
}
