#include "main.h"
#include "sender.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

#define MAX_NUM_bitstream 4
#define MAX_frame_size 1024 * 1024 // 1MB

CSender::CSender(void)
{
	m_bExit = false;
	m_sock = 0;
	m_queue = NULL;
	m_bIsRerverse = false;
	m_enable = true;
}

CSender::~CSender(void)
{
	m_bExit = true;
	Delete();
	//_d("[SENDER.ch%d] Trying to exit thread\n", m_nChannel);
	Terminate();
	_d("[SENDER.ch%d] exited...\n", m_nChannel);
}

#if 0
void CSender::log(int type, int state)
{
}
#endif

bool CSender::SetSocket()
{
	struct ip_mreq mreq;
	int state;

	memset(&m_mcast_group, 0x00, sizeof(m_mcast_group));
	m_mcast_group.sin_family = AF_INET;
	m_mcast_group.sin_port = htons(m_info["port"].asInt());
	m_mcast_group.sin_addr.s_addr = inet_addr(m_info["ip"].asString().c_str());
	//m_mcast_group.sin_addr.s_addr = inet_addr(INADDR_ANY);

	cout << "[SENDER.ch" << m_nChannel << "] ip : " << m_info["ip"].asString() << ", port : " << m_info["port"].asInt() << endl;

	m_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == m_sock)
	{
		cout << "[SENDER.ch" << m_nChannel << "] socket createion error" << endl;
		return false;
	}

	if (-1 == bind(m_sock, (struct sockaddr *)&m_mcast_group, sizeof(m_mcast_group)))
	{
		cout << "[SENDER.ch" << m_nChannel << "] bind error" << endl;
		return false;
	}

	uint reuse = 1;
	state = setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if (state < 0)
	{
		cout << "[SENDER.ch" << m_nChannel << "] Setting SO_REUSEADDR error" << endl;
		return false;
	}

	//uint ttl = 16;
	uint ttl = m_attr["udp_sender_ttl"].asUInt();
	state = setsockopt(m_sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
	if (state < 0)
	{
		cout << "[SENDER.ch" << m_nChannel << "] Setting IP_MULTICAST_TTL error" << endl;
		return false;
	}
#if 1
	mreq.imr_multiaddr = m_mcast_group.sin_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
#else
	mreq.imr_multiaddr.s_addr = inet_addr(m_info["ip"].asString().c_str());
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
#endif

	if (setsockopt(m_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	{
		cout << "[SENDER.ch" << m_nChannel << "] add membership setsocket opt" << endl;
		return false;
	}

	struct timeval read_timeout;
	read_timeout.tv_sec = 1;
	read_timeout.tv_usec = 0;
	if (setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0)
	{
		cout << "[SENDER.ch" << m_nChannel << "] set timeout error" << endl;
		return false;
	}

	return true;
}

bool CSender::Create(Json::Value info, Json::Value attr, int nChannel)
{
	m_info = info;
	m_nChannel = nChannel;
	m_attr = attr;
	pStream = NULL;

	//uint64_t num = m_info["num"].asUInt64();
	//uint64_t den = m_info["den"].asUInt64();

	m_nAudioCount = 0;
	m_nSpeed = 1;

	cout << "[SENDER.ch" << m_nChannel << "] type : " << m_info["type"].asString() << endl;

	if (SetSocket())
	{
		cout << "[SENDER.ch" << m_nChannel << "] SetSocket config is completed" << endl;
		usleep(1000);
		Start();
	}
	return true;
}

bool CSender::SetQueue(CQueue **queue, int nChannel)
{
	m_queue = *queue;
	m_nChannel = nChannel;
	return true;
}

#if 0
bool CSender::SetMutex(pthread_mutex_t *mutex)
{
	m_mutex_demuxer = mutex;
	cout << "[SENDER.ch" << m_nChannel << "] m_mutex_sender address : " << m_mutex_demuxer << endl;
}
#endif

void CSender::SetAVFormatContext(AVFormatContext **fmt_ctx)
{
	m_fmt_ctx = *fmt_ctx;
	pStream = m_fmt_ctx->streams[0];
	m_timeBase = pStream->time_base;
	//_d("[SENDER.ch%d] SetAVFormatContext : %x ...\n", m_nChannel, m_fmt_ctx);
}

void CSender::Run()
{
	uint64_t tick_diff = 0;
	uint64_t target_time = 0;
	int sended = 0;
	int want_send = 0;

	//Play();
	int keyframe_speed = m_attr["keyframe_speed"].asInt();
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();
	uint gop = m_info["gop"].asUInt();

	int nSpeed = 1;
	int nOldSpeed = 1;
	int speed_type = 0;

	uint64_t delay = num * AV_TIME_BASE / den;

	bool isPause = false;
	bool pauseOld = false;
	bool isReverse = false;
	bool reverseOld = false;
	uint64_t old_pts = 0;
	int64_t pts_diff = 0;
	int64_t pts_diff_old = 0;

	m_now = 0;

	m_begin = high_resolution_clock::now();
	string type = m_info["type"].asString();

	int counter = 0;
	int nFrame = 0;

	while (!m_bExit)
	{
		isPause = m_IsPause;
		if (m_nSpeed == 0)
		{
			nSpeed = 1;
		}
		else
		{
			nSpeed = m_nSpeed;
		}

		if (m_bIsRerverse == true)
		{
			if (m_nChannel < 4)
			{
				target_time = delay * gop / nSpeed;
			}
			else
			{
				target_time = delay / nSpeed;
			}
		}
		else if (m_bIsRerverse == false)
		{
			target_time = delay / nSpeed;
		}

		if (isPause == true)
		{
			target_time = delay;
		}

		m_end = high_resolution_clock::now();
		tick_diff = duration_cast<microseconds>(m_end - m_begin).count();
		if (tick_diff < target_time)
		{
			continue;
		}
		m_begin = m_end;

		sended += tick_diff;
		want_send += target_time;
		counter++;
		nFrame++;
		//cout << "[SENDER.ch" << m_nChannel << "] (" << nFrame << "), (" << tick_diff << "), (" << sended << "), (" << target_time << "), (" << want_send << ")" << endl;
		//cout << "[SENDER.ch" << m_nChannel << "] tick_diff : " << tick_diff << ", speed : " << nSpeed << endl;
#if 1

#endif

		if (type == "video")
		{
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;
			pkt.pts = 0;

			char isvisible;

			if (isPause != pauseOld)
			{
				//상태 변화가 일어났다
				if (isPause == true)
				{
					//
				}
				else if (isPause == false)
				{
					//마지막 프레임 버림
					if (m_queue->GetVideo(&pkt, &isvisible) > 0)
					{
						m_queue->RetVideo(&pkt);
					}
				}
			}
			pauseOld = isPause;

			//cout << "[SENDER.ch" << m_nChannel << "] pkt.flags (" << pkt.flags << "), (" << counter << "), (" << pts_diff << "), (" << sended << "), (" << target_time << "), (" << want_send << ")" << endl;
			while (m_bExit == false && m_queue)
			{
				int size = 0;

				size = m_queue->GetVideo(&pkt, &isvisible);
				//cout << "[SENDER.ch" << m_nChannel << "] get size : " << size << endl;
				if (size > 0)
				{
					m_current_pts = pkt.pts;
					pts_diff = llabs(m_current_pts - old_pts);

					if (m_nSpeed == 0)
					{
						m_is_pframe_skip = false;
					}
					else
					{
						m_is_pframe_skip = true;
					}
					if (isPause == true)
					{
						m_is_pframe_skip = false;
					}

					m_now = pkt.pts * AV_TIME_BASE / m_timeBase.den;
					if (m_is_pframe_skip == false || pkt.flags == AV_PKT_FLAG_KEY)
					{
						if (send_bitstream(pkt.data, pkt.size, isvisible))
						{
							#if 0
							cout << "[SENDER.ch" << m_nChannel << "] send_bitstream (" << pkt.pts << "), (" << m_now << "), (" << nFrame << "), diff(" << pts_diff << ") sended time (" << sended << "), (" << pkt.flags << ") is_pframe_skip : " << std::boolalpha << m_is_pframe_skip << endl;
							#endif
#if 0							
							if (pts_diff != pts_diff_old)
							{
								cout << "[SENDER.ch" << m_nChannel << "] !!!! frame (" << nFrame << "), (" << pts_diff << "), (" << pts_diff_old << ") pts (" << m_current_pts << "), (" << old_pts << ")" << endl;
							}
#endif
							sended = 0;
							want_send = 0;
							counter = 0;
						}
						else
						{
							cout << "[SENDER.ch" << m_nChannel << "] send_bitstream failed" << endl;
						}
					}

					if (m_queue)
					{
						m_queue->RetVideo(&pkt);
					}
					break;
				}
				//usleep(10);
			}
		}

		else if (type == "audio")
		{
			//m_out_pts = ((m_nAudioCount * AV_TIME_BASE) * num / den);

			if (m_queue)
			{
				char status;
				ELEM *pe = (ELEM *)m_queue->GetAudio();
				if (pe && pe->len > 0)
				{
					status = pe->state;
					m_nAudioCount++;
					m_current_pts = (m_nAudioCount * AV_TIME_BASE * num) / den;
					if (m_nSpeed == 0 && isReverse == false)
					{
						if (send_audiostream(pe->p, AUDIO_BUFF_SIZE, status))
						{
#if 0
							cout << "[SENDER.ch" << m_nChannel << "] send_audiotream sended(" << AUDIO_BUFF_SIZE << "), AudioCount : " << m_nAudioCount << endl;
#endif
						}
						else
						{
							cout << "[SENDER.ch" << m_nChannel << "] send_audiotream failed" << endl;
						}
					}
					if (m_queue)
					{
						m_queue->RetAudio(pe);
					}
				}
			}
		}

		pts_diff_old = pts_diff;
		old_pts = m_current_pts;
	}
}

bool CSender::send_audiostream(char *buff, int size, char state)
{
	char reserve[4] = {
		0,
	};

	reserve[0] = state; // 0 : 정상재생 1 : 구간점프

	int header_size = sizeof(reserve);

	char *p = buff;
	uint8_t buffer[header_size + size];

	memcpy(&buffer[0], &reserve, sizeof(reserve));
	memcpy(&buffer[header_size], p, size);

	int nSendto = 0;
	nSendto = sendto(m_sock, buffer, header_size + size, 0, (struct sockaddr *)&m_mcast_group, sizeof(m_mcast_group));
	if (nSendto > 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CSender::send_bitstream(uint8_t *stream, int size, char isvisible)
{
	int tot_packet = 0;
	int cur_packet = 1;
	int old_packet = 0;

	int tot_size;
	int cur_size;

	int remain;

	int nSendto;

	char video_type = 1;	   // 1(NTSC), 2(PAL), 3(PANORAMA), 4(FOCUS)
	char video_codec_type = 0; // 1(HEVC), 2(H264)
	char video_frame_type = 0; // 0(P frame), 1(I frame)
	char reserve[5] = {
		0,
	};

	reserve[0] = 1; // 0 : 노멀 1 : 확장

	int header_size = 0;

	if (reserve[0] == 0)
	{
		header_size = PACKET_HEADER_SIZE;
	}
	else if (reserve[0] == 1)
	{
		header_size = PACKET_HEADER_EXTEND_SIZE;
	}

	uint8_t *p = stream;
	uint8_t buffer[header_size + PACKET_SIZE];

	tot_size = remain = size;

	if ((tot_size % PACKET_SIZE) == 0)
	{
		tot_packet = tot_size / PACKET_SIZE;
	}
	else
	{
		tot_packet = tot_size / PACKET_SIZE + 1;
	}

	while (remain > 0)
	{
		if (remain > PACKET_SIZE)
		{
			cur_size = PACKET_SIZE;
		}
		else
		{
			cur_size = remain;
		}

		memcpy(&buffer[0], &tot_size, 4);
		memcpy(&buffer[4], &cur_size, 4);
		memcpy(&buffer[8], &tot_packet, 4);
		memcpy(&buffer[12], &cur_packet, 4);
		memcpy(&buffer[16], &video_type, 1);
		memcpy(&buffer[17], &video_codec_type, 1);
		memcpy(&buffer[18], &video_frame_type, 1);
		memcpy(&buffer[19], &reserve, sizeof(reserve));

		if (reserve[0] == 1)
		{
			// ..... here
			memcpy(&buffer[PACKET_HEADER_SIZE + 5], &isvisible, 1);
			memcpy(&buffer[PACKET_HEADER_SIZE + 6], &m_current_pts, 8);
			memcpy(&buffer[PACKET_HEADER_SIZE + 14], &m_now, 8);
		}

		memcpy(&buffer[header_size], p, cur_size);

		nSendto = sendto(m_sock, buffer, header_size + cur_size, 0, (struct sockaddr *)&m_mcast_group, sizeof(m_mcast_group));
		if (nSendto < 0)
		{
			return false;
		}
		else
		{
#if 0
			cout << "[DEMUXER.ch" << m_nChannel << "] "
				 << "tot_size : " << tot_size << ", cur_size : " << cur_size << ", tot_packet : " << tot_packet << ", cur_packet : " << cur_packet << ", nSendto : " << nSendto << endl;
#endif
		}

		remain -= cur_size;
		p += cur_size;

		old_packet = cur_packet;
		cur_packet++;
		usleep(100);
	}

	return true;
}

void CSender::SetSpeed(int speed)
{
	m_nSpeed = speed;
	//cout << "[SENDER.ch" << m_nChannel << "] Set speed : " << m_nSpeed << endl;
}

void CSender::SetPause(bool state)
{
	m_IsPause = state;
	//cout << "[SENDER.ch" << m_nChannel << "] Set Pause : " << m_IsPause << endl;
}

void CSender::SetReverse(bool state)
{
	m_bIsRerverse = state;
	//cout << "[SENDER.ch" << m_nChannel << "] Set Reverse : " << m_bIsRerverse << endl;
}

void CSender::Delete()
{
	if (m_sock > 0)
	{
		cout << "[SENDER.ch" << m_nChannel << "] sock " << m_sock << " closed" << endl;
		close(m_sock);
	}
	SAFE_DELETE(m_queue);
}