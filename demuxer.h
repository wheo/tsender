#ifndef _DEMUXER_H_
#define _DEMUXER_H_

#define MAX_AUDIO_STREAM 8

#include "queue.h"
#include "thumbnail.h"
#include "sender.h"

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
	void SetPauseSync(int64_t sync_pts);
	void SetSync(int64_t sync_pts);

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

	int64_t GetCurrentPTS();
	int64_t GetCurrentOfffset();

	bool Play();
	bool GetOutputs(string basepath);
	bool GetChannelFiles(string path);
	Json::Value GetThumbnail(int nSec);
	//int Demux(string src_filename);
	int Demux(Json::Value files);
	//int DemuxRerverse(string src_filename);
	bool SetMoveSec(int nSec);
	bool SetMovePTS(int64_t pts);
	bool Reverse();
	bool SeekPTS(int64_t pts);
	int FindFileIndexFromPTS(int64_t pts);
	int AudioSeek(uint64_t audioCount);
	void Disable();
	void Enable();
	string GetChannelType(int nChannel);

protected:
	int m_nChannel;		// 현재 채널 넘버
	Json::Value m_info; // 채널 정보 json
	Json::Value m_attr; // 채널 공유 속성 attribute
	CQueue *m_queue;
	CSender *m_CSender;

	//double m_fps;
	bool m_bIsRerverse;
	int64_t nFrame;
	int m_n_gop;
	bool m_isFOF;
	bool m_isEOF;
	string m_type;
	//int64_t m_start_pts;
	//bool m_b_ready_to_send_pts;

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
	int64_t m_seek_pts;
	int64_t m_offset_pts;

	char m_audio_status;

	int m_nMoveIdx;
	bool m_next_keyframe;

	int64_t m_current_pts;
	int64_t m_first_pts;
	int64_t m_last_pts;
	bool m_bDisable;

	AVStream *m_pStream;
	high_resolution_clock::time_point m_start_pts;

	uint64_t m_nTotalFrame;
	int m_nTotalSec;

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
	pthread_cond_t m_sleepCond;
	int m_nSpeed;
	int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type);

protected:
	void Run();
	void OnTerminate(){};
};

#endif // _DEMUXER_H_
