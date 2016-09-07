#ifndef REACTOR_H
#define REACTOR_H

#include "errorCode.h"
#include "protocol.h"
#include "log.h"
#include "protocolMaster.h"
#include "./timer/timer.h"
#include "worker.h"
#include "base.h"

#include<string>
#include<netinet/in.h>
#include<unistd.h>
#include<errno.h>
#include<stdlib.h>
#include<cstring>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<fcntl.h>
#include <arpa/inet.h>
#include<map>
#include<thread>

template<class DerivedProtocol>
 class ProtocolMaster;


template<class DerivedProtocol>
class Reactor{
private:
	int threadCount;
	int listenfd;
	int closefd;	
	int epfd ;
	struct sockaddr_in  serverAddr;
	Timer * timer;
	ProtocolMaster<DerivedProtocol> *protocolMaster;
	std::vector<threadConnection> threadConnections;	 //每一个线程中的连接数,实现简单负载均衡
	void closeConnectProtocol(Protocol*);		//回收Protocol到pool中
	void checkProtocolClose(Protocol*);
	int epoll_loop();
	int make_socket_non_blocking(int fd) ;
	void listenClosePort();
	void initThreadPool();
	threadConnection  getMinConnectionThread();	//获得连接数最小的thread

public:
	Reactor();
	void listenPort(int);
	void run();
	void callLater(callbackPtr, void*, time_t );
};


template<class DerivedProtocol>
void Reactor<DerivedProtocol>::initThreadPool(){
	char buf[256];
	sprintf(buf, "main thread: there are %d thread need to start", threadCount);
	debugLog(buf);
	for(int i=0; i<threadCount; i++){
		int fd =  epoll_create(EPOLLSIZE);
		threadConnections[i].epfd = fd;
		threadConnections[i].connection = 0;
		threadConnections[i].index = i;
		std::thread t(worker_epoll_loop, fd);
		t.detach();
	}
	sprintf(buf, "main thread: start %d worker thread success", threadCount);
	infoLog(buf);
	return;
}


template<class DerivedProtocol>
void Reactor<DerivedProtocol>::callLater(callbackPtr callback, void* arg, time_t seconds){
	timer->registerTimerEvent(callback, arg, seconds);
	return;
}


template<class DerivedProtocol>
void Reactor<DerivedProtocol>::checkProtocolClose(Protocol* pro){
	//服务端主动关闭时，调用
	//需要检查输出缓存是否还有数据，若有，要等到输出缓存没有数据再关闭
	if(pro->isclosed == 1){
		if(pro->EPOLL_OUT_OP == 0)
			closeConnectProtocol(pro);
	}
	return;
}


template<class DerivedProtocol>
void Reactor<DerivedProtocol>::closeConnectProtocol(Protocol* pro){
	char tempbuf[256];
	sprintf(tempbuf, "[main thread] close the connection protocol[%d] to the pool:client [%s:%d] disconnected and the protocol info:index=%d, fd = %d ",
			(int)pro,pro->clientHost.data(),pro->clientPort, pro->index, pro->socketfd);
	infoLog(tempbuf);

	int threadindex = pro->threadIndex;
	threadConnections[threadindex].connection--;
	
	int index = pro->getIndex();
	protocolMaster->closeProtocol(index);
	
	
	//thread info
	memset((void*)tempbuf, 0, sizeof(tempbuf));
	sprintf(tempbuf,"There are %d thread, and each thread has  connection count:\n", THREADCOUNT);

	for(int i=0; i<THREADCOUNT; i++){
		char info[256];
		sprintf(info, "thread: %d, %d\n", i, threadConnections[i].connection);
		strcat(tempbuf, info);
	}
	debugLog(tempbuf);
	return;	
}



