#ifndef _COMM_H_
#define _COMM_H_

#include "main.h"
#include "sender.h"

#define MAX_NUM_SIM_CLIENT 64
#define MAX_NUM_CHANNEL 6

class CCommBase // Class for comm. module base
{
public:
    CCommBase();
    ~CCommBase();

    bool IsAlive() { return m_bIsAlive; };

protected:
    pthread_mutex_t g_mutex;
    int m_sd; // Socket descriptor
    bool m_bIsAlive;

    char m_sndbuf[8];
    char m_buff[1024 * 128];

protected:
    int SR(char *pBuff, int nLen);
    int SS(char *pBuff, int nLen);
};

class CCommCt : public CCommBase, public PThread
{
public:
    CCommCt();
    ~CCommCt();

    void Create(int sd, char *pIpAddr);

protected:
    char m_strIpAddr[32]; // IP address of client

protected:
    void Run();
    void OnTerminate(){};
};

class CCommMgr : public PThread
{
public:
    CCommMgr();
    ~CCommMgr();

    bool Open(int nPort, Json::Value attr);

protected:
    void Run();
    void OnTerminate(){};
    void Delete();

protected:
    int m_nPort;
    int m_sdListen;
    int m_nChannel;
    bool m_bIsRunning;
    Json::Value m_attr;
    CSender *m_CSender[MAX_NUM_CHANNEL];

protected:
    //CCommCt *m_pCt[MAX_NUM_SIM_CLIENT];
};

#endif // _COMM_H_
