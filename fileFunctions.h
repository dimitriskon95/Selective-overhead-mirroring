#ifndef Included_fileFunctions_H
#define Included_fileFunctions_H

class FileNode
{
    private:
        char* dirorfile;
        bool type;
        FileNode* next;
    public:
        FileNode(char*,bool);
        ~FileNode();
        char* getDirorfile();
        bool isFile();
        FileNode* getNext();
        void setNext(FileNode*);
};

class FileList
{
    private:
        int listSize;
        FileNode* listhead;
    public:
        FileList();
        ~FileList();
        int getListSize();
        void addNode(FileNode*);            //add a request at the start of the list
        FileNode* getListHead();
        FileNode* removeNode();            //add a request at the start of the list
        void print();
};

#endif
