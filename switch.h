#ifndef _CSWITCH_H_
#define _CSWITCH_H_

#include "switch.h"

class CSwitch : public PThread
{
public:
    CSwitch(void);
    ~CSwitch(void);
    void Create(int nChannel);
    void Delete();
    int GetContext(AVFormatContext *fmt_ctx);
    void SetFrame(int current_frame, uint64_t last_frame);
    void SetNextFileName(string filename);

protected:
    pthread_mutex_t m_mutex_switch;
    AVFormatContext *m_fmt_ctx;

private:
    int m_nChannel;
    uint64_t m_last_frame;
    int m_current_frame;
    string m_filename;

protected:
    void Run();
    void OnTerminate(){};
};
#endif // _CSWITCH_H_
