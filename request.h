#ifndef Included_Request_H
#define Included_Request_H

class Request
{
    private:
        char* contentServerAddress;
        char* dirorfile;
        int contentServerPort;
        int delay;
        int id;

        Request* next;
    public:
        Request(char*, int, char*, int, int);
        ~Request();

        char* getServerAddress();
        char* getDirorfile();
        int getServerPort();
        int getDelay();
        int getID();

        Request* getNextRequest();
        void setNextRequest(Request*);

        void print();
};

class RequestList
{
    private:
        int numOfRequests;
        Request* listhead;
    public:
        RequestList();
        ~RequestList();

        int getNumOfRequests();
        bool isEmpty();
        Request* getListHead();

        void add(Request*);            //add a request at the start of the list
        Request* removeRequest();      //remove the first element of the list
        Request* getRequest(int);      //search for a specific Request
        void clear();
        void print();
};

#endif
