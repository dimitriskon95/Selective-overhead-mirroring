#ifndef Included_MirrorServer_H
#define Included_MirrorServer_H

#include "request.h"
#include "Queue.h"

class Request;

extern pthread_cond_t cond_alldone;
extern pthread_cond_t cond_nonempty;

class MirrorServer
{
    private:
        int port;
        int workers;
        int managers;
        int NumOfThreads;
        char* dirname;

        int bytesTransferred;
        int filesTransferred;
        int *bytesperDevice;

        RequestList* requestList;
        Queue* recordQueue;
    public:
	      MirrorServer(int,int,char*);
	      ~MirrorServer();

        int getPort();
        int getNumOfThreads();
        int get_BytesTransferred();
        int get_FilesTransferred();
        void increase_FilesTransferred(int, int);
        double calculate_Average();
        double calculate_Variance(double);

        void masterThread();

        bool initiate_Mirroring(int);
        void terminate_Workers();
        void terminate_Mirroring(int);

        Request* create_Request(char*);
        Record* create_Record(char*, int, char*, int, char*);

        char set_DirorfileName(char*&, char*);
        bool match_Path(char*, char*);

        void fetch_File(int, Record*);
        char* create_Filename(Record*);
        void create_Directory(Record*);
};

struct threadData
{
    Queue* queue;
    Request* request;
    MirrorServer* server;
    int initSocket;
};

#endif
