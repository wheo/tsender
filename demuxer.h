#ifndef _DEMUXER_H_
#define _DEMUXER_H_

#define MAX_AUDIO_STREAM 8

#include "sender.h"
#include "queue.h"

#define AUDIO_BUFF_SIZE 24

class CDemuxer : public PThread
{
public:
	CDemuxer(void);
	~CDemuxer(void);

	bool send_bitstream(uint8_t *stream, int size);
	bool send_audiostream(char *buff, int size);

	int ReadSocket(uint8_t *buffer, unsigned bufferSize);
	bool Create(Json::Value info, Json::Value attr, int nChannel);
	void Delete();
	void SetSpeed(int speed);
	void SetPause();
	void SetReverse();
	int GetSpeed() { return m_nSpeed; }

	bool SetMutex(pthread_mutex_t *mutex_sender);
	bool SetSocket();
	pthread_mutex_t *GetMutex() { return m_mutex_demuxer; }

	bool Play();
	void log(int type, int state);
	bool GetOutputs(string basepath);
	bool GetChannelFiles(string path);
	bool GetChannelFilesRerverse(string path);
	int Demux(string src_filename);
	int DemuxRerverse(string src_filename);
	bool SetMoveSec(int nSec);
	bool Reverse();

protected:
	int m_nChannel;		// 현재 채널 넘버
	Json::Value m_info; // 채널 정보 json
	Json::Value m_attr; // 채널 공유 속성 attribute
	CQueue *m_CQueue;
	CSender *m_CSender;

	char m_strShmPrefix[32];
	int m_nRead;
	int m_nWrite;
	double m_fps;
	bool m_IsRerverse;
	int64_t m_currentDuration;
	int64_t nFrame;

private:
	//mux_cfg_s m_mux_cfg;

	int m_nRecSec;	 // 얼마나 녹화를 할 것인가
	int m_nFrameCount; // 프레임 수
	int m_file_idx;	// 파일 인덱스 번호
	int m_sock;		   // 소켓 디스크립터
	bool m_IsPause;
	sockaddr_in m_mcast_group;
	AVPacket m_pkt;
	AVFormatContext *fmt_ctx;
	int m_index = 0;
	int m_nMoveSec;
	double m_fDuration;
	double m_fFPS;

	int video_stream_idx;
	int m_nAudioStream[MAX_AUDIO_STREAM];

	int m_nPlayAudioStream;
	int m_nAudioStreamCount;

	const AVBitStreamFilter *m_bsf = NULL;
	AVBSFContext *m_bsfc = NULL;

	AVCodecContext *video_dec_ctx = NULL;
	AVStream *video_stream = NULL, *audio_stream = NULL;

	int refcount;

	Json::Value json;

	pthread_mutex_t *m_mutex_demuxer;
	int m_nSpeed;
	bool MoveSec(int nSec);
	int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type);

protected:
	void Run();
	void OnTerminate(){};
};

#endif // _DEMUXER_H_
