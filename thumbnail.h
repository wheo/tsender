#ifndef _THUMBNAIL_H_
#define _THUMBNAIL_H_

#define MAX_AUDIO_STREAM 8

#include "sender.h"
#include "queue.h"

#define AUDIO_BUFF_SIZE 24

class CThumbnail : public PThread
{
public:
	CThumbnail(void);
	~CThumbnail(void);

	bool Create(Json::Value info, Json::Value attr, int nChannel);
	void Delete();
	void SetAVFormatContext(AVFormatContext **fmt_ctx);
	Json::Value GetThumbnail(int nSec);

private:
	pthread_mutex_t m_mutex_thumbnail;
	int m_nChannel;
	Json::Value m_info; // 채널 정보 json
	Json::Value m_attr; // 채널 공유 속성 attribute
	AVPacket m_pkt;
	AVFormatContext *m_fmt_ctx;
	bool MoveSec(int nSec);

protected:
	void Run();
	void OnTerminate(){};
};

#endif // _THUMBNAIL_H_