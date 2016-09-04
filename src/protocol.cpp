#include "protocol.h"
#include <cstring>
#include<unistd.h>
#include<errno.h>
#include<string>
//#include "timer.h"

Timer* Protocol::timer = NULL;

void Protocol::callLater(callbackPtr callback, void* arg, time_t seconds){
	Protocol::timer->registerTimerEvent(callback, arg, seconds);
	return;
}


int Protocol::getIndex(){
	return this->index;
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
	EPOLL_OUT_OP = 0;
	writeBuf = "";
}

Protocol::~Protocol(){
	return;
}