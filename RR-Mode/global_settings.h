#ifndef GLOBAL_SETTINGS_H_
#define GLOBAL_SETTINGS_H_

class CGlobalSettings
{
public:
	int remote_listen_port_;
	unsigned int thread_num_;

	int client_heartbeat_timeout_;
};

#endif /* GLOBAL_SETTINGS_H_ */
