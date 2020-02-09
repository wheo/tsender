#ifndef _SENDER_H_
#define _SENDER_H_

#define MAX_AUDIO_STREAM 8

#include "queue.h"
//#include "thumbnail.h"

#define AUDIO_BUFF_SIZE 24

class CSender : public PThread
{
public:
	CSender(void);
	~CSender(void);

	bool send_bitstream(uint8_t *stream, int size, char isvisible);
	bool send_audiostream(char *buff, int size, char state);

	int ReadSocket(uint8_t *buffer, unsigned bufferSize);
	bool Create(Json::Value info, Json::Value attr, int nChannel);
	void SetAVFormatContext(AVFormatContext **fmt_ctx);
	void Delete();
	void SetSpeed(int speed);
	void SetPause(bool state);
	void SetSyncPTS(uint64_t pts);

	//void SyncCheck();
	void SetReverse(bool state);
	int GetSpeed() { return m_nSpeed; }

	//bool SetMutex(pthread_mutex_t *mutex_sender);
	bool SetSocket();
	pthread_mutex_t *GetMutex() { return m_mutex_demuxer; }

	//bool Play();
	void log(int type, int state);
	bool SetQueue(CQueue **queue, int nChannel);
	void Enable() { m_enable = true; }
	void Disable() { m_enable = false; }

protected:
	int m_nChannel;		// 현재 채널 넘버
	Json::Value m_info; // 채널 정보 json
	Json::Value m_attr; // 채널 공유 속성 attribute
	CQueue *m_queue;

	bool m_enable;

	char m_strShmPrefix[32];
	int m_nRead;
	int m_nWrite;
	//double m_fps;
	bool m_bIsRerverse;
	bool m_bSend;
	bool m_bIsTimerReset;
	int64_t m_currentDuration;
	int64_t nFrame;
	AVStream *pStream;
	AVRational m_timeBase;
	//int64_t m_start_pts;

private:
	//mux_cfg_s m_mux_cfg;
	//CThumbnail *m_CThumbnail;

	int m_nRecSec; // 얼마나 녹화를 할 것인가
	//uint64_t m_nFrameCount; // 프레임 수
	uint64_t m_nAudioCount; // 오디오 수
	int m_file_idx;			// 파일 인덱스 번호
	int m_sock;				// 소켓 디스크립터

	bool m_IsPause;

	bool m_IsMove;

	Json::Value m_files;

	sockaddr_in m_mcast_group;
	//AVPacket m_pkt;
	AVFormatContext *m_fmt_ctx;
	int m_nSeekFrame;
	int m_nMoveIdx;
	bool m_next_keyframe;
	//double m_fMoveLeftSec;
	double m_fDuration;
	double m_fFPS;
	uint64_t m_current_pts;

	//uint64_t m_sync_pts;
	//uint m_sync_cnt;
	uint64_t m_seek_pts;
	uint64_t m_seek_old_pts;

	uint64_t m_dur_pts;
	uint64_t m_dur_cnt;
	int64_t m_add_pts;

	uint64_t m_out_pts;
	high_resolution_clock::time_point m_begin;
	high_resolution_clock::time_point m_end;
	high_resolution_clock::time_point m_reserve_clock;
	uint64_t m_now;
	uint64_t m_timer;
	//uint m_wait_frame;

	uint64_t m_file_first_pts;
	uint64_t m_reverse_pts;

	uint64_t m_nTotalFrame;
	int m_nTotalSec;
	int m_file_cnt;

	int video_stream_idx;
	int m_nAudioStream[MAX_AUDIO_STREAM];

	int m_nPlayAudioStream;
	int m_nAudioStreamCount;

	const AVBitStreamFilter *m_bsf = NULL;
	AVBSFContext *m_bsfc = NULL;

	AVCodecContext *video_dec_ctx = NULL;
	AVStream *video_stream = NULL, *audio_stream = NULL;

	int refcount;
	bool m_is_pframe_skip;

	Json::Value json;

	pthread_mutex_t *m_mutex_demuxer;
	int m_nSpeed;

protected:
	void Run();
	void OnTerminate(){};
};

#endif // _SENDER_H_
