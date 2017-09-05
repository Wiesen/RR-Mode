#include "worker_threads.h"
#include "global_settings.h"
#include "utils.h"
#include "socket_wrapper.h"

int CWorkerThread::init_count_ = 0;
pthread_mutex_t	CWorkerThread::init_lock_ = PTHREAD_MUTEX_INITIALIZER;
// 该常量负责初始化静态分配（全局）的条件变量，使其具有缺省属性；pthread_cond_init函数负责初始化动态分配的条件变量
pthread_cond_t  CWorkerThread::init_cond_ = PTHREAD_COND_INITIALIZER;

int CWorkerThread::freetotal_ = 0;
int CWorkerThread::freecurr_  = 0;
boost::mutex CWorkerThread::mutex_;
std::vector<CONN*> CWorkerThread::vec_freeconn_;


CWorkerThread::CWorkerThread()
{
	last_thread_ = -1;
}

CWorkerThread::~CWorkerThread()
{
}

/* 初始化worker线程池 */
bool CWorkerThread::InitThreads(struct event_base* main_base)
{

	InitFreeConns();

	LOG4CXX_INFO(g_logger, "Initializes worker threads...");

	for(unsigned int i=0; i<utils::G<CGlobalSettings>().thread_num_; ++i)
	{
		LIBEVENT_THREAD* libevent_thread_ptr = new LIBEVENT_THREAD;
		/* 建立每个worker线程和主监听线程通信的管道 */
		int fds[2];
		if (pipe(fds) != 0)	// 创建：int pipe(int fd[2])
		{
			LOG4CXX_ERROR(g_logger, "CThread::InitThreads:Can't create notify pipe");
			return false;
		}
		libevent_thread_ptr->notify_receive_fd = fds[0]; // fd[0]为读而打开
		libevent_thread_ptr->notify_send_fd	   = fds[1]; // fd[1]为写而打开
		if(!SetupThread(libevent_thread_ptr))
		{
			utils::SafeDelete(libevent_thread_ptr);
			LOG4CXX_ERROR(g_logger, "CThread::InitThreads:SetupThread failed.");
			return false;
		}
		vec_libevent_thread_.push_back(libevent_thread_ptr);
	}

	for (unsigned int i = 0; i < utils::G<CGlobalSettings>().thread_num_; i++)
	{
		CreateWorker(WorkerLibevent, vec_libevent_thread_.at(i));
	}

	 /* 等待所有线程都已经启动完毕. */
	WaitForThreadRegistration(utils::G<CGlobalSettings>().thread_num_);

	LOG4CXX_INFO(g_logger, "Create threads success. we hava done all the libevent setup.");

	return true;
}

void CWorkerThread::CreateWorker(void *(*func)(void *), void *arg)
{
	pthread_t thread;
	pthread_attr_t attr;
	int ret;

    // 属性结构为pthread_attr_t，它同样在头文件pthread.h中定义，属性值不能直接设置，须使用相关函数进行操作
    // 初始化的函数为pthread_attr_init，这个函数必须在pthread_create函数之前调用。属性对象主要包括是否绑定、是否分离等
    // 调用pthread_attr_init之后，pthread_attr_t结构所包含的内容就是操作系统实现支持的线程所有属性的默认值
    // 需要用pthread_attr_destroy函数对其去除初始化
	pthread_attr_init(&attr);

    // 线程创建即运行，但不能保证哪个线程先运行：是新创建的线程、还是调用线程
	if ((ret = pthread_create(&thread, &attr, func, arg)) != 0)
	{
		LOG4CXX_FATAL(g_logger, "CWorkerThread::CreateWorker:Can't create thread:" << strerror(ret));
		exit(1);
	}
}


void *CWorkerThread::WorkerLibevent(void *arg)
{
	LIBEVENT_THREAD *me = static_cast<LIBEVENT_THREAD *>(arg);

	me->thread_id = pthread_self(); // 获得线程自身id

	RegisterThreadInitialized();

	event_base_dispatch(me->base);

	return NULL;
}

