#include "reactor.h"
#include "protocol.h"
#include<vector>
#include<iostream>

class myProtocol:public Protocol{
public:
	virtual void dataReceived(std::string data);
	virtual void connectionMade() ;
	virtual void connectionLost() ;
};

void myProtocol::dataReceived(std::string data){
	writeToClient(data);
	closeConnect();
	return;
}

void myProtocol::connectionMade(){
}

void myProtocol::connectionLost(){
}

int main(){
	Reactor<myProtocol> reactor;
	reactor.listenPort(12345);
	reactor.run();

}
