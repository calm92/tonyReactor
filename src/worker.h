#ifndef WORKER_H
#define WORKER_H
#include "protocol.h"
struct threadConnection{
	int epfd;
	int connection;	//连接数量
	int index ;    
	threadConnection():epfd(0), connection(0), index(-1){}

};



void initLocal();
void checkProtocolClose(Protocol* pro);
void closeConnect(Protocol* pro);
void worker_epoll_loop(int epfd);




#endif