bool CWorkerThread::SetupThread(LIBEVENT_THREAD* me)
{
    // 为每个worker线程创建event_base
	me->base = event_base_new();
	assert(me != NULL);

	/* 通过每个worker线程的读管道监听来自master的通知 */
	me->notify_event = *event_new(me->base, me->notify_receive_fd, EV_READ|EV_PERSIST, ReadPipeCb, (void*)me);
	assert(&me->notify_event != NULL);

	// 非未决的事件上调用event_add()将使其在配置的event_base中成为未决的
	// 成功时函数返回0，失败时返回-1。如果tv为NULL，添加的事件不会超时
	if (event_add(&me->notify_event, NULL) == -1)
	{
		int error_code = EVUTIL_SOCKET_ERROR();
		LOG4CXX_ERROR(g_logger, "CWorkerThread::SetupThread:event_add errorCode = " << error_code
								<< ", description = " << evutil_socket_error_to_string(error_code));
		return false;
	}

	return true;
}

void CWorkerThread::ReadPipeCb(int fd, short event, void* arg)
{

	LIBEVENT_THREAD *libevent_thread_ptr = static_cast<LIBEVENT_THREAD*>(arg);
	assert(libevent_thread_ptr != NULL);

	/* read from master-thread had write, a byte 代表一个客户端连接 */
	char buf[1];
	if (read(fd, buf, 1) != 1)
	{
		LOG4CXX_ERROR(g_logger, "CWorkerThread::ThreadLibeventProcess:Can't read from libevent pipe.");
		return;
	}

	/* 将主线程塞到队列中的连接pop出来 */
	CONN_INFO connInfo;
	if(!libevent_thread_ptr->list_conn.pop_front(connInfo))
	{
		LOG4CXX_ERROR(g_logger, "CWorkerThread::ThreadLibeventProcess:list_conn.pop_front NULL.");
		return;
	}

	/*初始化新连接，将连接事件注册入libevent */
	if(connInfo.sfd != 0)
	{
		CONN* conn = InitNewConn(connInfo, libevent_thread_ptr);
		if(NULL == conn)
		{
			LOG4CXX_ERROR(g_logger, "CWorkerThread::ReadPipeCb:Can't listen for events on sfd = " << connInfo.sfd);
			close(connInfo.sfd);
		}
		LOG4CXX_TRACE(g_logger, "CWorkerThread::ReadPipeCb thread id = " << conn->thread->thread_id);
	}
}

CONN* CWorkerThread::InitNewConn(const CONN_INFO& conn_info, LIBEVENT_THREAD* libevent_thread_ptr)
{
	CONN* conn = GetConnFromFreelist();
	if (NULL == conn)
	{
		conn = new CONN;
		if (NULL == conn)
		{
			LOG4CXX_ERROR(g_logger, "CWorkerThread::InitNewConn:new conn error.");
			return NULL;
		}

		try
		{
			conn->rBuf = new char[DATA_BUFFER_SIZE];
			conn->wBuf = new char[DATA_BUFFER_SIZE];
		} catch (std::bad_alloc &)
		{
			FreeConn(conn);
			LOG4CXX_ERROR(g_logger, "CWorkerThread::InitNewConn:new buf error.");
			return NULL;
		}
	}

	conn->sfd = conn_info.sfd;
	conn->rlen = 0;
	conn->wlen = 0;
	conn->thread = libevent_thread_ptr;

	/* 将新连接加入此线程libevent事件循环 */
	int flag = EV_READ | EV_PERSIST;    // 标记写事件并持久化
	struct bufferevent *client_tcp_event = bufferevent_socket_new(libevent_thread_ptr->base, conn->sfd, BEV_OPT_CLOSE_ON_FREE); // 创建基于套接字的bufferevent
	if (NULL == client_tcp_event)
	{
		if(!AddConnToFreelist(conn))
		{
			FreeConn(conn);
		}
		int error_code = EVUTIL_SOCKET_ERROR();
		LOG4CXX_ERROR(g_logger,
				"CWorkerThread::conn_new:bufferevent_socket_new errorCode = " << error_code << ", description = " << evutil_socket_error_to_string(error_code));

		return NULL;
	}
    // bufferevent_setcb()函数修改bufferevent的一个或者多个回调（读、写、事件/错误）；禁用回调，传递NULL而不是回调函数
    // 注意：bufferevent的所有回调函数共享单个cbarg，所以修改它将影响所有回调函数
	bufferevent_setcb(client_tcp_event, ClientTcpReadCb, NULL, ClientTcpErrorCb, (void*) conn);

	/* 利用客户端心跳超时机制处理半开连接 */
	struct timeval heartbeat_sec;   // 使用timeval结构体设置超时，单位为s
	heartbeat_sec.tv_sec = utils::G<CGlobalSettings>().client_heartbeat_timeout_;
	heartbeat_sec.tv_usec= 0;
	bufferevent_set_timeouts(client_tcp_event, &heartbeat_sec, NULL);
    // bufferevent_socket_connect可以启动连接，常用于客户端
	bufferevent_enable(client_tcp_event, flag); // 可以启用或者禁用bufferevent上的EV_READ、 EV_WRITE或者EV_READ|EV_WRITE事件

	return conn;
}


