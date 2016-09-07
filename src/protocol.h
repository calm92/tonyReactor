#ifndef PROTOCOL_H
#define PROTOCOL_H
#include<string>
#include<time.h>

#include"timer.h"

class Protocol{
public:
	int epfd;
	int threadIndex;
	int socketfd;	//protocol所表示的connection的socket
	int EPOLL_OUT_OP;
	int isclosed;	
	int index;	//Protocol实例在数组中的下标
	unsigned short clientPort;
	std::string clientHost;
	static Timer* timer;
	virtual void dataReceived(std::string data) = 0;
	virtual void connectionMade() = 0;
	virtual void connectionLost() = 0;
	
	//可调用函数
	void closeConnect();
	void writeToClient(std::string data);
	void callLater(callbackPtr, void*, time_t );

	void init();
	int epollWrite();
	int getIndex();

	Protocol();
	~Protocol();
private:
	std::string writeBuf;
	
};

#endif

