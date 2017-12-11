#ifndef Included_ContentServer_H
#define Included_ContentServer_H

#include "fileFunctions.h"

struct delayNode
{
    int nodeID;
    int delayTime;
    struct delayNode *next;
};

class ContentServer
{
    private:
        char* dirorfilename;
        int port;
        struct delayNode *dnode;
        pthread_mutex_t mtx;
    public:
	      ContentServer(int,char*);
	      ~ContentServer();

        char* getDirorfile();
        int getPort();

        void masterThread();

        int parseCommand(char*, char*&);
        void add_dir_content(char*,FileList*);

        void add_delayNode(struct delayNode*);
        int getDelayTime(int);

};

struct threadContentData
{
    int sock;
    char* command;
    ContentServer* server;
};

#endif
