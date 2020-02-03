#ifndef _QUEUE_H_
#define _QUEUE_H_

#define MAX_NUM_QUEUE 2
#define MAX_NUM_AUDIO_QUEUE 2
#define QUE_INFINITE -1
#define MIN_BUF_FRAME 9

#define VIDEO_PACKET_BUFFER 30

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

	int PutVideo(AVPacket *pkt, uint64_t start_pts);
	int PutAudio(char *pData, int nSize, uint64_t start_pts);

	int GetVideo(AVPacket *pkt, uint64_t *start_pts);
	void *GetAudio(uint64_t *start_pts);

	void RetVideo(AVPacket *pkt);
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

	//int m_nReadFramePos;
	//int m_nWriteFramePos;

	uint64_t m_start_pts;

	int m_nChannel;
	string m_type;

	pthread_mutex_t m_mutex;

	AVPacket m_pkt[MAX_NUM_QUEUE];
	ELEM m_e[MAX_NUM_AUDIO_QUEUE];
};

#endif //_QUEUE_H_
