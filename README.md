# tonyReactor

tonyReactor实现了对epoll模型的封装，使用面向对象的思想来建立一种服务器模型。
使用方法简单，只需要自定义一个继承Protocol的类即可，在继承类中，需要重写三个函数：

virtual void dataReceived(std::string data);	//当服务器接受到data时的动作
virtual void connectionMade() ;			//服务器建立连接时的动作
virtual void connectionLost() ;			//服务器断开连接时的动作


tonyReactor还实现了主动断开连接的功能，只需要调用
closeConnect()函数即可。
向客户端发送数据：
writeToClient(string data);


************************************
2016.9.4添加新功能：
添加定时器功能：
如果需要添加定时器，只需要在自定义的继承类中，调用：
callLater(callbackPtr callback, void* arg, time_t seconds);
其中：
callbackPtr:   typedef void (*callbackPtr) (void* arg)
arg为函数参数
seconds:为seconds后执行任务callback
为了方便，暂时时间精度只到秒，后面考虑增加时间精度到ms


具体的例子见sample/main.cpp
编译main.cpp方法：./make.sh


