#include "main.h"
#include "thumbnail.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

#define MAX_NUM_bitstream 4
#define MAX_frame_size 1024 * 1024 // 1MB

CThumbnail::CThumbnail(void)
{
	m_bExit = false;
	pthread_mutex_init(m_mutex_thumbnail, 0);
	m_fmt_ctx = NULL;
}

CThumbnail::~CThumbnail(void)
{
	m_bExit = true;

	Delete();
	_d("[THUMBNAIL.ch%d] Trying to exit thread\n", m_nChannel);
	Terminate();
	_d("[THUMBNAIL.ch%d] exited...\n", m_nChannel);
	pthread_mutex_destroy(m_mutex_thumbnail);
}

bool CThumbnail::Create(Json::Value info, Json::Value attr, int nChannel)
{
	m_info = info;
	m_attr = attr;
	m_nChannel = nChannel;
	return true;
}

void CThumbnail::SetAVFormatContext(AVFormatContext **fmt_ctx)
{
	m_fmt_ctx = *fmt_ctx;
	_d("[THUMBNAIL.ch%d] SetAVFormatContext : %x ...\n", m_nChannel, m_fmt_ctx);
}

Json::Value CThumbnail::GetThumbnail(int nSec)
{
	int num = m_info["num"].asInt();
	int den = m_info["den"].asInt();
	double fps = den / num;
	Json::Value ret_json;
	ret_json["cmd"] = "";
	if (m_fmt_ctx)
	{
		int ret = 0;
		double fTime = 0;
		AVStream *pStream = m_fmt_ctx->streams[0];
		int nFrame = nSec * fps;
		fTime = (((double)nFrame * pStream->avg_frame_rate.den) / pStream->avg_frame_rate.num) - 0.5;
		fTime = max(fTime, 0.);

		AVRational timeBaseQ;
		AVRational timeBase = pStream->time_base;

		timeBaseQ.num = 1;
		timeBaseQ.den = AV_TIME_BASE;

		int64_t tm = (int64_t)(fTime * AV_TIME_BASE);
		tm = av_rescale_q(tm, timeBaseQ, timeBase);
		int64_t seek_target = av_rescale((nSec * AV_TIME_BASE), m_fmt_ctx->streams[0]->time_base.den, m_fmt_ctx->streams[0]->time_base.num);
		if (m_nChannel < 6)
		{
			ret = avformat_seek_file(m_fmt_ctx, 0, 0, tm, tm, AVSEEK_FLAG_FRAME);
			_d("[THUMBNAIL.ch%d] ret : %d , timebaseQ : %d/%d, timebase : %d/%d\n", m_nChannel, ret, timeBaseQ.num, timeBaseQ.den, timeBase.num, timeBase.den);
		}
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;

		if (av_read_frame(m_fmt_ctx, &pkt) < 0)
		{
			//read frame error
		}
	}
	else
	{
		cout << "error" << endl;
		ret_json["error"] = true;
	}
	string base64_str = base64_encode(m_pkt.data, m_pkt.size);
	ret_json["thumbnail"] = base64_str;

	return ret_json;
}

void CThumbnail::Run()
{
	while (!m_bExit)
	{
		usleep(100);
	}
}

void CThumbnail::Delete()
{
}