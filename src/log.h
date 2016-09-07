#ifndef LOG_H
#define LOG_H

//开启线程安全功能
#define ELPP_THREAD_SAFE

void initLog();
void errorLog(const char* s);
void debugLog(const char* s);
void infoLog(const char* s);
void warnLog(const char* s);

#endif