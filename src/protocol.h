#ifndef PROTOCOL_H
#define PROTOCOL_H
#include<string>

class Protocol{
public:
	int socketfd;	//protocol所表示的connection的socket
	int EPOLL_OUT_OP;
	int isclosed;	
	int index;	//Protocol实例在数组中的下标
	unsigned short clientPort;
	std::string clientHost;
	virtual void dataReceived(std::string data) = 0;
	virtual void connectionMade() = 0;
	virtual void connectionLost() = 0;
	
	Protocol();
	~Protocol();
	void closeConnect();
	void writeToClient(std::string data);
	void init();
	int epollWrite();
	int getIndex();
private:
	std::string writeBuf;
	
	
	
	
};

#endif

