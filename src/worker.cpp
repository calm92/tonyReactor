#include "base.h"
#include "worker.h"
 #include "log.h"

#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<string.h>

static struct sockaddr_in localCloseAddr;

void initLocal(){
	memset((void*) &localCloseAddr, 0, sizeof(localCloseAddr));
	localCloseAddr.sin_family = AF_INET;
	localCloseAddr.sin_port = htons(CLOSEPORT);
	inet_pton(AF_INET, "127.0.0.1", (void*)&localCloseAddr.sin_addr);
	return;
}

void checkProtocolClose(Protocol* pro){
	//服务端主动关闭时，调用
	//需要检查输出缓存是否还有数据，若有，要等到输出缓存没有数据再关闭
	if(pro->isclosed == 1){
		if(pro->EPOLL_OUT_OP == 0)
			closeConnect(pro);
	}
	return;
}

void closeConnect(Protocol* pro){
	char buf[256];
	sprintf(buf, "[thread:%d]start to send to closePort, client [%s:%d] disconnected ,the protocol info:profd=%d",
			pro->threadIndex,pro->clientHost.data(),pro->clientPort, pro->socketfd);
	infoLog(buf);
	//取消连接的文件描述符
	struct  epoll_event event;
	event.data.fd = pro->socketfd;
	event.events = 0;
	if(epoll_ctl(pro->epfd, EPOLL_CTL_DEL, pro->socketfd, &event) < 0){
		sprintf(buf, "[thread:%d] delete the fd from epoll fail and the fd = %d", pro->threadIndex, pro->socketfd);
		errorLog(buf);
	}
	close(pro->socketfd);
	sprintf(buf, "[thread:%d] close the connection[fd:%d] in the epoll success and have send the index to the main thread",
				pro->threadIndex,pro->socketfd);
	infoLog(buf);
	//user function
	pro->connectionLost();

	//向本机关闭端口发送gf
	sprintf(buf, "%d", pro->socketfd);
	int sockfd = socket(AF_INET, SOCK_STREAM,0);
	if( connect(sockfd, (struct sockaddr*)&localCloseAddr, sizeof(struct sockaddr)) < 0){
		char temp[256];
		sprintf(buf, "[thread:%d] connect to the main thred close port error, host:%s, port:%d",pro->threadIndex, 
			inet_ntop(AF_INET,&localCloseAddr.sin_addr, temp, sizeof(temp)), ntohs(localCloseAddr.sin_port));
		errorLog(buf);
		return;
	}
	int n = write(sockfd, buf, strlen(buf));
	if(n < 0){
		sprintf(buf, "[thread:%d] write to the main thread close port error", pro->threadIndex);
		warnLog(buf);
	}
	close(sockfd);
	
}


void worker_epoll_loop(int epfd){
	initLocal();
	struct epoll_event *events = (struct epoll_event* )malloc(sizeof(struct epoll_event)*MAXEVENTS);
	char* buf = (char*)malloc(BUFSIZE);
	time_t epoll_timeout = EPOLLTIMEOUT;

	sprintf(buf, "[thread:%d] thread start and be looping", epfd);
	infoLog(buf);
	while(1){
		int n = epoll_wait(epfd, events, MAXEVENTS, epoll_timeout * 1000);
		for(int i=0; i<n; i++){
			Protocol* eventPro = (Protocol* )events[i].data.ptr;
			//错误事件
			if ( (events[i].events & EPOLLRDHUP) ||
				(events[i].events & EPOLLERR)||
				(events[i].events & EPOLLHUP)	)
			{
				//ERROR ,关闭连接，返回资源
				errorcode = CONNECT_ERROR;
				sprintf(buf, "[thread:%d] client[%s:%d] connect fail",eventPro->threadIndex, eventPro->clientHost.data(), eventPro->clientPort);
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
					n = read(eventPro->socketfd,  buf, BUFSIZE -1);
					if(n < 0){
						if(errno == EAGAIN){
							eventPro->dataReceived(data);
							if(eventPro->EPOLL_OUT_OP == 1){
								//需要向客户端写
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
							sprintf(buf, "[thread:%d] client[%s:%d] read error",eventPro->threadIndex,eventPro->clientHost.data(), eventPro->clientPort);
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
					sprintf(buf, "[thread:%d] client[%s:%d] read error",eventPro->threadIndex,eventPro->clientHost.data(), eventPro->clientPort);
					errorLog(buf);
					closeConnect(eventPro);
				}
				if(ret == 0){
					//返回0，表示成功
					if(eventPro->EPOLL_OUT_OP == 0){
						struct epoll_event event;
						event.data.ptr = (void*)eventPro;
						event.events = EPOLLIN | EPOLLET;
						epoll_ctl(epfd, EPOLL_CTL_MOD, eventPro->socketfd, &event);
						//检查服务端是否主动关闭连接
						checkProtocolClose(eventPro);
					}
				}
			}	
		}	
	}

}


