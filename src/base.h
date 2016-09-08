

#define INITIALPOLLSIZE 2			//ProtocolMaster中的连接池的初始大小
#define INITIALTImerEventSIZE 2		//定时器事件容器初始大小
#define BACKLOG 4096			//listen函数的backlog队列大小
#define EPOLLSIZE 4096			//epoll_create的size大小
#define MAXEVENTS 4096			//epoll_wait中的maxevents大小
#define BUFSIZE 4096				//
#define EPOLLTIMEOUT 200			//epoll timeout 的初始值,ms
#define CLOSEPORT        54321
#define THREADCOUNT  4