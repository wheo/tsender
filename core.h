#ifndef _CCORE_H_
#define _CCORE_H_

#define MAX_NUM_TRAPS 32

#include "comm.h"

class CCore : public PThread
{
public:
	CCore(void);
	~CCore(void);

	bool Create();
	void Delete();
	void UndeleteFileDelete();

	//bool GetOutputs(string basepath);

protected:
	pthread_mutex_t m_mutex_core;

private:
	int m_nChannel;
	Json::Value m_root;
	Json::Value m_init;
	Json::Reader m_reader;
	CCommMgr *m_comm;

protected:
	void Run();
	void OnTerminate(){};
};
#endif // _CCORE_H_
