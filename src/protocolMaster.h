#ifndef PROTOCOLMASTER_H
#define PROTOCOLMASTER_H

#include "protocol.h"
#include "errorCode.h"
#include "log.h"
#include<vector>
#include<string>
#include<algorithm>
#define INITIALPOLLSIZE 2



template<class DerivedProtocol>
class ProtocolMaster{
public:
	ProtocolMaster();
	Protocol* getProtocolIns();
	int closeProtocol(int);
	Protocol* getProtocolFromIndex(int);
private:
	int poolSize;
	int poolCap;

	std::vector<Protocol*> protocolPool;
	int addPoolCap(int);	//往连接池中添加元素
};

template<class DerivedProtocol>
Protocol* ProtocolMaster<DerivedProtocol>:: getProtocolFromIndex(int index){
	if(index >= poolSize || index <= 0){
		char buf[256];
		sprintf(buf, "get protocol from index error, the index =%d, the poolSize=%d", index, poolSize);
		warnLog(buf);
		//debug 
		exit(1);
		return NULL;
	}

	return protocolPool[index];
}



template<class DerivedProtocol>
ProtocolMaster<DerivedProtocol>:: ProtocolMaster(){
	poolSize = 0;
	poolCap = 0;
	int ret = addPoolCap(INITIALPOLLSIZE);
	if(ret < 0)
		return ;
	return;
}

template<class DerivedProtocol>
int ProtocolMaster<DerivedProtocol>:: addPoolCap(int count){
	int i;
	for(i=0; i<count; i++){
		Protocol* temp = new (std::nothrow)DerivedProtocol;
		if(temp != NULL)
			protocolPool.push_back(temp);
		else{
			errorLog("new protocol pool fail ");
			break;
		}
	}
	poolCap += i;
	if(i == 0){
		errorcode = NEW_FAIL;
		return -1;
	}
	else
		return i;

}

template<class DerivedProtocol>
Protocol* ProtocolMaster<DerivedProtocol>:: getProtocolIns(){
	if(poolSize == poolCap){
		//连接池满了，需要重新申请
		//新申请的大小为原来的两倍
		int ret = addPoolCap(poolSize);
		if(ret < 0)
			return NULL;
	}

	Protocol* ins =  protocolPool[poolSize];
	ins->Protocol::init();
	ins->index = poolSize;
	poolSize++;
	char buf[1024];
	sprintf(buf, "make the instance of protocol and the instance index = %d,"
			"poolSize = %d, poolCap= %d", ins->index, poolSize,poolCap);
	debugLog(buf);
	return ins;
}

template<class DerivedProtocol>
int ProtocolMaster<DerivedProtocol>::closeProtocol(int index){
	if(index >= poolSize){
		char buf[256];
		sprintf(buf, "close the protocol error the index > poolSize: index= %d, poolSize = %d", index, poolSize);
		errorLog(buf);
		errorcode = CLOSE_PROCOL_FAIL;
		return -1;
	}
	char buf[256];
	sprintf(buf, "before close [protocol:index = %d],  poolSize = %d, poolCap= %d", index, poolSize,poolCap);
	debugLog(buf);

	protocolPool[index]->init();
	std::swap(protocolPool[index],    protocolPool[poolSize-1]);
	protocolPool[index]->index = index;
	poolSize--;

	sprintf(buf, "close the connection  protocol to the pool sucess, poolSize= %d, poolCap=%d", poolSize, poolCap);
	debugLog(buf);
	return 0;
}



#endif