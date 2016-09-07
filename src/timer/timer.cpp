#include "timer.h"
#include "../errorCode.h"
#include "../log.h"

#include <algorithm>
#include<time.h>

#define INITIALTImerEventSIZE 2

timerEvent::timerEvent(){
	callback = NULL;
	arg = NULL;
	timestamp = 0;
	index = -1;		//感觉好像没用，因为都是从头开始，不用知道它的index,和Protocol不同
	return; 
}

timeoutEvent::timeoutEvent(){
	callback = NULL;
	arg = NULL;
	return;
}

void timerEvent::destory(){
	callback = NULL;
	arg = NULL;
	timestamp = -1;
	index = -1;		
	return; 
}



void Timer::registerTimerEvent(callbackPtr callback, void* arg, time_t seconds){
	addTimerEvent(callback, arg, seconds);
	
}

timerEvent* Timer::top(){
	if(timerEventsSize == 0){
		warnLog("timerEventSize =0, and top the event");
		return NULL;
	}
	else
		return timerEvents[0];
}

int Timer::pop(){
	if(timerEventsSize == 0){
		warnLog("timerEventSize =0, and pop the event");
		return -1;
	}

	timerEventsSize--;
	if(timerEventsSize == 0){
		timerEvents[0]->destory();
		return 0;
	}


	int temp = timerEventsSize;
	std::swap(timerEvents[0],  timerEvents[temp]);
	timerEvents[temp]->destory();

	//调整最小堆
	temp = 0;
	while(temp *2 +1 < timerEventsSize){
		int left = temp * 2 ;
		int right = temp * 2 +1;
		if(timerEvents[temp]->timestamp > timerEvents[left]->timestamp ||
			timerEvents[temp]->timestamp > timerEvents[right]->timestamp){

			if(timerEvents[left]->timestamp < timerEvents[right]->timestamp){
				std::swap(timerEvents[left], timerEvents[temp]);
				timerEvents[temp]->index = temp;
				temp = left;
			}
			else{
				std::swap(timerEvents[right], timerEvents[temp]);
				timerEvents[temp]->index = temp;
				temp = right;
			}
		}

		else
			break;
	}

	timerEvents[temp]->index = temp;
	return 0;
}


time_t Timer::timer_loop(){
	char buf[256];

	mtx.lock();
	sprintf(buf, "start the timer loop, the events size = %d", timerEventsSize);
	debugLog(buf);
	

	if(timerEventsSize == 0){
		//最好不要在中间return，容易造成死锁
		mtx.unlock();
		return 0;
	}
	time_t currentTime= 0;
	time(&currentTime);
	while(timerEventsSize > 0){
		timerEvent*  event = top();
		if(event == NULL)
			break;
		if(event->timestamp > currentTime)
			break;
		//将超时事件加入队列
		timeoutEvent tempevent;
		tempevent.callback = event->callback;
		tempevent.arg = event->arg;
		timeoutEvents.push_back(tempevent);

		sprintf(buf, "start pop the event from the timerEvents, and the timerEventSize=%d", timerEventsSize);
		debugLog(buf);
		pop();
		sprintf(buf, "have done pop the event from the timerEvents, and the timerEventSize=%d", timerEventsSize);
		debugLog(buf);
		continue;
	}
	mtx.unlock();

	

	//执行timeoutevent
	sprintf(buf, "do the timer event, currentTime=%ld", currentTime);
	debugLog(buf);
	int len = timeoutEvents.size();
	for(int i=0; i<len; i++){
		callbackPtr callback = timeoutEvents[i].callback;
		callback(timeoutEvents[i].arg);
	}
	//clear不释放内存，避免重复申请释放内存
	timeoutEvents.clear();

	time(&currentTime);
	mtx.lock();
	time_t seconds = 0;
	if(timerEventsSize == 0)
		seconds = 0;
	else{
		timerEvent* event = timerEvents[0];
		seconds = (event->timestamp - currentTime);
	}
	mtx.unlock();

	return seconds;
}



int Timer::addTimerEventCap(int count){
	int i = 0;
	for(i=0; i<count; i++){
		timerEvent* temp = new(std::nothrow)timerEvent;
		if(temp != NULL)
			timerEvents.push_back(temp);
		else{
			errorLog("new timerEvent fail");
			break;
		}
	}
	timerEventsCap += i;
	char buf[256];
	sprintf(buf, "add the timer event cap and now the cap = %d", timerEventsCap);
	debugLog(buf);

	if(i == 0){
		errorcode = NEW_FAIL;
		return -1;
	}
	else
		return i;
}


int Timer::addTimerEvent(callbackPtr callback, void* arg, time_t seconds){
	mtx.lock();

	if(timerEventsSize  == timerEventsCap){
		int ret = addTimerEventCap(timerEventsSize);
		if(ret < 0){
			mtx.unlock();
			return -1;
		}
	}
	time_t currentTime = 0;
	time(&currentTime);

	timerEvents[timerEventsSize]->callback = callback;
	timerEvents[timerEventsSize]->arg = arg;
	timerEvents[timerEventsSize]->timestamp = seconds + currentTime;
	timerEvents[timerEventsSize]->index = timerEventsSize;

	//插入事件
	int temp = timerEventsSize++;
	while(temp){
		int father = temp / 2;
		if(timerEvents[temp]->timestamp < timerEvents[father]->timestamp){
			std::swap(timerEvents[temp], timerEvents[father]);
			timerEvents[temp]->index = temp;
			timerEvents[father]->index = father;
			temp = father;
			continue;
		}
		else
			break;
	}

	char buf[256];
	sprintf(buf, "register timer event to the timer, and the event info:  "
		"timer_events size = %d, timer_events cap = %d, register event index = %d ", 
		timerEventsSize,timerEventsCap, temp);
	debugLog(buf);

	mtx.unlock();
	return 0;
}

Timer::Timer(){
	init();
}

Timer:: ~Timer(){
}

void Timer::init(){
	addTimerEventCap(INITIALTImerEventSIZE);
	timerEventsCap = INITIALTImerEventSIZE;
	timerEventsSize = 0;
	return;

}