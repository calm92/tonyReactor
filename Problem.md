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

使用hash表依旧会有问题，因为是在子线程中，先close掉fd连接，此时会有新连接进来，fd会被新连接使用，此时，会更新hash表，导致hash表对应的连接是新的index，而不是老的index,导致资源回收发生错误。

解决方案：
１.当主线程收到关闭连接的请求时，执行连接池的资源回收，完成之后，给子线程socket发送成功标示。
　子线程发送关闭连接请求之后，阻塞读，当读到主线程发送的成功标示之后，再关闭连接。这样,fd就会在资源回收之前，一直占用，不会导致上面的hash表不一致问题。
２.由于程序在同一台主机上运行，所以子线程可以直接关闭连接，然后给主线程发送连接所对应的protocol的地址，然后直接关闭主线程连接，继续执行任务。主线程得到protocol的地址之后，得到对应的protocol，然后进行资源回收，这样就能准确定位protocol.

最后采用方案２，方案１的问题在于子线程会阻塞住，会很大的影响效率。

