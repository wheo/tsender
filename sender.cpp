#include "main.h"
#include "sender.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

#define MAX_NUM_bitstream 4
#define MAX_frame_size 1024 * 1024 // 1MB

CSender::CSender(void)
{
	m_bExit = false;
	//pthread_mutex_init(&m_mutex_sender, 0);
}

CSender::~CSender(void)
{
	m_bExit = true;

	Delete();
	_d("[SENDER.ch%d] Trying to exit thread\n", m_nChannel);
	Terminate();
	_d("[SENDER.ch%d] exited...\n", m_nChannel);
	//pthread_mutex_destroy(&m_mutex_sender);
}

void CSender::log(int type, int state)
{
}

bool CSender::Create(Json::Value info, int nChannel)
{
	m_info = info;
	m_nChannel = nChannel;
	m_file_idx = 0;
	m_nSpeed = 1;
	m_pause = false;
	_d("[Sender.ch%d] Thread address : %x\n", m_nChannel, this);

	if (!SetSocket())
	{
		cout << "[SENDER.ch" << m_nChannel << "] SetSocket config is failed" << endl;
		return false;
	}
	cout << "[SENDER.ch" << m_nChannel << "] type : " << m_info["type"].asString() << endl;
	cout << "[SENDER.ch" << m_nChannel << "] SetSocket config is completed" << endl;
	Start();

	return true;
}

bool CSender::SetQueue(CQueue **queue, int nChannel)
{
	m_queue = *queue;
	m_nChannel = nChannel;
	m_queue->SetInfo(m_nChannel, m_info["type"].asString());
	m_queue->Enable();
	cout << cout << "[SENDER.ch" << m_nChannel << "] SetQueue Success" << endl;

	return true;
}

bool CSender::SetSocket()
{
	struct ip_mreq mreq;
	int state;

	memset(&m_mcast_group, 0x00, sizeof(m_mcast_group));
	m_mcast_group.sin_family = AF_INET;
	m_mcast_group.sin_port = htons(m_info["port"].asInt());
	m_mcast_group.sin_addr.s_addr = inet_addr(m_info["ip"].asString().c_str());

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

	mreq.imr_multiaddr = m_mcast_group.sin_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

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

void CSender::Run()
{
	while (!m_bExit)
	{
		if (!Send())
		{
			//error send() return false
		}
	}
}

bool CSender::Send()
{
	double fTime = m_info["fps"].asDouble();
	double num = m_info["num"].asDouble();
	double den = m_info["den"].asDouble();
	double target_time = num / den * 1000000;

	int64_t acc_time = 0;
	int64_t tick_diff = 0;
	high_resolution_clock::time_point begin;
	high_resolution_clock::time_point end;
	cout << "[SENDER.ch" << m_nChannel << "] " << target_time << ", " << num << ", " << den << ", type : " << m_info["type"].asString() << endl;

	if (m_info["type"].empty())
	{
		cout << "[SENDER.ch" << m_nChannel << "] type not setting yet" << endl;
		return false;
	}

	begin = high_resolution_clock::now();
	while (!m_bExit)
	{
		if (m_info["type"].asString() == "video")
		{
			AVPacket pkt;
			av_init_packet(&pkt);
#if 0
			if (av_read_frame(fmt_ctx, &pkt) < 0)
			{
				cout << "[SENDER.ch" << m_nChannel << "] meet EOF" << endl;
				avformat_close_input(&fmt_ctx);
				break;
			}
#endif
			if (!m_queue->Get(&pkt))
			{
				//cout << "[SENDER.ch" << m_nChannel << "] GetVideo packet failed" << endl;
			}
			else
			{
				//cout << "[SENDER.ch" << m_nChannel << "] GetVideo packet success (" << m_queue->GetVideoPacketSize() << ")" << endl;
				if (!send_bitstream(pkt.data, pkt.size))
				{
					cout << "[SENDER.ch" << m_nChannel << "] send_bitstream failed" << endl;
				}
				else
				{
					//cout << "[SENDER.ch" << m_nChannel << "] send_bitstream success" << endl;
				}
				//av_packet_unref(&pkt);
				m_queue->Ret(&pkt);
			}
		}
		else if (m_info["type"].asString() == "audio")
		{
#if 0
			//char audio_buf[AUDIO_BUFF_SIZE];
			//ifs.read(audio_buf, AUDIO_BUFF_SIZE);
			//cout << hex << audio_buf << endl;
			ELEM *pe;
			pe->p = (char *)m_queue->GetAudio();
			if (!send_audiostream(pe->p, pe->len))
			{
				cout << "[SENDER.ch" << m_nChannel << "] send_audiostream failed" << endl;
			}
			else
			{
				cout << "[SENDER.ch" << m_nChannel << "] send_audiostream success" << endl;
			}
			m_queue->RetAudio(pe);
#endif
		}

		if (m_info["type"].asString() == "video")
		{
			target_time = num / den * 1000000 / m_nSpeed;
		}

		while (tick_diff < target_time)
		{
			//usleep(1); // 1000000 us = 1 sec
			end = high_resolution_clock::now();
			tick_diff = duration_cast<microseconds>(end - begin).count();
			this_thread::sleep_for(microseconds(1000));
		}
		begin = end;

		tick_diff = 0;
	}
	return true;
}

bool CSender::send_audiostream(char *buff, int size)
{
	int nSendto = 0;
	nSendto = sendto(m_sock, buff, size, 0, (struct sockaddr *)&m_mcast_group, sizeof(m_mcast_group));
	if (nSendto > 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CSender::send_bitstream(uint8_t *stream, int size)
{
	int tot_packet = 0;
	int cur_packet = 1;

	int tot_size;
	int cur_size;

	int remain;

	int nSendto;

	char video_type = 0;	   // 1(NTSC), 2(PAL), 3(PANORAMA), 4(FOCUS)
	char video_codec_type = 0; // 1(HEVC), 2(H264)
	char video_frame_type = 0; // 0(P frame), 1(I frame)
	char reserve[5] = {
		0,
	};

	uint8_t *p = stream;
	uint8_t buffer[PACKET_HEADER_SIZE + PACKET_SIZE];

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
		memcpy(&buffer[24], p, cur_size);

		nSendto = sendto(m_sock, buffer, PACKET_HEADER_SIZE + cur_size, 0, (struct sockaddr *)&m_mcast_group, sizeof(m_mcast_group));
		if (nSendto < 0)
		{
			return false;
		}
		else
		{
#if 0
			cout << "[SENDER.ch" << m_nChannel << "] "
				 << "tot_size : " << tot_size << ", cur_size : " << cur_size << ", tot_packet : " << tot_packet << ", cur_packet : " << cur_packet << ", nSendto : " << nSendto << endl;
#endif
		}

		remain -= cur_size;
		p += cur_size;
		cur_packet++;
	}

	return true;
}

void CSender::SetSpeed(int speed)
{
	m_nSpeed = speed;
	cout << "[SENDER.ch" << m_nChannel << "] Set speed : " << m_nSpeed << endl;
}

void CSender::SetPause()
{
	m_pause = !m_pause;
}

void CSender::SetReverse()
{
	// SetReverse
}

void CSender::Delete()
{
	cout << "[SENDER.ch" << m_nChannel << "] sock " << m_sock << " closed" << endl;
	close(m_sock);
}
