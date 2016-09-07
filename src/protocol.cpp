#include "protocol.h"
#include <cstring>
#include<unistd.h>
#include<errno.h>
#include<string>
#include"base.h"
#include<sys/epoll.h>
#include"log.h"
//#include "timer.h"

Timer* Protocol::timer = NULL;

void Protocol::callLater(callbackPtr callback, void* arg, time_t seconds){
	Protocol::timer->registerTimerEvent(callback, arg, seconds);
	return;
}


int Protocol::getIndex(){
	return this->index;
}

int Protocol::epollRead(){
	int n = 0;
	std::string data;
	char buf[BUFSIZE];
	memset((void*)buf, 0, BUFSIZE);
	while(1){
		n = read(socketfd,  buf, BUFSIZE -1);
		if(n < 0){
			if(errno == EAGAIN){
				dataReceived(data);
				if(EPOLL_OUT_OP == 1){
					//需要向客户端写
					struct  epoll_event event;
					event.data.ptr = (void*)this;
					event.events = EPOLLIN | EPOLLET | EPOLLOUT;
					epoll_ctl(epfd, EPOLL_CTL_MOD, this->socketfd, &event);
				}
				//debug
				return 0;
				// checkProtocolClose(eventPro);
				// break;
			}
			else if (errno == EINTR)
				continue;
			else{
				errorcode = READ_ERROR;
				sprintf(buf, "[thread:%d] client[%s:%d] read error",threadIndex,clientHost.data(), clientPort);
				errorLog(buf);
				return -1;
				// closeConnect(eventPro);
				// break;
			}

		}
		else if (n == 0){
			//client close the connection
			if (!data.empty())
				this->dataReceived(data);
			return -1;
			// closeConnect(eventPro);
			// break;
		}
		else{
			buf[n] = '\0';
			data += buf;
		}
	}
}

int Protocol::epollWrite(){
	int fd = socketfd;
	const char *buf = writeBuf.data();
	int size =strlen(buf);
	int writeNum = 0;
	while(1){
		if(size == 0){
			EPOLL_OUT_OP = 0;
			writeBuf = "";
			return 0;
		}
		int n = write(fd, buf,  size);
		if(n < 0){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				writeBuf.erase(0, writeNum);
				return 0;
			}
			else
				return -1;
		}
		else{
			writeNum += n;
			buf += writeNum;
			size -= writeNum;
		}
	}
}


void Protocol::writeToClient(std::string  data){
	writeBuf += data;
	EPOLL_OUT_OP = 1;
	return;
}


// void Protocol::writeToClinet(const char* str){
// 	writeBuf += str;
// 	EPOLL_OUT_OP = 1;
// 	return;
// }



void Protocol::closeConnect(){
	isclosed = 1;
	return;
}

Protocol::Protocol(){
	init();
	return;
}

void Protocol::init(){
	isclosed = 0;
	index = -1;
	socketfd = -1;
	epfd = -1;
	EPOLL_OUT_OP = 0;
	writeBuf = "";
}

Protocol::~Protocol(){
	return;
}