#ifndef _DEMUXER_H_
#define _DEMUXER_H_

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
	bool CreateReverse(Json::Value info, Json::Value attr, int nChannel);
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
	bool MoveFileTime(int nSec);
	void ReadStart();
	void ReadStop();

protected:
	int m_nChannel;		// 현재 채널 넘버
	Json::Value m_info; // 채널 정보 json
	Json::Value m_attr; // 채널 공유 속성 attribute
	CQueue *m_CQueue;
	CSender *m_CSender;

	char m_strShmPrefix[32];
	int m_nRead;
	int m_nWrite;
	bool m_IsRerverse;

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

	//const AVBitStreamFilter *m_bsf = NULL;
	//AVBSFContext *m_bsfc = NULL;

	string m_filename;

	Json::Value json;

	pthread_mutex_t *m_mutex_demuxer;
	int m_nSpeed;

protected:
	void Run();
	void OnTerminate(){};
};

#endif // _DEMUXER_H_