void CWorkerThread::ClientTcpReadCb(struct bufferevent *bev, void *arg)
{
	CONN* conn = static_cast<CONN*>(arg);
	assert(conn != NULL);

	int recv_size = 0;
	if ((recv_size = bufferevent_read(bev, conn->rBuf + conn->rlen, DATA_BUFFER_SIZE - conn->rlen)) > 0)
	{
		conn->rlen = conn->rlen + recv_size;
		//防止恶意连接，进行token校验，不满足校验条件的为恶意连接，直接关闭
		if (conn->rlen >= TOKEN_LENGTH && conn->isVerify == false)
		{
			conn->isVerify = true;
			std::string str_verify(conn->rBuf, TOKEN_LENGTH);
			if (str_verify.compare(std::string(TOKEN_STR)) != 0)
			{
				LOG4CXX_WARN(g_logger, "CWorkerThread::ClientTcpReadCb DDOS. str = " << str_verify);
				CloseConn(conn, bev);
				return;
			} else
			{
				conn->rlen = conn->rlen - TOKEN_LENGTH;
				memmove(conn->rBuf, conn->rBuf + TOKEN_LENGTH, conn->rlen);
			}
		}
	}

	std::string str_recv(conn->rBuf, conn->rlen);
	if (utils::FindCRLF(str_recv))
	{
		/* 有可能同时收到多条信息 */
		std::vector<std::string> vec_str;
		utils::SplitData(str_recv, CRLF, vec_str);

		for (unsigned int i = 0; i < vec_str.size(); ++i)
		{
            // 直接写回给client，该程序并没有复杂的应用层交互，因此没有使用到写事件
            // 如果socket的写缓冲区是满的，则write(sock, buffer, strlen(buffer))会阻塞，进而导致整个on_read()方法阻塞，而无法读到接下来client传过来的数据了
            // 用了libevent的”写事件“之后,socket缓冲区不可以写就不会触发这个“写事件”,程序就不会阻塞
			if(!SocketOperate::WriteSfd(conn->sfd, vec_str.at(i).c_str(), vec_str.at(i).length()))
			{
				LOG4CXX_ERROR(g_logger, "CWorkerThread::ClientTcpReadCb:send sfd .error = " << strerror(errno));
			}
		}

		int len = str_recv.find_last_of(CRLF) + 1;
		memmove(conn->rBuf, conn->rBuf + len, DATA_BUFFER_SIZE - len);
		conn->rlen = conn->rlen - len;
	}
}

void CWorkerThread::ClientTcpErrorCb(struct bufferevent *bev, short event, void *arg)
{
	CONN* conn = static_cast<CONN*>(arg);

	if (event & BEV_EVENT_TIMEOUT)
	{
		LOG4CXX_WARN(g_logger, "CWorkerThread::ClientTcpErrorCb:TimeOut.");
	}
	else if (event & BEV_EVENT_EOF)
	{
	}
	else if (event & BEV_EVENT_ERROR)
	{
		int error_code = EVUTIL_SOCKET_ERROR();
		LOG4CXX_WARN(g_logger,
				"CWorkerThread::ClientTcpErrorCb:some other errorCode = " << error_code << ", description = " << evutil_socket_error_to_string(error_code));
	}

	CloseConn(conn, bev);
}

