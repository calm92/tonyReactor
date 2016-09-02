#include "log.h"
#include "easylogging++.h"
#include<cstdio>

INITIALIZE_EASYLOGGINGPP

void initLog(){
	el::Configurations conf("log.conf");
	el::Loggers::reconfigureAllLoggers(conf);
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
