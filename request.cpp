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

#include "request.h"

using namespace std;

Request::Request(char* address, int port, char* dorf, int del, int identifier)
{
    contentServerAddress = address;
    dirorfile = dorf;
    contentServerPort = port;
    delay = del;
    id = identifier;
    next = NULL;
}

Request::~Request()
{
    delete [] contentServerAddress;
    delete [] dirorfile;
}

char* Request::getServerAddress()
{
    return contentServerAddress;
}

char* Request::getDirorfile()
{
    return dirorfile;
}

int Request::getServerPort()
{
    return contentServerPort;
}

int Request::getDelay()
{
    return delay;
}

int Request::getID()
{
    return id;
}

Request* Request::getNextRequest()
{
    return next;
}

void Request::setNextRequest(Request* newRequest)
{
    next = newRequest;
}

void Request::print()
{
    cout << "<" << contentServerAddress << ":" << contentServerPort << ":" << dirorfile << ":" << delay << ":" << id << ">\n";
}

RequestList::RequestList()
{
    numOfRequests=0;
    listhead = NULL;
}

RequestList::~RequestList()
{
    this->clear();
}

int RequestList::getNumOfRequests()
{
    return numOfRequests;
}

Request* RequestList::getListHead()
{
    return listhead;
}

bool RequestList::isEmpty()
{
    return (numOfRequests==0);
}

void RequestList::add(Request* addrequest)
{
    addrequest->setNextRequest(this->getListHead());
    listhead = addrequest;
    numOfRequests++;
}

Request* RequestList::removeRequest()
{
    Request* tmpRequest = this->getListHead();
    listhead = tmpRequest->getNextRequest();
    numOfRequests--;
    return tmpRequest;
}

Request* RequestList::getRequest(int identifier)
{
    Request* current = this->getListHead();
    while(current != NULL)
    {
        if (current->getID() == identifier)
            return current;
        current = current->getNextRequest();
    }
    return NULL;
}

void RequestList::clear()
{
    Request* tmpRequest;
    while(this->getNumOfRequests()>0)
    {
        tmpRequest = removeRequest();
        delete tmpRequest;
    }
}

void RequestList::print()
{
    int counter=0;
    Request* current = getListHead();
    cout << "Printing RequestList" << endl;
    while(current != NULL)
    {
        cout << "Request " << counter << ": ";
        current->print();
        current = current->getNextRequest();
        counter++;
    }
}