template<class DerivedProtocol>
int Reactor<DerivedProtocol>::epoll_loop(){
	struct epoll_event listenEvent, closeEvent;
	struct epoll_event *events = (struct epoll_event* )malloc(sizeof(struct epoll_event)*MAXEVENTS);
	struct sockaddr_in clientaddr;
	char* buf = (char*)malloc(BUFSIZE);
	
	time_t epoll_timeout = EPOLLTIMEOUT;

	memset((void*)&listenEvent, 0, sizeof(listenEvent) );
	memset((void*)&closeEvent, 0, sizeof(closeEvent));
	memset((void*)&clientaddr, 0, sizeof(struct sockaddr_in));
	
	epfd =  epoll_create(EPOLLSIZE);
	if(epfd < 0){
		errorLog("create epoll error");
		errorcode = CREATE_EPOLL_FAIL;
		return -1;
	}

	Protocol* lisProtocol = protocolMaster->getProtocolIns();
	Protocol* closeProtocol = protocolMaster->getProtocolIns();
	if(lisProtocol == NULL || closeProtocol == NULL)
		return -1;
	//添加listenfd 的监听文件
	lisProtocol->socketfd = listenfd;
	listenEvent.data.ptr = (void*)lisProtocol;
	listenEvent.events = EPOLLIN | EPOLLET;
	if(epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &listenEvent) < 0){
		errorcode = EPOLL_CTL_ADD_FAIL;
		errorLog("EPOLL_CTL_ADD fail");
		exit(-1);
	}
	
	//添加closefd的监听文件
	closeProtocol->socketfd = closefd;
	closeEvent.data.ptr = (void*)closeProtocol;
	closeEvent.events = EPOLLIN | EPOLLET;
	if(epoll_ctl(epfd, EPOLL_CTL_ADD, closefd, &closeEvent) < 0){
		errorcode = EPOLL_CTL_ADD_FAIL;
		errorLog("EPOLL_CTL_ADD fail");
		exit(-1);
	}

	//epoll_wait
	while(1){
		int n = epoll_wait(epfd, events, MAXEVENTS, epoll_timeout * 1000);
		for(int i=0; i<n; i++){
			Protocol* eventPro = (Protocol* )events[i].data.ptr;

			//判断是否是监听事件
			if(listenfd == eventPro->socketfd){
				int fd =0;
				struct epoll_event event;
				while(1){
					socklen_t addrlen = sizeof(clientaddr);
					fd = accept(listenfd, (struct sockaddr*)&clientaddr, &addrlen);
					if(fd < 0){
						if((errno == EAGAIN) || (errno == EWOULDBLOCK))
							break;
						else if(errno == EINTR)
							continue;
						else{
							errorcode = ACCEPT_ERROR;
							errorLog("accept");
							break; 
						}
					}
					if ( make_socket_non_blocking(fd) < 0)
						continue;
					Protocol* pro = protocolMaster->getProtocolIns();
					if(pro == NULL)
						break;
					pro->socketfd = fd;
					pro->clientPort = ntohs(clientaddr.sin_port);
					pro->clientHost = inet_ntoa(clientaddr.sin_addr);
					event.data.ptr = (void*)pro;
					event.events = EPOLLIN | EPOLLET;

					//分配描述符
					threadConnection  t = getMinConnectionThread();
					pro->epfd = t.epfd;
					pro->threadIndex = t.index;

					if(epoll_ctl(t.epfd, EPOLL_CTL_ADD, fd, &event) < 0){
						errorcode = EPOLL_CTL_ADD_FAIL;
						errorLog("EPOLL_CTL_ADD FAIL");
						
						closeConnectProtocol(pro);
						continue;
					}
					//添加链接数量
					threadConnections[t.index].connection++;
					//info log
					sprintf(buf, "[main thread] client: [%s:%d] connected and the protocol info: index =%d, fd = %d"
						"and the connection send to the [thread:%d]"
							,pro->clientHost.data(),pro->clientPort,pro->index, pro->socketfd,pro->threadIndex);
					infoLog(buf);
					pro->connectionMade();
					if(pro->EPOLL_OUT_OP == 1){
						event.data.ptr = (void*)pro;
						event.events = EPOLLIN | EPOLLET | EPOLLOUT;
						epoll_ctl(pro->epfd, EPOLL_CTL_MOD, fd, &event);
					}
					
				}
			}

			else if(closefd == eventPro->socketfd){
				//主线程关闭接口，用于多线程的通信。
				while(1){
					socklen_t addrlen = sizeof(clientaddr);
					int fd = accept(closefd, (struct sockaddr*)&clientaddr, &addrlen);
					if(fd < 0){
						
						if((errno == EAGAIN) || (errno == EWOULDBLOCK))
							break;
						else if(errno == EINTR)
							continue;
						else{
							errorcode = ACCEPT_ERROR;
							errorLog("accept");
							break; 
						}
					}
					char readbuf[256];
					memset((void*)readbuf, 0, 256);
					read(fd, readbuf, 256);
					sprintf(buf, "[main thread] close port have the connection request and  the content: %s", readbuf);
					infoLog(buf);
					int addr = 0;
					sscanf(readbuf,"%d", &addr);

					//sprintf(buf, "[main thread] close port get connection fd  to close the [Protocol: fd = %d]", proFd);
					//infoLog(buf);

					//int index = fdHash[proFd];
					//Protocol* closePro = protocolMaster->getProtocolFromIndex(index);
					// if(closePro == NULL){
					// 	warnLog("index is too not right");
					// 	close(fd);
					// 	continue;
					// }

					//debug
					// if(closePro->socketfd != proFd){
					// 	sprintf(buf, "the fdhash is not right");
					// 	warnLog(buf);
					// 	continue;
					// }

					Protocol* closePro = (Protocol*)addr;
					sprintf(buf,"[main thread] close port get the protocol:client[%s:%d] index=%d, threadIndex=%d, fd=%d",
							closePro->clientHost.data(), closePro->clientPort, closePro->index, closePro->threadIndex, closePro->socketfd);
					
					closeConnectProtocol(closePro);
					close(fd);

					continue;
				}
			}
			else{
				sprintf(buf, "[main thread] get the fd is not listenfd or closefd");
				warnLog(buf);
			}	
		}

		//处理超时事件
		epoll_timeout =  timer->timer_loop();
		if(epoll_timeout  ==  0)
			epoll_timeout = EPOLLTIMEOUT;
		sprintf(buf, "the timer loop is done and the epoll_timeout = %ld ", epoll_timeout);
		infoLog(buf);		
	}		
}
	




