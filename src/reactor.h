#ifndef REACTOR_H
#define REACTOR_H

#include "errorCode.h"
#include "protocol.h"
#include "log.h"
#include "protocolMaster.h"

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

#define BACKLOG 4096
#define EPOLLSIZE 4096
#define MAXEVENTS 4096
#define BUFSIZE 4096
extern errorCode errorcode;



template<class DerivedProtocol>
 class ProtocolMaster;


template<class DerivedProtocol>
class Reactor{
private:
	int listenfd;
	int epfd ;
	struct sockaddr_in  serverAddr;
	ProtocolMaster<DerivedProtocol> *protocolMaster;
	void closeConnect(Protocol*);
	void checkProtocolClose(Protocol*);
	//function
	int epoll_loop();
public:
	Reactor();
	void listenPort(int);
	void run();
	int make_socket_non_blocking(int fd) ;
};

template<class DerivedProtocol>
void Reactor<DerivedProtocol>::checkProtocolClose(Protocol* pro){
	if(pro->isclosed == 1){
		if(pro->EPOLL_OUT_OP == 0)
			closeConnect(pro);
	}
	return;
}


template<class DerivedProtocol>
void Reactor<DerivedProtocol>::closeConnect(Protocol* pro){
	char tempbuf[256];
	sprintf(tempbuf, "client [%s:%d] disconnected and the protocol info:index=%d, fd = %d"\
			,pro->clientHost.data(),pro->clientPort, pro->index, pro->socketfd);
	infoLog(tempbuf);

	struct  epoll_event event;
	event.data.fd = pro->socketfd;
	event.events = 0;
	if(epoll_ctl(epfd, EPOLL_CTL_DEL, pro->socketfd, &event) < 0){
		char buf[256];
		sprintf(buf, "delete the fd from epoll fail and the fd = %d", pro->socketfd);
		errorLog(buf);
	}

	close(pro->socketfd);
	//user function
	pro->connectionLost();
	protocolMaster->closeProtocol(pro->getIndex());

	
}



template<class DerivedProtocol>
int Reactor<DerivedProtocol>::epoll_loop(){
	struct epoll_event listenEvent;
	struct epoll_event *events = (struct epoll_event* )malloc(sizeof(struct epoll_event)*MAXEVENTS);
	struct sockaddr_in clientaddr;
	char* buf = (char*)malloc(BUFSIZE);

	memset((void*)&listenEvent, 0, sizeof(listenEvent) );
	memset((void*)&clientaddr, 0, sizeof(struct sockaddr_in));
	
	epfd =  epoll_create(EPOLLSIZE);
	if(epfd < 0){
		errorLog("create epoll error");
		errorcode = CREATE_EPOLL_FAIL;
		return -1;
	}

	Protocol* lisProtocol = protocolMaster->getProtocolIns();
	if(lisProtocol == NULL)
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

	//epoll_wait
	while(1){
		int n = epoll_wait(epfd, events, MAXEVENTS, 60*1000*60);
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

					if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) < 0){
						errorcode = EPOLL_CTL_ADD_FAIL;
						errorLog("EPOLL_CTL_ADD FAIL");
						
						closeConnect(pro);
						continue;
					}
					//info log
					char tempbuf[256];
					sprintf(tempbuf, "client: [%s:%d] connected and the protocol info: index =%d, fd = %d"\
							,pro->clientHost.data(),pro->clientPort,pro->index, pro->socketfd);
					infoLog(tempbuf);

					pro->connectionMade();
					if(pro->EPOLL_OUT_OP == 1){
						event.data.ptr = (void*)pro;
						event.events = EPOLLIN | EPOLLET | EPOLLOUT;
						epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
					}
					checkProtocolClose(pro);

				}
			}
			else{
				//错误事件
				 if ( (events[i].events & EPOLLRDHUP) ||
					(events[i].events & EPOLLERR)	    ||
					(events[i].events & EPOLLHUP)	)
				{
					//ERROR ,关闭连接，返回资源
					errorcode = CONNECT_ERROR;
					errorLog("connect fail");
					//关闭连接并且返还pro资源,由于单进程不用EPOLL_CTL_DEL
					//因为close一个fd,fd的引用计数会从１->0,从而epoll自动删除fd
					closeConnect(eventPro);
					continue;
				}

				//读事件
				if(events[i].events & EPOLLIN){
					int n = 0;
					std::string data;
					while(1){
						n = read(eventPro->socketfd,  buf, MAXEVENTS-1);
						if(n < 0){
							if(errno == EAGAIN){
								eventPro->dataReceived(data);
								if(eventPro->EPOLL_OUT_OP == 1){
									struct  epoll_event event;
									event.data.ptr = (void*)eventPro;
									event.events = EPOLLIN | EPOLLET | EPOLLOUT;
									epoll_ctl(epfd, EPOLL_CTL_MOD, eventPro->socketfd, &event);
								}
								checkProtocolClose(eventPro);
								break;
							}
							else if (errno == EINTR)
								continue;
							else{
								errorcode = READ_ERROR;
								char buf[256];
								sprintf(buf, "read error,and the data = %s", data.data());
								errorLog(buf);
								closeConnect(eventPro);
								break;
							}

						}
						else if (n == 0){
							//client close the connection
							if (!data.empty())
								eventPro->dataReceived(data);
							closeConnect(eventPro);
							break;
						}
						else{
							buf[n] = '\0';
							data += buf;
						}
					}

				}

				//写事件
				if(events[i].events & EPOLLOUT){
					int ret = eventPro->epollWrite();
					if(ret < 0){
						errorcode = WRITE_ERRPR;
						errorLog("write error");
						closeConnect(eventPro);
					}
					if(ret == 0){
						//返回0，表示成功
						if(eventPro->EPOLL_OUT_OP == 0){
							struct epoll_event event;
							event.data.ptr = (void*)eventPro;
							event.events = EPOLLIN | EPOLLET;
							epoll_ctl(epfd, EPOLL_CTL_MOD, eventPro->socketfd, &event);
							checkProtocolClose(eventPro);
						}
					}
				}	
			}	
		}
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
Reactor<DerivedProtocol>::Reactor(){
	errorcode = NORMAL;
	listenfd = -1;
	protocolMaster = new ProtocolMaster<DerivedProtocol>;
	initLog();
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

	int flag =1;
	int len = sizeof(int);
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, len) < 0){
		errorLog("setsockopt error");
		exit(1);
	}




	char tempbuf[256];
	sprintf(tempbuf, "listent the port: %d",port);
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

	epoll_loop();
}



#endif