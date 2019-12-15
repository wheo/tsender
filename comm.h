#ifndef _COMM_H_
#define _COMM_H_

#include "main.h"
#include "demuxer.h"

#define MAX_NUM_SIM_CLIENT 64
#define MAX_NUM_CHANNEL 6
#define READ_SIZE 1500

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

class CCommMgr : public PThread
{
public:
    CCommMgr();
    ~CCommMgr();

    bool Open(int nPort, Json::Value attr);
    bool Echo(char *buf);

protected:
    void Run();
    void OnTerminate(){};
    void Delete();
    bool SetSocket();
    bool RX();
    bool TX(char *buff, int size);

protected:
    int m_nPort;
    int m_sdRecv;
    int m_sdSend;
    int m_nChannel;
    bool m_bIsRunning;
    struct sockaddr_in m_sin;
    Json::Value m_attr;
    CDemuxer *m_CDemuxer[MAX_NUM_CHANNEL];
    pthread_mutex_t m_mutex_comm;

private:
    int m_nSpeed;

protected:
    //CCommCt *m_pCt[MAX_NUM_SIM_CLIENT];
};

#endif // _COMM_H_