template<class DerivedProtocol>
int Reactor<DerivedProtocol>::make_socket_non_blocking(int fd){
	int flags, s;
	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		errorLog("fcntl");
		errorcode = SET_SOCK_NOBLOCK_FAIL;
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl(fd, F_SETFL, flags);
	if (s == -1) {
		errorLog("fcntl");
		errorcode = SET_SOCK_NOBLOCK_FAIL;
		return -1;
	}

	return 0;


}

template<class DerivedProtocol>
threadConnection Reactor<DerivedProtocol>:: getMinConnectionThread(){
	threadConnection minThread;
	minThread=threadConnections[0];
	for(int i=1; i<THREADCOUNT; i++){
		if(minThread.connection > threadConnections[i].connection)
			minThread = threadConnections[i];
	}
	return minThread;
}

template<class DerivedProtocol>
Reactor<DerivedProtocol>::Reactor():threadConnections(THREADCOUNT){
	errorcode = NORMAL;
	listenfd = -1;
	closefd = -1;
	protocolMaster = new ProtocolMaster<DerivedProtocol>;
	threadCount = THREADCOUNT;
	initLog();

	//init Timer
	Timer* newTimer = new Timer();
	timer = newTimer;
	Protocol::timer = newTimer;

	//listen the close port
	listenClosePort();
	return;
}

template<class DerivedProtocol>
void Reactor<DerivedProtocol>::listenPort(int port = 5000){
	//init serverAddr
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	//init socket
	listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenfd < 0){
		errorLog("socket error");
		errorcode = SOCKET_FAIL;
		exit(1);
	}

	//address 复用
	int flag =1;
	int len = sizeof(int);
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, len) < 0){
		errorLog("setsockopt error");
		exit(1);
	}


	if (bind(listenfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
		errorLog("bind error");
		errorcode = BIND_FAIL;
		exit(1);
	}

	if (listen(listenfd, BACKLOG) < 0){
		errorLog("listen error");
		errorcode = LISTEN_FAIL;
		exit(1);
	}

	if ( make_socket_non_blocking(listenfd) < 0)
		exit(1);


	char tempbuf[256];
	sprintf(tempbuf, "listent the port: %d",port);
	infoLog(tempbuf);
	return;
}

template<class DerivedProtocol>
void Reactor<DerivedProtocol>::listenClosePort(){
	//init addr
	struct sockaddr_in  closeAddr;
	memset(&closeAddr, 0, sizeof(closeAddr));
	closeAddr.sin_family = AF_INET;
	closeAddr.sin_port = htons(CLOSEPORT);
	closeAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	//init socket
	closefd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (closefd < 0){
		errorLog("socket error");
		errorcode = SOCKET_FAIL;
		exit(1);
	}

	//address 复用
	int flag =1;
	int len = sizeof(int);
	if(setsockopt(closefd, SOL_SOCKET, SO_REUSEADDR, &flag, len) < 0){
		errorLog("setsockopt error");
		exit(1);
	}


	if (bind(closefd, (struct sockaddr*)&closeAddr, sizeof(closeAddr)) < 0){
		errorLog("bind close port error");
		errorcode = BIND_FAIL;
		exit(1);
	}

	if (listen(closefd, BACKLOG) < 0){
		errorLog("listen close port error");
		errorcode = LISTEN_FAIL;
		exit(1);
	}

	if ( make_socket_non_blocking(closefd) < 0)
		exit(1);


	char tempbuf[256];
	sprintf(tempbuf, "listen the close port: %d",CLOSEPORT);
	infoLog(tempbuf);
	return;
}


template<class DerivedProtocol>
void Reactor<DerivedProtocol>::run(){
	if (listenfd < 0){
		errorLog("the reactor do not listen the port");
		errorcode = LISTEN_FAIL;
		exit(1);
	}
	infoLog("the reactor is run");
	initThreadPool();
	epoll_loop();
}



#endif