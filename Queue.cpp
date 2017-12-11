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
#include <pthread.h>
#include <unistd.h>

#include "Queue.h"

using namespace std;

int runningWorkers = 0;

Record::Record(char* name, char* address, char* conserverpath, int port, char tp, int id)
{
    int size;
    if (tp == 'D')          //is a directory
    {
        size = strlen(name)+1;
        dirorfilename = new char[size+1];
        strcpy(dirorfilename, name);
        strcat(dirorfilename, "/");
        dirorfilename[size] = '\0';
    }
    else
    {
        size = strlen(name);
        dirorfilename = new char[size+1];
        strcpy(dirorfilename, name);
        dirorfilename[size] = '\0';
    }

    size = strlen(address);
    contentServerAddress = new char[size+1];
    strcpy(contentServerAddress, address);
    contentServerAddress[size] = '\0';

    size = strlen(conserverpath);
    contentServerPath = new char[size+1];
    strcpy(contentServerPath, conserverpath);
    contentServerPath[size] = '\0';

    contentServerPort = port;
    type = tp;
    requestID = id;
    next = NULL;
}

Record::~Record()
{
    delete [] dirorfilename;
    delete [] contentServerAddress;
    delete [] contentServerPath;
}


char* Record::getDirorfilename()
{
    return dirorfilename;
}

char* Record::getContentServerAddress()
{
    return contentServerAddress;
}

char* Record::getContentServerPath()
{
    return contentServerPath;
}

int Record::getContentServerPort()
{
    return contentServerPort;
}

char Record::getType()
{
    return type;
}

int Record::getRequestID()
{
    return requestID;
}

Record* Record::getNext()
{
    return next;
}

void Record::setNext(Record* rec)
{
    next = rec;
}

void Record::print()
{
    cout << "Record: <" << dirorfilename << "," << contentServerAddress << "," << contentServerPort << "," << type << "," << requestID << ">" << endl;
}

Queue::Queue()
{
    queueSize = 0;
    headNode = NULL;
    lastNode = NULL;
    pthread_mutex_init(&mtx, NULL);
}

Queue::~Queue()
{
    Record *current = headNode, *delnode;
    while(current != NULL)
    {
        delnode = current;
        current = current->getNext();
        delete delnode;
    }
    pthread_mutex_destroy(&mtx);
}

int Queue::getQueueSize()
{
    return queueSize;
}

Record* Queue::getHeadNode()
{
    return headNode;
}

Record* Queue::getLastNode()
{
    return lastNode;
}

void Queue::push(Record* rec)
{
    pthread_mutex_lock(&mtx);
    if (queueSize == 0)     //empty
    {
        headNode = rec;
        lastNode = rec;
    }
    else
    {
        lastNode->setNext(rec);
        lastNode = rec;
    }
    queueSize++;
    pthread_cond_broadcast(&cond_nonempty);          //Queue is not empty. signal all workers
    pthread_mutex_unlock(&mtx);
}

Record* Queue::pop()
{
    cout << "Worker thread " << pthread_self() << ": Popping..." << endl;
    pthread_mutex_lock(&mtx);
    while(queueSize == 0)
    {
        pthread_cond_wait(&cond_nonempty, &mtx);      //an einai adeia perimenw signal
    }
    runningWorkers++;
    queueSize--;

    Record* tmpRec;
    if (headNode == lastNode)
    {
        tmpRec = headNode;
        headNode = NULL;
        lastNode = NULL;
    }
    else
    {
        tmpRec = headNode;
        headNode = tmpRec->getNext();
    }

    pthread_mutex_unlock(&mtx);
    return tmpRec;
}

void Queue::decrease_RunningWorkers()
{
    pthread_mutex_lock(&mtx);
    runningWorkers--;
    if (runningWorkers == 0 && queueSize == 0)
        pthread_cond_signal(&cond_alldone);
    pthread_mutex_unlock(&mtx);
}

void Queue::wait_requestCompletion()
{
    pthread_mutex_lock(&mtx);
    while(queueSize > 0 || runningWorkers > 0)
    {
        pthread_cond_wait(&cond_alldone, &mtx);            //an einai adeia perimenw signal
        cout << "Signal for condition variable alldone received" << endl;
    }
    pthread_mutex_unlock(&mtx);
}

void Queue::print()
{
    pthread_mutex_lock(&mtx);
    Record* current = getHeadNode();
    while(current != NULL)
    {
        current->print();
        current = current->getNext();
    }
    pthread_mutex_unlock(&mtx);
}
