# In  order  to  execute  this  "Makefile " just  type  "make "
OBJS1    = MirrorInitiator.o readwriteFunctions.o
OBJS2    = mainMirrorServer.o MirrorServer.o readwriteFunctions.o request.o Queue.o
OBJS3    = mainContentServer.o ContentServer.o readwriteFunctions.o fileFunctions.o
SOURCE   = MirrorInitiator.cpp mainMirrorServer.cpp MirrorServer.cpp readwriteFunctions.cpp request.cpp mainContentServer.cpp ContentServer.cpp fileFunctions.cpp Queue.cpp
HEADER1  = readwriteFunctions.h
HEADER2  = MirrorServer.h readwriteFunctions.h request.h Queue.h
HEADER3  = ContentServer.h readwriteFunctions.h fileFunctions.h
OUT      = initiator server contentserver
CC       = g++
FLAGS    = -g -c
# -g  option  enables  debugging  mode
# -c flag  generates  object  code  for  separate  files
all: initiator server contentserver

#  create/ compile  the  individual  files  >> separately <<
initiator: $(OBJS1)
	$(CC) -g $(OBJS1) -o initiator
server: $(OBJS2)
	$(CC) -g $(OBJS2) -o server -pthread
contentserver: $(OBJS3)
	$(CC) -g $(OBJS3) -o contentserver -pthread

MirrorInitiator.o: MirrorInitiator.cpp $(HEADER1)
	$(CC) $(FLAGS) MirrorInitiator.cpp

mainMirrorServer.o: mainMirrorServer.cpp $(HEADER2)
	$(CC) $(FLAGS) mainMirrorServer.cpp

MirrorServer.o: MirrorServer.cpp MirrorServer.h
	$(CC) $(FLAGS) MirrorServer.cpp

readwriteFunctions.o: readwriteFunctions.cpp readwriteFunctions.h
	$(CC) $(FLAGS) readwriteFunctions.cpp

request.o: request.cpp request.h
	$(CC) $(FLAGS) request.cpp

Queue.o: Queue.cpp Queue.h
	$(CC) $(FLAGS) Queue.cpp

mainContentServer.o: mainContentServer.cpp $(HEADER3)
	$(CC) $(FLAGS) mainContentServer.cpp

ContentServer.o: ContentServer.cpp ContentServer.h
	$(CC) $(FLAGS) ContentServer.cpp

fileFunctions.o: fileFunctions.cpp fileFunctions.h
	$(CC) $(FLAGS) fileFunctions.cpp

#  clean  house
clean :
	rm -f $(OBJS1) $(OBJS2) $(OBJS3) $(OUT)
