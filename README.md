# tonyReactor

tonyReactor实现了对epoll模型的封装，使用面向对象的思想来建立一种服务器模型。
使用方法简单，只需要自定义一个继承Protocol的类即可，在继承类中，需要重写三个函数：

virtual void dataReceived(std::string data);	//当服务器接受到data时的动作
virtual void connectionMade() ;			//服务器建立连接时的动作
virtual void connectionLost() ;			//服务器断开连接时的动作


tonyReactor还实现了主动断开连接的功能，只需要调用
closeConnect()函数即可。

具体的例子见sample/main.cpp
编译main.cpp方法：./make.sh


