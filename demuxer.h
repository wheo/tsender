#ifndef _DEMUXER_H_
#define _DEMUXER_H_

#define MAX_AUDIO_STREAM 8

#include "queue.h"
#include "thumbnail.h"
#include "sender.h"

#define AUDIO_BUFF_SIZE 24

class CDemuxer : public PThread
{
public:
	CDemuxer(void);
	~CDemuxer(void);

	//bool send_bitstream(uint8_t *stream, int size);
	//bool send_audiostream(char *buff, int size);

	int ReadSocket(uint8_t *buffer, unsigned bufferSize);
	bool Create(Json::Value info, Json::Value attr, int nChannel);
	void Delete();
	void SetSpeed(int speed);
	void SetPause(bool state);
	//void SetSyncPTS(uint64_t pts);

	//void SyncCheck();
	void SetReverse(bool state);
	int GetSpeed() { return m_nSpeed; }
	bool SetMutex(pthread_mutex_t *mutex_sender);
#if 0	
	bool SetSocket();
#endif
	pthread_mutex_t *GetMutex()
	{
		return m_mutex_demuxer;
	}

	uint64_t GetCurrentPTS();
	void SetSyncPTS(uint64_t max_pts);

	bool Play();
	void log(int type, int state);
	bool GetOutputs(string basepath);
	bool GetChannelFiles(string path);
	bool GetChannelFilesRerverse(string path);
	Json::Value GetThumbnail(int nSec);
	//int Demux(string src_filename);
	int Demux(Json::Value files);
	//int DemuxRerverse(string src_filename);
	bool SetMoveSec(int nSec);
	bool SetMoveFrame(uint64_t nFrame);
	bool SetMoveAudioCount(uint64_t audioCount);
	bool Reverse();
	bool SeekFrame(int nFrame);
	bool SeekPTS(uint64_t pts);
	int FindFileIndexFromFrame(uint64_t nFrame);
	int AudioSeek(uint64_t audioCount);
	bool SyncNReset();
	void Disable();
	void Enable();

protected:
	int m_nChannel;		// 현재 채널 넘버
	Json::Value m_info; // 채널 정보 json
	Json::Value m_attr; // 채널 공유 속성 attribute
	CQueue *m_queue;
	CSender *m_CSender;

	char m_strShmPrefix[32];
	int m_nRead;
	int m_nWrite;
	//double m_fps;
	bool m_bIsRerverse;
	int64_t m_currentDuration;
	int64_t nFrame;
	int64_t lldur;
	int m_n_gop;
	//int64_t m_start_pts;

private:
	//mux_cfg_s m_mux_cfg;
	CThumbnail *m_CThumbnail;

	int m_nRecSec; // 얼마나 녹화를 할 것인가
	//uint64_t m_nFrameCount; // 프레임 수
	uint64_t m_nAudioCount; // 오디오 수
	int m_file_idx;			// 파일 인덱스 번호
	int m_sock;				// 소켓 디스크립터

	bool m_IsPause;

	bool m_IsMove;

	bool m_IsAudioRead;

	Json::Value m_files;

	sockaddr_in m_mcast_group;
	AVPacket m_pkt;
	AVFormatContext *fmt_ctx;
	int m_nSeekFrame;
	int m_nMoveIdx;
	bool m_bIsSync;
	bool m_next_keyframe;
	uint64_t m_sync_pts;
	bool m_b_sync_check;
	//double m_fMoveLeftSec;
	double m_fDuration;
	double m_fFPS;
	uint64_t m_current_pts;
	bool m_bDisable;

	uint64_t m_file_first_pts;
	uint m_reverse_count;
	uint m_sync_cnt;
	uint64_t m_seek_pts;
	uint64_t m_reverse_pts;
	uint64_t m_first_reverse_pts;

	AVStream *m_pStream;
	AVRational m_timeBase;
	high_resolution_clock::time_point m_start_pts;

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
	bool m_is_skip;

	Json::Value json;

	pthread_mutex_t *m_mutex_demuxer;
	int m_nSpeed;
	int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type);

protected:
	void Run();
	void OnTerminate(){};
};

#endif // _DEMUXER_H_
