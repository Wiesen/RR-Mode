#ifndef MASTER_THREAD__H_
#define MASTER_THREAD__H_

#include "defines.h"

class CWorkerThread;

class CMasterThread: boost::noncopyable
{
public:
	CMasterThread();
	virtual ~CMasterThread();

public:

	bool InitMasterThread();

	void Run();

private:

	bool CheckLibeventVersion();

	bool InitRemoteListenSocket(evutil_socket_t& listen_socket);

	static void AccepCb(evutil_socket_t listen_socket, short event, void* arg);

private:
	struct event_base 	*main_base_;
	evutil_socket_t 	remote_listen_socket_;
	struct event		*listen_event_;

	CWorkerThread		*worker_thread_ptr_; // 通过CWorkerThrad类管理所有worker thread（vector，RR mode）
};


#endif /* NET_CORE_H_ */
