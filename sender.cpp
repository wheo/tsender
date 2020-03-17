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

bool CSender::Create(Json::Value info, Json::Value attr, string type, int nChannel)
{
	m_info = info;
	m_nChannel = nChannel;
	m_attr = attr;
	m_type = type;
	m_nSpeed = 0;

	cout << "[SENDER.ch" << m_nChannel << "] type : " << m_type << endl;

	if (SetSocket())
	{
		cout << "[SENDER.ch" << m_nChannel << "] SetSocket config is completed" << endl;
		//usleep(10000);
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

void CSender::Run()
{
	int64_t tick_diff = 0;
	int64_t target_time = 0;
	int64_t out_diff = 0;
	int nSpeed = 1;
	int nOldSpeed = 1;
	int speed_type = 0;

	bool isSeekAfter = false;

	bool isPause = false;
	bool pauseOld = false;
	bool isReverse = false;
	bool reverseOld = false;
	int64_t old_pts = 0;
	int64_t offset_pts = 0;
	int64_t old_offset_pts = 0;
	target_time = 0;
	m_current_pts = 0;

	m_begin = high_resolution_clock::now();

	while (!m_bExit)
	{
		isPause = m_IsPause;
		isReverse = m_bIsRerverse;
#if 1
		if (m_nSpeed == 0)
		{
			nSpeed = 1;
		}
		else
		{
			nSpeed = m_nSpeed;
		}
#endif
		m_end = high_resolution_clock::now();
		tick_diff = duration_cast<microseconds>(m_end - m_begin).count();

		//cout << "[SENDER.ch" << m_nChannel << "] (" << tick_diff << ")/(" << target_time << ")" << endl;
#if 1
		if (tick_diff < target_time)
		{
			usleep(1);
			continue;
		}
#endif
		m_begin = m_end;
		out_diff = target_time - tick_diff;

		if (m_type == "video")
		{
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;
			pkt.pts = 0;

			char isvisible;
#if 0
			if (isPause != pauseOld)
			{
				//상태 변화가 일어났다
				if (isPause == true)
				{
					//
				}
				else if (isPause == false)
				{
				}
			}
			pauseOld = isPause;
#endif
			int size = 0;
			if (isPause == false && m_queue || m_sync_pause_pts > 0)
			{
				m_queue->Enable();
				size = m_queue->GetVideo(&pkt, &offset_pts, &isvisible);
			}
			else if (isPause == true && m_queue)
			{
				m_queue->Disable();
			}

			//cout << "[SENDER.ch" << m_nChannel << "] offset : " << offset_pts << endl;
			if (size > 0)
			{
				m_current_pts = pkt.pts;
				if (old_offset_pts != offset_pts)
				{
					cout << "[SENDER.ch" << m_nChannel << "] !!!!!!!!!!! check !!!!!!!!!!!" << old_offset_pts << " / " << offset_pts << endl;
					old_pts = 0;
					target_time = m_current_pts - offset_pts;
					isSeekAfter = true;
				}
				else
				{
#if 0
					if (isReverse == false)
					{
						if (offset_pts > m_current_pts)
						{
							target_time = 0;
						}
						else
						{
							target_time = llabs((m_current_pts - old_pts)) / nSpeed;
						}
					}
					else
					{
						target_time = llabs((m_current_pts - old_pts)) / nSpeed;
					}
#endif
					target_time = llabs((m_current_pts - old_pts)) / nSpeed;

					if (isReverse == false)
					{
						if (offset_pts > m_current_pts)
						{
							target_time = 0;
						}
					}
				}

				if (isSeekAfter == true)
				{
					isSeekAfter = false;
					if (m_sync_pause_pts < m_current_pts)
					{
						cout << "[SENDER.ch" << m_nChannel << "] sync_pause_pts (" << m_sync_pause_pts << ")/(" << m_current_pts << ")" << endl;
						m_sync_pause_pts = 0;
					}
				}

				if (isPause == false || m_sync_pause_pts > m_current_pts)
				{
					if (send_bitstream(pkt.data, pkt.size, isvisible))
					{
#if 0
						cout << "[SENDER.ch" << m_nChannel << "] send_bitstream (" << m_current_pts << ")/(" << old_pts << ")(" << offset_pts << "), tick : " << tick_diff << ", target_time(" << target_time << "), out_diff : " << out_diff << " sended, (" << pkt.flags << ") " << endl;
#endif
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
			}
			else
			{
				usleep(10);
			}
		}

		else if (m_type == "audio")
		{
			//m_out_pts = ((m_nAudioCount * AV_TIME_BASE) * num / den);

			if (m_queue)
			{
				char status;
				ELEM *pe = NULL;
				if ((isPause == false && m_queue))
				{
					m_queue->Enable();
					pe = (ELEM *)m_queue->GetAudio(&offset_pts);
				}
				else if (isPause == true && m_queue)
				{
					m_queue->Disable();
				}

				if (pe && pe->len > 0)
				{
					memcpy(&m_current_pts, pe->p, 8);
					status = pe->state;

					if (old_offset_pts != offset_pts)
					{
						cout << "[SENDER.Audio.ch" << m_nChannel << "!!!!!!!!!!! check !!!!!!!!" << old_offset_pts << "/" << offset_pts << endl;
						old_pts = 0;
						target_time = m_current_pts - offset_pts;
					}
					else
					{
						target_time = llabs((m_current_pts - old_pts));
					}

					if (m_nSpeed == 0 && isReverse == false)
					{
						if (send_audiostream(pe->p + 8, AUDIO_BUFF_SIZE - 8, status))
						{
#if 0
							cout << "[SENDER.Audio.ch" << m_nChannel << "] send_audiostream (" << m_current_pts << ")/(" << old_pts << "), (" << offset_pts << "), tick : " << tick_diff << ", target_time(" << target_time << "), out_diff : " << out_diff << " sended" << endl;
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
		old_pts = m_current_pts;
		old_offset_pts = offset_pts;
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
			memcpy(&buffer[PACKET_HEADER_SIZE + 14], &m_current_pts, 8);
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
		usleep(200);
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

void CSender::SetPauseSync(int64_t sync_pts)
{
	m_sync_pause_pts = sync_pts;
	cout << "[SENDER.SetPauseSync.ch" << m_nChannel << "] Set sync_pts : " << m_sync_pause_pts << ", current_pts : " << m_current_pts << endl;
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