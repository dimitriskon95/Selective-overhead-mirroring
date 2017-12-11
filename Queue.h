#ifndef Included_Queue_H
#define Included_Queue_H

#include <pthread.h>
#include <unistd.h>

extern pthread_cond_t cond_nonempty;
extern pthread_cond_t cond_alldone;

class Record
{
    private:
        char* dirorfilename;
        char* contentServerAddress;
        char* contentServerPath;
        int contentServerPort;
        char type;          //'D' or 'F'
        int requestID;
        Record* next;
    public:
        Record(char*, char*, char*, int, char, int);
        ~Record();

        char* getDirorfilename();
        char* getContentServerAddress();
        char* getContentServerPath();
        int getContentServerPort();
        char getType();
        int getRequestID();

        Record* getNext();
        void setNext(Record*);
        void print();
};

class Queue
{
    private:
        int queueSize;
        Record *headNode;
        Record *lastNode;
        pthread_mutex_t mtx;
    public:
	      Queue();
        ~Queue();

        int getQueueSize();
        Record* getHeadNode();
        Record* getLastNode();

        void push(Record*);
        Record* pop();
        void print();
        void wait_requestCompletion();
        void decrease_RunningWorkers();
};

#endif
