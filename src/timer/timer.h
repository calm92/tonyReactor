#ifndef TIMER_H
#define TIMER_H

#include<vector>
#include<time.h>
#include "../errorCode.h"

typedef void (*callbackPtr) (void *);

//定时任务结构体
struct timerEvent{
	callbackPtr callback;
	void* arg;
	time_t timestamp;
	int index;
	
	timerEvent();
	void destory();
};



//定时器结构体
class Timer{
public:
	Timer();
	~Timer();
	time_t timer_loop();
	void registerTimerEvent(callbackPtr, void*, time_t);


private:
	std::vector<timerEvent*> timerEvents;		//时间队列，最小堆
	void init();
	int timerEventsCap;
	int timerEventsSize;
	int addTimerEventCap(int);
	timerEvent* top();	//得到时间队列中的顶
	int  pop();	//删除时间队列中的顶
	int addTimerEvent(callbackPtr callback, void* arg, time_t timestamp);		//往时间队列中添加事件
};

#endif



