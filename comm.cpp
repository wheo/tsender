#include "comm.h"

using namespace std;

CCommMgr::CCommMgr()
{
    m_bExit = false;
    pthread_mutex_init(&m_mutex_comm, 0);
}

CCommMgr::~CCommMgr()
{
    m_bExit = true;
    pthread_mutex_destroy(&m_mutex_comm);
    cout << "[COMM] Trying to exit thread" << endl;
    Delete();
    if ( m_sdRecv > 0 ) {
        close(m_sdRecv);
        cout << "[COMM] RECV socket(" << m_sdRecv << ") has been closed" << endl;
    }
    if ( m_sdSend > 0 ) {
        close(m_sdSend);
        cout << "[COMM] SEND socket(" << m_sdSend << ") has been closed" << endl;
    }
    cout << "[COMM] before Terminate()" << endl;
    Terminate();
    cout << "[COMM] Exited..." << endl;
}

bool CCommMgr::SetSocket() {
    int t = 1;
    struct sockaddr_in sin;

    m_sdRecv = socket(PF_INET, SOCK_DGRAM, 0);
    if (m_sdRecv < 0)
    {
        _d("[COMM] Failed to open rx socket\n");
        return false;
    }

    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(m_nPort);

    setsockopt(m_sdRecv, SOL_SOCKET, SO_REUSEADDR, (const char *)&t, sizeof(t));

    if (bind(m_sdRecv, (const sockaddr *)&sin, sizeof(sin)) < 0)
    {
        _d("[COMM] Failed to bind to port %d\n", sin.sin_port);
        return false;
    }

	struct timeval read_timeout;
	read_timeout.tv_sec = 1;
	read_timeout.tv_usec = 0;
	if (setsockopt(m_sdRecv, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0)
	{
		cout << "[COMM] set timeout error" << endl;
		return false;
	}

    m_sdSend = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if ( m_sdSend < 0 ) {
        _d("[COMM] failed to open tx socket\n");
    }
    return EXIT_SUCCESS;
}

bool CCommMgr::Open(int nPort, Json::Value attr)
{
    m_nChannel = 0;
    m_nSpeed = 1;
    m_bIsRunning = false;
    m_nPort = nPort;
    m_attr = attr;

    SetSocket();
    cout << "[COMM] Before Thread Start" << endl;
    Start();

    return true;
}

bool CCommMgr::RX() {
    char buff[5];
    int rd = 0;

    struct sockaddr_in sin;

    socklen_t sin_size = sizeof(sin);

    cout << "[COMM] udp RX ready (port) : " << m_nPort << endl;

    while (!m_bExit)
    {
        rd = recvfrom(m_sdRecv, buff, sizeof(buff), 0, (struct sockaddr *)&sin, &sin_size);
        if ( rd < 1 ) {
            usleep(100);
            continue;
        }
        sendto(m_sdRecv, buff, sizeof(buff), 0, (struct sockaddr *)&sin, sin_size);

#if 0
        if ( m_bIsRunning ) {
            continue;
            usleep(1000);
        }
#endif
        _d("%c %c %d %d\n", buff[0], buff[1], buff[2], buff[3]);
        if (buff[0] == 'T' && buff[1] == 'N')
        {
            if (buff[2] == 0x00)
            {
                if (buff[3] == 0x01)
                {
                    _d("tn01 실행\n");
                    if (!m_bIsRunning)
                    {
                        m_bIsRunning = true;
                        m_nChannel = 0;
                        for (auto &value : m_attr["output_channels"])
                        {
                            m_CSender[m_nChannel] = new CSender();
                            m_CSender[m_nChannel]->SetMutex(&m_mutex_comm);
                            m_CSender[m_nChannel]->Create(m_attr["output_channels"][m_nChannel], m_attr, m_nChannel);
                            m_nChannel++;
                        }
                    }
                    else
                    {
                        cout << "[COMM] 실행 중" << endl;
                    }
                }
                else if (buff[3] == 0x02)
                {
                    _d("tn02 실행\n");
                    if (m_bIsRunning)
                    {
                        Delete();
                        _d("[COMM] Sender 종료\n");
                        m_nChannel = 0;
                        m_bIsRunning = false;
                    }
                    else
                    {
                        _d("[COMM] 실행 중이 아닙니다.\n");
                    }
                }
                else if (buff[3] == 0x03)
                {
                    //배속 재생
                    if (m_bIsRunning)
                    {
                        //실행 중이어야 배속 재생이 됨
                        m_nSpeed = m_nSpeed * 2; // 1 -> 2 -> 4 -> 8-> 16
                        for (int i = 0; i < m_nChannel; i++)
                        {
                            //일단 2배속
                            m_CSender[i]->SetSpeed(m_nSpeed);
                        }
                    }
                }
                else if ( buff[3] == 0x04) {
                    if ( m_nSpeed > 1 ) {
                        m_nSpeed = m_nSpeed / 2;
                        for(int i=0;i < m_nChannel; i++) {
                            m_CSender[i]->SetSpeed(m_nSpeed);
                        }
                    } else {
                        cout << "[COMM] 스피드를 내릴 수 없음" << endl;
                    }
                }
            }
        }
        else
        {
            _d("[CCT] Unknown sync code...%c %c %c %c\n", buff[0], buff[1], buff[2], buff[3]);
        }
    }
    _d("[CMGR] exit loop\n");
}

bool CCommMgr::TX(char *buff) {
    if ( m_sdSend < 0 ) {
        cout << "[COMM] send socket not created" << endl;
        return false;
    }

    struct sockaddr_in sin;
    socklen_t sin_size = sizeof(sin);

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(m_attr["uep_sender_ip"].asString().c_str());
    sin.sin_port = m_nPort;

    sendto(m_sdRecv, buff, sizeof(buff), 0, (struct sockaddr *)&sin, sin_size);
    cout << "ip : " << inet_ntoa(sin.sin_addr) << " send message : " << buff[0] << buff[1] << buff[2] << buff[3] << endl;
}

void CCommMgr::Run()
{
    RX();
}

void CCommMgr::Delete()
{
    cout << "[COMM] open channel count : " << m_nChannel << endl;
    for (int i = 0; i < m_nChannel; i++)
    {
        SAFE_DELETE(m_CSender[i]);
        cout << "[COMM] channel " << i << " has been SAFE_DELETE" << endl;
    }
}

CCommBase::CCommBase()
{
    m_sd = -1;
    m_bIsAlive = false;
}

CCommBase::~CCommBase()
{
    if (m_sd >= 0)
    {
        shutdown(m_sd, 2);
        close(m_sd);

        m_sd = -1;
        m_bIsAlive = false;
    }
}

int CCommBase::SR(char *pBuff, int nLen)
{
    int w = 0, local = nLen;

    while (local)
    {
        int r = recv(m_sd, &pBuff[w], local, 0);
        if (r > 0)
        {
            w += r;
            local -= r;
        }
        else
        {
            m_bIsAlive = false;
            return -1;
        }
    }

    return w;
}

int CCommBase::SS(char *pBuff, int nLen)
{
    int w = 0, local = nLen;

    while (local)
    {
        int s = send(m_sd, &pBuff[w], local, 0);
        if (s > 0)
        {
            w += s;
            local -= s;
        }
        else
        {
            m_bIsAlive = false;
            return -1;
        }
    }

    return w;
}
