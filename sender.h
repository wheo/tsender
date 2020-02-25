#ifndef _SENDER_H_
#define _SENDER_H_

#define MAX_AUDIO_STREAM 8

#include "queue.h"
//#include "thumbnail.h"

class CSender : public PThread
{
public:
	CSender(void);
	~CSender(void);

	bool send_bitstream(uint8_t *stream, int size, char isvisible);
	bool send_audiostream(char *buff, int size, char state);

	int ReadSocket(uint8_t *buffer, unsigned bufferSize);
	bool Create(Json::Value info, Json::Value attr, string type, int nChannel);
	void Delete();
	void SetSpeed(int speed);
	void SetPause(bool state);
	void SetPauseSync(int64_t sync_pts);

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
	bool m_bIsRerverse;
	string m_type;

private:
	//mux_cfg_s m_mux_cfg;
	int m_file_idx; // 파일 인덱스 번호
	int m_sock;		// 소켓 디스크립터

	bool m_IsPause;

	sockaddr_in m_mcast_group;
	//AVPacket m_pkt;
	AVFormatContext *m_fmt_ctx;

	int64_t m_current_pts;

	int64_t m_seek_pts;
	int64_t m_seek_old_pts;
	high_resolution_clock::time_point m_begin;
	high_resolution_clock::time_point m_end;
	Json::Value json;

	int64_t m_sync_pause_pts;

	pthread_mutex_t *m_mutex_demuxer;
	int m_nSpeed;

protected:
	void Run();
	void OnTerminate(){};
};

#endif // _SENDER_H_
