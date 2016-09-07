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

************************************
2016.9.7　多线程模型
程序结构改为多线程模型
　　程序采用多线程模型，主线程负责accept新的连接，然后将新的连接分配给各个子线程。子线程负责读写自己所负责的连接。这里为了实现主线程和子线程之间的无锁通信，采用以下方法：
　　主线程建立子线程的epoll文件描述符，然后主线程保存各个子线程的epfd，当有新连接时，主线程从连接池中得到一个可用的Protocol并对它进行初始化，然后选择一个子线程的epfd，为这个epfd中加入监听事件。采用这种机制，对于主线程和子线程的任务分配就可以实现无锁。
　　对于需要关闭连接时，主要进行两个操作：1.close连接。2.进行连接池protocolMaster的资源回收。
　　对于close连接，不存在竞争。但是连接池的资源回收，如果在多个线程中进行，会有竞争存在。所以放弃各个子线程进行连接池的资源回收这一步。
　　考虑统一在主线程中，进行连接池的资源回收和连接池中得到可用连接这一操作，此时，就不会出现竞争。所以子线程关闭连接时，需要通过一定的方法来通知主线程，进行此连接的连接池资源回收操作。这里不能用消息队列等需要锁的方式。考虑到主线程和子线程都具有网络通信，而且都是使用epoll，所以采用一下方法：
　　在主线程中，监听两个端口，一个为外部客户端的连接监听端口，另一个则为主线程监听子线程的消息的端口。当子线程需要关闭一个连接时，子线程进行close操作之后，向主线程监听的端口发送一个protocol(连接实例)的index，则主线程可以通过这个index来找到对应的连接实例，从而进行连接池的资源回收。考虑到同一主机之间进行socket通信是不走网卡的，所以效率肯定会高于有锁模型。
　　通过上述方法，就可以做到多线程的无锁模型。
　　
遇到的问题：
1.在多线程模型中，刚开始采用子线程向主线程采用index的方法，来进行连接的关闭。但是后来查看warnninglog，发现出现大量的Protocol的index错误，即子线程发送的index很多都都超过了poolsize。通过查看日志，发现子线程发送的index数据没有问题。问题出在多线程同步上。主线程对资源进行回收时，有swap的操作，然后在对protocol中的index进行重新赋值。详见代码：
```
protocolPool[index]->init();
std::swap(protocolPool[index],    protocolPool[poolSize-1]);
protocolPool[index]->index = index;
poolSize--;
```

当子线程关闭的是index = poolSize-1的连接时，可能主线程随后进行了交换操作，所以子线程发送的index=poolSize-1,而主线程中，这个index就是一个越界的index,从而导致了错误。
解决方案：index会变，但是fd是不会变化的。所以考虑在主线程中，维护一个hash表，将fd和index进行一个映射，这样，当主线程进行资源回收的时候，只需要改变hash[fd]的值就可以找到对应的index,从而完成回收。而子线程由发送index转为发送fd.避免同步的问题。
