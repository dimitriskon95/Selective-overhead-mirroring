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

#include "fileFunctions.h"

using namespace std;

FileNode::FileNode(char* df, bool tp)
{
    dirorfile = df;
    type = tp;
    next = NULL;
}

FileNode::~FileNode()
{
    delete [] dirorfile;
}

char* FileNode::getDirorfile()
{
    return dirorfile;
}

bool FileNode::isFile()
{
    return type;
}

FileNode* FileNode::getNext()
{
    return next;
}

void FileNode::setNext(FileNode* newnode)
{
    next = newnode;
}

FileList::FileList()
{
    listSize=0;
    listhead = NULL;
}

FileList::~FileList()
{
    FileNode *delnode;
    while(listSize > 0)
    {
        delnode = this->removeNode();
        if (delnode != NULL)
            delete delnode;
    }
}

int FileList::getListSize()
{
    return listSize;
}

FileNode* FileList::getListHead()
{
    return listhead;
}

void FileList::addNode(FileNode* node)
{
    node->setNext(this->getListHead());
    listhead = node;
    listSize++;
}

FileNode* FileList::removeNode()
{
    FileNode* tmpNode = this->getListHead();
    listhead = tmpNode->getNext();
    listSize--;
    return tmpNode;
}

void FileList::print()
{
    FileNode* current = this->getListHead();
    while(current != NULL)
    {
        if (current->isFile())
            cout << "File      :" << current->getDirorfile() << endl;
        else
            cout << "Directory :" << current->getDirorfile() << endl;
        current = current->getNext();
    }
}
