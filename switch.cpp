#include "main.h"
#include "switch.h"

using namespace std;

CSwitch::CSwitch(void)
{
    m_bExit = false;
    m_fmt_ctx = NULL;
    m_current_frame = 0;
    m_last_frame = 0;
    pthread_mutex_init(&m_mutex_switch, 0);
    m_filename = "";
    m_nChannel = 0;
    Start();
}

CSwitch::~CSwitch(void)
{
    m_bExit = true;
    pthread_mutex_destroy(&m_mutex_switch);
    _d("[SWITCH] Trying to exit thread\n");
    Delete();
    cout << "[SWITCH] Before Terminate()" << endl;
    Terminate();
    cout << "[SWITCH] has been exited..." << endl;
}

void CSwitch::Create(int nChannel)
{
    m_nChannel = nChannel;
}

int CSwitch::GetContext(AVFormatContext *fmt_ctx)
{
    if (m_fmt_ctx)
    {
        fmt_ctx = m_fmt_ctx;
        m_fmt_ctx = NULL;
        return 1;
    }
    else
    {
        return 0;
    }
}

void CSwitch::SetFrame(int current_frame, uint64_t last_frame)
{
    m_current_frame = current_frame;
    m_last_frame = last_frame;
}

void CSwitch::SetNextFileName(string filename)
{
    if (filename.empty())
    {
        m_filename == "";
    }
    else
    {
        m_filename = filename;
    }
}

void CSwitch::Run()
{
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    pkt.pts = 0;

    while (!m_bExit)
    {
        //cout << "[SWOTCH.ch" << m_nChannel << "] last frame : " << m_last_frame << ", current frame : " << m_current_frame << endl;
        if (m_last_frame - 60 < m_current_frame)
        {
            if (m_fmt_ctx == NULL && !m_filename.empty())
            {
                if (avformat_open_input(&m_fmt_ctx, m_filename.c_str(), NULL, NULL) < 0)
                {
                    cout << "[SWITCH.ch" << m_nChannel << "] Could not open source file : " << m_filename << endl;
                }
                else
                {
                    m_filename = "";
#if 0
                    for (int i = 0; i < 10; i++)
                    {
                        av_read_frame(m_fmt_ctx, &pkt);
                        av_packet_unref(&pkt);
                    }
#endif
                }
            }
        }
        usleep(100);
    }
}

void CSwitch::Delete()
{
}
