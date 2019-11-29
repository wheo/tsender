#ifndef _CCORE_H_
#define _CCORE_H_

#define MAX_NUM_TRAPS			32
#define MAX_NUM_CHANNEL			6

#include "sender.h"

class CCore : public PThread {
public:
	CCore(void);
	~CCore(void);
	
	bool Create();	
	void Delete();

	//bool GetOutputs(string basepath);

protected:
	pthread_mutex_t m_mutex_trap;
private:
	int m_nChannel;
	Json::Value m_root;
	Json::Reader m_reader;
	CSender *m_CSender[MAX_NUM_CHANNEL];

protected:
	void Run();
	void OnTerminate() {};

};
#endif // _CCORE_H_
