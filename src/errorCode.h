#ifndef 		ERRORCODE_H
#define 	ERRORCODE_H

enum errorCode{
	NORMAL = 0,
	SOCKET_FAIL = -1,
	BIND_FAIL = -2,
	LISTEN_FAIL = -3,
	NEW_FAIL = -4,
	CLOSE_PROCOL_FAIL = -5,
	SET_SOCK_NOBLOCK_FAIL = -6,
	CREATE_EPOLL_FAIL = -7,
	EPOLL_CTL_ADD_FAIL = -8,
	ACCEPT_ERROR = -9,
	CONNECT_ERROR = -10,	//EPOLL_WAIT的fd, 返回错误
	READ_ERROR = -11,
	WRITE_ERRPR = -12,



};


errorCode errorcode = NORMAL;


#endif