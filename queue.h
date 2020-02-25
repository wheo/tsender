#ifndef _QUEUE_H_
#define _QUEUE_H_

#define MAX_NUM_QUEUE 4
#define MAX_NUM_AUDIO_QUEUE 10
#define QUE_INFINITE -1
#define MIN_BUF_FRAME 9

#define VIDEO_PACKET_BUFFER 30

typedef struct tagELEM
{
	char *p;
	int len;
	char state;
} ELEM;

typedef struct tagVIDEOELEM
{
	AVPacket *pkt;
	char isvisible;
} VIDEOELEM;

class CQueue
{
public:
	CQueue(int nMaxSize, int nChannel, string type);
	~CQueue();

	void Clear();

	int PutVideo(AVPacket *pkt, int64_t offset_pts, char isvisible);
	int PutAudio(char *pData, int nSize, int64_t offset_pts, char statue);

	int GetVideo(AVPacket *pkt, int64_t *offset_pts, char *isvisible);
	void *GetAudio(int64_t *offset_pts);

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
	int64_t GetCurrentPTS();
	bool Exit();

private:
	bool m_bExit;

	char m_audio_status;

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

	uint64_t m_offset_pts;

	uint64_t m_current_video_pts;
	uint64_t m_current_audio_pts;

	int m_nChannel;
	string m_type;

	pthread_mutex_t m_mutex;

	AVPacket m_pkt[MAX_NUM_QUEUE];
	VIDEOELEM m_ve[MAX_NUM_QUEUE];
	ELEM m_e[MAX_NUM_AUDIO_QUEUE];
};

#endif //_QUEUE_H_
