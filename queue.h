#ifndef _QUEUE_H_
#define _QUEUE_H_

#define MAX_NUM_QUEUE 120
#define MAX_NUM_AUDIO_QUEUE 512
#define QUE_INFINITE -1
#define MIN_BUF_FRAME 9

typedef struct tagELEM
{
	char *p;
	int len;
} ELEM;

class CQueue
{
public:
	CQueue();
	~CQueue();

	void SetInfo(int nChannel, string type);
	void Clear();

	int Put(AVPacket *pkt);
	int PutAudio(char *pData, int nSize);

	int Get(AVPacket *pkt);
	void *GetAudio();

	void Ret(AVPacket *pkt);
	void RetAudio(void *p);

	void Enable();
	void Disable();
	void EnableAudio();  // 아직 구현 안됨 2019-12-17
	void DisableAudio(); // 아직 구현 안됨 2019-12-17
	bool IsEnable() { return m_bEnable; };
	bool IsAudioEnable() { return m_bAudioEnable; };

	int GetVideoPacketSize() { return m_nPacket; }
	int GetAudioPacketSize() { return m_nAudio; }
	bool Exit();

private:
	bool m_bExit;

	bool m_bEnable;
	bool m_bAudioEnable;

	int m_nPacket;
	int m_nAudio;

	int m_nMaxQueue;
	int m_nMaxAudioQueue;

	int m_nSizeQueue;

	int m_nReadPos;
	int m_nWritePos;

	int m_nReadAudioPos;
	int m_nWriteAudioPos;

	int m_nReadFramePos;
	int m_nWriteFramePos;

	int m_nChannel;
	string m_type;

	pthread_mutex_t m_mutex;

	AVPacket m_pkt[MAX_NUM_QUEUE];
	ELEM m_e[MAX_NUM_AUDIO_QUEUE];
};

#endif //_QUEUE_H_