void CWorkerThread::DispatchSfdToWorker(int sfd)
{
	/* Round Robin*/
	int tid = (last_thread_ + 1) % utils::G<CGlobalSettings>().thread_num_; // 更加高效应该由worker thread进行基于锁的任务竞争
	LIBEVENT_THREAD *libevent_thread_ptr = vec_libevent_thread_.at(tid);
	last_thread_ = tid;

	/* 将新连接的加入此worker线程连接队列 */
	CONN_INFO connInfo;
	connInfo.sfd = sfd;
	libevent_thread_ptr->list_conn.push_back(connInfo);

	/* 通知此worker线程有新连接到来，可以读取了 */
	char buf[1];
	buf[0] = 'c';
	if (write(libevent_thread_ptr->notify_send_fd, buf, 1) != 1)    // 利用管道写操作write
	{
		LOG4CXX_WARN(g_logger, "CWorkerThread::DispatchSfdToWorker:Writing to thread notify pipe");
	}
}

void CWorkerThread::RegisterThreadInitialized(void)
{
    pthread_mutex_lock(&init_lock_);
    init_count_++;
    if(init_count_ == int(utils::G<CGlobalSettings>().thread_num_))
    {
    	pthread_cond_signal(&init_cond_);
    }
    pthread_mutex_unlock(&init_lock_);
}

void CWorkerThread::WaitForThreadRegistration(int nthreads)
{
    // 条件变量与互斥量一起使用时，允许线程以无竞争的方式等待特定的事件发生；条件本身由互斥量保护，线程在改变条件状态之前必须首先锁住互斥量
    // 调用者把上锁的mutex_lock传给函数，函数把调用线程放到等待条件（cond）的线程列表上，对互斥量解锁
	// 这就关闭了条件检查和线程进入休眠状态等待条件改变这两个操作之间的时间通道，不会错过条件的任何变化
	// pthread_cond_wait等待条件变量变为真，其返回时互斥量再次被锁住
	pthread_mutex_lock(&init_lock_);
    pthread_cond_wait(&init_cond_, &init_lock_);
    pthread_mutex_unlock(&init_lock_);
}

void CWorkerThread::InitFreeConns()
{
	freetotal_ 	= 200;	// 创建200个free connections
	freecurr_	= 0;

	vec_freeconn_.resize(freetotal_);
}

CONN* CWorkerThread::GetConnFromFreelist()
{
	CONN *conn = NULL;

	boost::mutex::scoped_lock Lock(mutex_);
	if(freecurr_ > 0)
	{
		conn = vec_freeconn_.at(--freecurr_);
	}

	return conn;
}

//todo： connection减少时缩减缓存
bool CWorkerThread::AddConnToFreelist(CONN* conn)
{
	bool ret = false;
	boost::mutex::scoped_lock Lock(mutex_);
	if (freecurr_ < freetotal_)
	{
		vec_freeconn_.at(freecurr_++) = conn;
		ret = true;
	}
	else
	{
		/* 增大连接内存池队列 */
		size_t newsize = freetotal_ * 2;
		vec_freeconn_.resize(newsize);
		freetotal_ = newsize;
		vec_freeconn_.at(freecurr_++) = conn;
		ret = true;
	}

	return ret;
}

void CWorkerThread::FreeConn(CONN* conn)
{
	if (conn)
	{
		utils::SafeDeleteArray(conn->rBuf);
		utils::SafeDeleteArray(conn->wBuf);
		utils::SafeDelete (conn);
	}
}

void CWorkerThread::CloseConn(CONN* conn, struct bufferevent* bev)
{
	assert(conn != NULL);

	/* 清理资源：the event, the socket and the conn */
	bufferevent_free(bev);

	LOG4CXX_TRACE(g_logger, "CWorkerThread::conn_close sfd = " << conn->sfd);

	/* if the connection has big buffers, just free it */
	if (!AddConnToFreelist (conn))
	{
		FreeConn(conn);
	}

	return;
}
