#include "log.h"
#include "easylogging++.h"
#include<cstdio>

INITIALIZE_EASYLOGGINGPP



void initLog(){
	el::Configurations conf("../sample/log.conf");
	el::Loggers::reconfigureAllLoggers(conf);
}

void warnLog(const char* s){
	LOG(WARNING)<<s;
}

void errorLog(const char* s){
	LOG(ERROR) <<s;
	perror(s);
}
void debugLog(const char* s){
	LOG(DEBUG) <<s;
}
void infoLog(const char* s){
	LOG(INFO) <<s;
}
