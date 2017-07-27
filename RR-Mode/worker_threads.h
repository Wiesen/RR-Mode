#ifndef WORK_THREAD_H_
#define WORK_THREAD_H_

#include <vector>
#include <boost/thread.hpp>
#include "defines.h"
#include "threadSafe_container.h"

#define DATA_BUFFER_SIZE 2048

typedef struct
{
	char buf[DATA_BUFFER_SIZE];
	unsigned int len;
	int  sfd;
}LOCAL_REV_DATA;

typedef struct {
	int sfd;
}CONN_INFO;

// one loop per thread IO模型
typedef struct {
    pthread_t 			thread_id;        	/* unique ID of this thread */
    struct event_base 	*base;    			/* libevent handle this thread uses */
    struct event 		notify_event;  		/* listen event for notify pipe */  // 与master thread通信的事件
    int 				notify_receive_fd;  /* receiving end of notify pipe */  // 与master thread的读管道
    int 				notify_send_fd;     /* sending end of notify pipe */    // 与master thread的写管道
    CThreadSafeList<CONN_INFO> 	list_conn;	/* queue of new connections to handle */
} LIBEVENT_THREAD;

// conn数据结构，作为缓冲区存储与客户端交互的数据
// 为什么不用evbuffer？而是自己实现字符串
typedef struct{
    int    sfd;     // socket file descriptor
    char*  rBuf;
    int    rlen;
    char*  wBuf;
    int    wlen;
    bool   isVerify;
    LIBEVENT_THREAD *thread; 				/* Pointer to the thread object serving this connection */
}CONN;


class CWorkerThread: boost::noncopyable
{
public:
	CWorkerThread();
	virtual ~CWorkerThread();

public:

	bool InitThreads(struct event_base* main_base);

	void DispatchSfdToWorker(int sfd);

private:

	bool SetupThread(LIBEVENT_THREAD* me);

	static void RegisterThreadInitialized(void);
	static void WaitForThreadRegistration(int nthreads);

	static void ReadPipeCb(int fd, short event, void* arg);
	static CONN*InitNewConn(const CONN_INFO& conn_info, LIBEVENT_THREAD* libevent_thread_ptr);

	static void CreateWorker(void *(*func)(void *), void *arg);
	static void *WorkerLibevent(void *arg);

	static void ClientTcpReadCb(struct bufferevent *bev, void *arg);
	static void ClientTcpErrorCb(struct bufferevent *bev, short event, void *arg);

	/* 为了重复利用连接内存资源 */
	static void InitFreeConns();
	static CONN *GetConnFromFreelist();
	static bool AddConnToFreelist(CONN *conn);
	static void FreeConn(CONN *conn);
	static void CloseConn(CONN *conn, struct bufferevent *bev);

private:

	std::vector<LIBEVENT_THREAD*> vec_libevent_thread_;
	int last_thread_;

	static int init_count_;             // 初始化线程计数
	static pthread_mutex_t 	init_lock_; // 初始化锁
	static pthread_cond_t 	init_cond_; // 初始化条件变量

	static boost::mutex mutex_;
	static std::vector<CONN*> vec_freeconn_;    // 缓存，相当于内存池
	static int freetotal_;  // 缓存total number
	static int freecurr_;   // 缓存当前指向的栈顶

};

#endif /* CTHREAD_H_ */
