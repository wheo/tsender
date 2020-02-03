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
	m_file_idx = 0;
	m_nSpeed = 1;
	m_nSeekFrame = 0;
	m_is_pframe_skip = false;
	m_current_pts = 0;
	//m_sync_pts = 0;
	//m_sync_cnt = 0;
	m_IsPause = false;
	m_bIsRerverse = false;
	m_IsMove = false;
	m_nTotalFrame = 0;
	m_nTotalSec = 0;
	m_file_cnt = 0;
	m_files = NULL;
	m_file_first_pts = 0;
	m_out_pts = 0;
	m_next_keyframe = false;
	m_seek_pts = 0;
	m_seek_old_pts = 0;
	m_dur_pts = 0;
	m_dur_cnt = 0;

	//uint64_t num = m_info["num"].asUInt64();
	//uint64_t den = m_info["den"].asUInt64();

	m_nAudioCount = 0;
	refcount = 0; // 1과 0의 차이를 알아보자

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
	_d("[SENDER.ch%d] SetAVFormatContext : %x ...\n", m_nChannel, m_fmt_ctx);
}

void CSender::Run()
{
	//Play();
	int keyframe_speed = m_attr["keyframe_speed"].asInt();
	m_now_pts = 0;
	m_timer = 0;
	m_seek_pts = 0;
	m_add_pts = 0;

	uint64_t old_now_pts = 0;
	uint64_t now_diff = 0;
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();
	uint gop = m_info["gop"].asUInt();
	uint64_t old_pts = 0;
	uint64_t pts_diff = 0;

	m_begin = high_resolution_clock::now();
	AVStream *pStream = NULL;
	AVRational timeBase;
	timeBase.num = num;
	timeBase.den = den;

	int skip_cnt = 0;

	int nSpeed = 1;
	int nOldSpeed = 1;

	m_bIsTimerReset = false;

	bool reverseState = false;
	bool isReverse = false;
	bool isPause = false;
	bool pauseOld = false;
	uint64_t pause_time = 0;

	uint64_t pause_cnt = 0;
	uint64_t pause_diff = 0;

	while (!m_bExit)
	{
		nSpeed = m_nSpeed;
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		pkt.pts = 0;

		isReverse = m_bIsRerverse;
		isPause = m_IsPause;
		// seek_pts 현재 지점 m_start_pts는 now()

		if (isReverse == false)
		{
			m_now_pts = m_add_pts + m_seek_pts + (m_dur_cnt * m_dur_pts);
		}
		else
		{
			m_now_pts = m_add_pts + m_seek_pts - (m_dur_cnt * m_dur_pts);
		}

		this_thread::sleep_for(microseconds(1));

		if (m_seek_pts != m_seek_old_pts)
		{
			//구간점프 시간이 달라지면 타이머 리셋
			m_begin = high_resolution_clock::now();
			cout << "[SENDER.ch" << m_nChannel << "] seek diff timer reset" << endl;
			m_add_pts = 0;
			m_dur_pts = 0;
			m_dur_cnt = 0;
			skip_cnt = gop;
		}

		//방향이 바뀐것을 탐지했을 때
		if (reverseState != isReverse)
		{
			//역방향 재생시
			if (m_bIsRerverse == true)
			{
				//역방향 재생 때 seek_time이 발생했을 때
				// out_pts를 가져오지 못함
				m_out_pts = (((m_current_pts * timeBase.num) * AV_TIME_BASE) / timeBase.den);
#if 0
				if (m_seek_pts != m_seek_old_pts)
				{
					cout << "[SENDER.ch" << m_nChannel << "] 역방향시 구간이동 함" << endl;
				}
#endif
				//현재 시간을 add_pts에 누적
				m_add_pts = m_add_pts + m_timer;

				cout << "[SENDER.ch" << m_nChannel << "] m_current_pts : " << m_current_pts << " ,m_add_pts : " << m_add_pts << ", m_out_pts : " << m_out_pts << ", timer : " << m_timer << endl;
			}
			else
			{
				// 정방향 재생시 add_pts에 output_pts를 넣음
				m_add_pts = m_out_pts;
				if (m_seek_pts > 0)
				{
					m_add_pts = m_add_pts - m_seek_pts;
				}
			}

			//방향이 바뀌면 타이머는 리셋하고 큐를 비움
			m_begin = high_resolution_clock::now();
			cout << "[SENDER.ch" << m_nChannel << "] reverse diff timer reset" << endl;
			m_queue->Clear();

			m_dur_pts = 0;
			m_dur_cnt = 0;
		}

		m_seek_old_pts = m_seek_pts;
		reverseState = isReverse;

		if (isReverse == false)
		{
			m_now_pts = m_now_pts + m_timer;
			// out_pts <= now_pts;
			if (m_out_pts <= m_now_pts)
			{
				m_bSend = true;
			}
			else
			{
				m_bSend = false;
			}
		}
		else
		{
			m_now_pts = m_now_pts - m_timer;
			// out_pts > now_pts;
			if (m_out_pts > m_now_pts)
			{
				m_bSend = true;
			}
			else
			{
				m_bSend = false;
			}
		}

		m_end = high_resolution_clock::now();
		m_timer = duration_cast<microseconds>(m_end - m_begin).count();

		//cout << "[SENDER.ch" << m_nChannel << "] timer : " << m_timer << ", now : " << m_now_pts << ", out : " << m_out_pts << ", reverse : " << isReverse << ", seek : " << m_seek_pts << ", 보정 : " << m_interpolation_pts << endl;
		//cout << "[SENDER.ch" << m_nChannel << "] timer : " << m_timer << ", now : " << m_now_pts << ", out : " << m_out_pts << endl;
		if (m_bSend == true)
		{
			if (m_info["type"].asString() == "video")
			{
				if (m_queue && m_fmt_ctx)
				{
					if (m_queue->GetVideo(&pkt, &m_seek_pts) > 0)
					{
						if (m_fmt_ctx)
						{
							if (pStream == NULL)
							{
								pStream = m_fmt_ctx->streams[0];
								timeBase = pStream->time_base;
							}
						}

						if (isPause != pauseOld)
						{
							//상태 변화가 일어났다
							if (isPause == true)
							{
								pause_time = m_timer;
								cout << "[SENDER.ch" << m_nChannel << "] pause_time : " << pause_time << endl;
							}
							else
							{
								m_add_pts = m_add_pts - m_timer + pause_time;
								m_now_pts = m_out_pts - ((timeBase.num * AV_TIME_BASE) / timeBase.den);
								pause_time = 0;
							}
						}
						pauseOld = isPause;

						pts_diff = llabs(pkt.pts - old_pts);
						now_diff = llabs(m_now_pts - old_now_pts);

						m_out_pts = (((pkt.pts * timeBase.num) * AV_TIME_BASE) / timeBase.den);

						if (nSpeed != nOldSpeed)
						{
							if (isReverse == false)
							{
								//빨리 감기를 한 시간을 누적함
								m_add_pts = m_add_pts + (m_dur_pts * m_dur_cnt);
							}
							else if (isReverse == true)
							{
								//되감기를 한 시간을 누적함
								//m_add_pts = m_add_pts - (m_dur_pts * m_dur_cnt);
								//m_add_pts = 0;
								cout << "speed diff & reverse true" << endl;
							}
							cout << "[SENDER.ch" << m_nChannel << "] m_add_pts : " << m_add_pts << ", m_dur_pts : " << m_dur_pts << ", m_dur_cnt : " << m_dur_cnt << endl;
							m_dur_pts = 0;
							m_dur_cnt = 0;
						}
						if (nSpeed > 1)
						{
							m_dur_pts = ((num * AV_TIME_BASE) / den);
							m_dur_pts = m_dur_pts - (m_dur_pts / m_nSpeed);
							m_dur_cnt++;
						}
						nOldSpeed = nSpeed;

						if (skip_cnt > 0)
						{
							skip_cnt--;
						}
						else
						{
							if (keyframe_speed > nSpeed)
							{
								m_is_pframe_skip = false;
							}
							else
							{
								m_is_pframe_skip = true;
							}
							if (m_is_pframe_skip == false || pkt.flags == AV_PKT_FLAG_KEY)
							{
								if (pkt.pts != old_pts)
								{
									if (send_bitstream(pkt.data, pkt.size))
									{
										//cout << "[SENDER.ch" << m_nChannel << "] send_bitstream (" << pkt.pts << ") sended" << endl;
										cout << "[SENDER.ch" << m_nChannel << "] dur_cnt : " << m_dur_cnt << ", dur_pts : " << m_dur_pts << ", add_pts : " << m_add_pts << ", seek_pts : " << m_seek_pts << ", pkt.pts : " << pkt.pts << ", timer : " << m_timer << ", now_pts : " << m_now_pts << ", out_pts : " << m_out_pts << ", pts_diff : " << pts_diff << ", now_diff : " << now_diff << " (" << timeBase.num << "/" << timeBase.den << ") sended" << endl;
										old_now_pts = m_now_pts;
									}
									else
									{
										cout << "[SENDER.ch" << m_nChannel << "] send_bitstream failed" << endl;
									}
								}
							}
						}

						m_current_pts = pkt.pts;
						old_pts = pkt.pts;

						if (m_queue)
						{
							m_queue->RetVideo(&pkt);
						}
					}
				}
			}

			else if (m_info["type"].asString() == "audio")
			{
				m_out_pts = ((m_nAudioCount * AV_TIME_BASE) * num / den);
				if (m_queue)
				{
					ELEM *pe = (ELEM *)m_queue->GetAudio(&m_seek_pts);
					if (pe && pe->len > 0)
					{
						if (nSpeed != nOldSpeed)
						{
							cout << "[SENDER.ch" << m_nChannel << "] here" << endl;
							m_add_pts = m_add_pts + (m_dur_pts * m_dur_cnt);
							m_dur_pts = 0;
							m_dur_cnt = 0;
						}
						if (nSpeed > 1)
						{
							m_dur_pts = ((num * AV_TIME_BASE) / den);
							m_dur_pts = m_dur_pts - (m_dur_pts / m_nSpeed);
							m_dur_cnt++;
						}

						nOldSpeed = nSpeed;

						m_nAudioCount++;
						m_current_pts = m_nAudioCount;
						if (nSpeed == 1 && isReverse == false)
						{
							if (send_audiostream(pe->p, AUDIO_BUFF_SIZE))
							{
#if 1
								cout << "[SENDER.ch" << m_nChannel << "] send_audiotream sended(" << AUDIO_BUFF_SIZE << "), AudioCount : " << m_nAudioCount << endl;
								cout << "[SENDER.ch" << m_nChannel << "] padding_cnt : " << m_dur_cnt << ", padding_pts : " << m_dur_pts << ", interpolation : " << m_add_pts << ", seek_pts : " << m_seek_pts << ", now_pts : " << m_now_pts << ", out_pts : " << m_out_pts << ", pts_diff : " << pts_diff << ", now_diff : " << now_diff << " sended" << endl;
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
		}
	}
}

#if 0
bool CSender::Play()
{
	stringstream sstm;
	sstm.str("");
	sstm << m_attr["file_dst"].asString() << "/" << m_attr["target"].asString() << "/" << m_nChannel;
	//cout << "[DEMUXER.ch" << m_nChannel << "] target : " << sstm.str() << endl;
	if (!GetChannelFiles(sstm.str()))
	{
		cout << "[DEMUXER.ch" << m_nChannel << "] delete this" << endl;
		delete this;
	}
	return true;
}
#endif
#if 0
	bool CSender::GetChannelFiles(string path)
	{
		if (!IsDirExist(path))
		{
			cout << "[DEMUXER.ch" << m_nChannel << "] " << path << " is not exist" << endl;
			return false;
		}

		Json::Reader reader;
		Json::Value meta;

		ifstream ifs(path + "/" + "meta.json");
		if (!ifs.is_open())
		{
			//if not open or not exist
			cout << "[SENDER.ch" << m_nChannel << "] " << path + "/" + "meta.json"
				 << " is not exist" << endl;
			return false;
		}
		else
		{
			if (reader.parse(ifs, meta, true))
			{
				// parse success
				ifs.close();
				m_attr["type"] = meta["type"]; // video or audio
				m_CSender->SetAttribute(m_attr);
			}
			else
			{
				// parse failed
				cout << "[SENDER.ch" << m_nChannel << "] " << path + "/" + "meta.json"
					 << " parse failed" << endl;
				ifs.close();
				return false;
			}
		}

		if (Demux(meta["files"]))
		{
			cout << "[SENDER.ch" << m_nChannel << "] success" << endl;
		}
		return true;
	}
#endif

#if 0
int CSender::Demux(Json::Value files)
{
	string src_filename = "";
	Json::Value value;
	//m_nFrameCount = 0;
	m_seek_pts = 0;
	// file 데이터 전역변수로 복사
	m_files = files;

	for (int i = 0; i < files.size(); i++)
	{
		m_nTotalFrame += files[i]["frame"].asInt();
		m_file_cnt++;
	}

	int codec = m_attr["codec"].asInt(); // 0 : H264, 1 : HEVC
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();

	//m_nTotalSec = m_nTotalFrame * num / den;

	//cout << "[DEMUXER.ch" << m_nChannel << "] totalFrame : " << m_nTotalFrame << ", totalSec : " << m_nTotalSec << ", filecnt : " << m_file_cnt << endl;
	cout << "[DEMUXER.ch" << m_nChannel << "] totalFrame : " << m_nTotalFrame << ", filecnt : " << m_file_cnt << endl;

	//m_start_pts = high_resolution_clock::now();
	//high_resolution_clock::time_point current_pts;

	int64_t out_base = 0;
	int64_t pts_diff = 0;
	int64_t out_old_pts = 0;
	m_out_pts = 0;
	uint64_t old_pts = 0;
	int64_t now_pts = 0;
	int64_t old_now_pts = 0;
	int64_t now_diff = 0;
	int key_frame_count = 0;

	for (int i = 0; i < files.size(); i++)
	{
		value = files[i];

		if (m_bExit == true)
		{
			break;
		}
		uint64_t last_frame = value["frame"].asInt64();
		int keyframe_speed = m_attr["keyframe_speed"].asInt();

		if (last_frame == 0)
		{
			break;
		}

		src_filename = value["name"].asString();
		string type = m_info["type"].asString();
		ifstream ifs;

		int ret = 0;

		//cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << ", " << num << ", " << den << ", type : " << type << endl;

		if (type == "video")
		{
			if (avformat_open_input(&fmt_ctx, src_filename.c_str(), NULL, NULL) < 0)
			{
				cout << "[SENDER.ch" << m_nChannel << "] Could not open source file : " << src_filename << endl;
				return false;
			}
			else
			{
				m_currentDuration = fmt_ctx->duration;
				cout << "[SENDER.ch" << m_nChannel << "] " << src_filename << " file is opened (" << m_currentDuration << ")" << endl;
			}

			ret = avformat_find_stream_info(fmt_ctx, NULL);
			if (ret < 0)
			{
				cout << "[SENDER.ch" << m_nChannel << "] failed to find proper stream in FILE : " << src_filename << endl;
			}

			if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0)
			{
				video_stream = fmt_ctx->streams[video_stream_idx];

				/* dump input information to stderr */
				av_dump_format(fmt_ctx, 0, src_filename.c_str(), 0);

				if (!video_stream)
				{
					fprintf(stderr, "[DEMUXER] Could not find audio or video stream in the input, aborting\n");
					ret = 1;
				}
			}
			if (codec == 0)
			{
				m_bsf = av_bsf_get_by_name("h264_metadata");
			}
			else if (codec == 1)
			{
				m_bsf = av_bsf_get_by_name("hevc_metadata");
			}

			if (i == 0) // 첫번째 파일일 경우에만 필터 초기화를 할 것!!!
			{
				if (av_bsf_alloc(m_bsf, &m_bsfc) < 0)
				{
					_d("[SENDER] Failed to alloc bsfc\n");
					return false;
				}
				if (avcodec_parameters_copy(m_bsfc->par_in, fmt_ctx->streams[0]->codecpar) < 0)
				{
					_d("[SENDER] Failed to copy codec param.\n");
					return false;
				}
				m_bsfc->time_base_in = fmt_ctx->streams[0]->time_base;

				if (av_bsf_init(m_bsfc) < 0)
				{
					_d("[SENDER] Failed to init bsfc\n");
					return false;
				}
			}
		}
		else if (type == "audio")
		{
			// fileopen
			ifs.open(src_filename, ios::in);
			if (!ifs.is_open())
			{
				cout << "[SENDER.ch" << m_nChannel << "] " << src_filename << " file open failed" << endl;
				return false;
			}
			else
			{
				cout << "[SENDER.ch" << m_nChannel << "] " << src_filename << " file open success" << endl;
			}
			//_d("[DEMUXER.ch%d] fduration : %3f\n", m_nChannel, m_fDuration);
		}

		int64_t ts = 0;
		int readcnt = 0;
#if 0
		m_CThumbnail->Create(m_info, m_attr, m_nChannel);
#endif
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		pkt.pts = 0;

		m_file_first_pts = 0;
		int nSpeed = 1;

		while (!m_bExit)
		{
			//current_pts = high_resolution_clock::now();
			//now_pts = duration_cast<microseconds>(current_pts - m_start_pts).count();
			//now_pts = m_seek_pts + now_pts;
			// seek_pts 현재 지점 m_start_pts는 now()
			this_thread::sleep_for(microseconds(1));
			//cout << "[DEMUXER.ch" << m_nChannel << "] pts : " << pkt.pts << ", now_pts : " << now_pts << ", out_pts : " << m_out_pts << endl;

			if (m_out_pts <= now_pts)
			{
				now_diff = now_pts - old_now_pts;
				old_now_pts = now_pts;

				if (type == "video")
				{
					if (m_IsMove == true)
					{
						m_IsMove = false;
						if (m_bIsRerverse == false)
						{
							i = m_nMoveIdx - 1;
						}
						else
						{
							i = m_nMoveIdx;
						}
						cout << "[SENDER.ch" << m_nChannel << "] set index : " << i << endl;
						break;
					}

					if (m_nSeekFrame > 0)
					{
						SeekFrame(m_nSeekFrame);
						m_nSeekFrame = 0;
					}

					if (m_bIsRerverse)
					{
						if (m_reverse_count > 0)
						{
						}
						else
						{
							m_reverse_pts = pkt.pts;
						}
						m_reverse_count++;
						if (Reverse())
						{
							//Reverse continue
							cout << "[SENDER.ch" << m_nChannel << "] Reverse continue" << endl;
						}
						else
						{
							i = i - 2; // 직전 인덱스로 이동
							//m_nFrameCount = last_frame;
							//m_reverse_count = 0;
							cout << "[SENDER.ch" << m_nChannel << "] set index : " << i << endl;
							break;
						}
					}
					else
					{
						m_reverse_count = 0;
					}
					//nSpeed = m_nSpeed;

					av_packet_unref(&pkt);
					av_init_packet(&pkt);

					pkt.data = NULL;
					pkt.size = 0;
					pkt.pts = 0;
					if (m_IsPause == false)
					{
						if (av_read_frame(fmt_ctx, &pkt) < 0)
						{
							cout << "[SENDER.ch" << m_nChannel << "] meet EOF(" << fmt_ctx->filename << endl;
							avformat_close_input(&fmt_ctx);
							fmt_ctx = NULL;
							break;
						}
						else
						{
							AVStream *pStream = fmt_ctx->streams[0];
							AVRational timeBase = pStream->time_base;
							if (m_file_first_pts == 0)
							{
								m_file_first_pts = fmt_ctx->start_time;
							}

							if (m_bIsRerverse == false)
							{
								out_base = ((pkt.pts * AV_TIME_BASE) / timeBase.den);
								pts_diff = out_base - out_old_pts;
								m_out_pts = m_out_pts + (pts_diff / m_nSpeed);
							}

							out_old_pts = out_base;

							cout << "[SENDER.ch" << m_nChannel << "] first pts : " << m_file_first_pts << ", pts : " << pkt.pts << ", now_tick : " << now_pts << ", out_pts : " << m_out_pts << ", out_base : " << out_base << ", pts_diff : " << pts_diff << " (" << timeBase.num << "/" << timeBase.den << "), now_diff : " << now_diff << endl;
						}
					}
					else
					{
						m_seek_pts = m_out_pts;
						m_start_pts = high_resolution_clock::now();
						continue;
					}

					if (old_pts != pkt.pts && m_IsPause == false)
					{
						if ((ret = av_bsf_send_packet(m_bsfc, &pkt)) < 0)
						{
							av_log(fmt_ctx, AV_LOG_ERROR, "Failed to send packet to filter %s for stream %d\n", m_bsfc->filter->name, pkt.stream_index);
							cout << "[SENDER.ch" << m_nChannel << "] current pts : " << pkt.pts << endl;
						}
						// TODO: when any automatically-added bitstream filter is generating multiple
						// output packets for a single input one, we'll need to call this in a loop
						// and write each output packet.
						if ((ret = av_bsf_receive_packet(m_bsfc, &pkt)) < 0)
						{
							if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
							{
								av_log(fmt_ctx, AV_LOG_ERROR, "Failed to receive packet from filter %s for stream %d\n", m_bsfc->filter->name, pkt.stream_index);
								cout << "[SENDER.ch" << m_nChannel << "] current pts : " << pkt.pts << endl;
							}
							if (fmt_ctx->error_recognition & AV_EF_EXPLODE)
							{
								//
							}
						}
						if (pkt.size > 0)
						{
							if (keyframe_speed < m_nSpeed)
							{
								if (pkt.flags == AV_PKT_FLAG_KEY)
								{
									m_is_skip = false;
								}
								else
								{
									m_is_skip = true;
								}
							}
							else
							{
								m_is_skip = false;
							}

							if (m_is_skip == false)
							{
								if (!send_bitstream(pkt.data, pkt.size))
								{
									cout << "[SENDER.ch" << m_nChannel << "] send_bitstream failed" << endl;
								}
								else
								{
									cout << "[SENDER.ch" << m_nChannel << "] send_bitstream (" << pkt.pts << ") sended" << endl;
								}
							}
						}
					}
					old_pts = pkt.pts;
				}
				else if (type == "audio")
				{
					char audio_buf[AUDIO_BUFF_SIZE];

					if (m_IsMove == true)
					{
						m_IsMove = false;
						i = m_nMoveIdx - 1;
						break;
					}

					if (m_nSeekFrame > 0)
					{
						m_nAudioCount = m_nSeekFrame;
						m_nSeekFrame = m_nSeekFrame - (last_frame * m_nMoveIdx);

						cout << "[SENDER.ch" << m_nChannel << "] move audio frame (" << m_nSeekFrame << ")" << endl;
						//MoveFrame(m_nMoveFrame);
						ifs.seekg(m_nSeekFrame * AUDIO_BUFF_SIZE);
						m_nSeekFrame = 0;
						//m_seek_pts = (m_nAudioCount * AV_TIME_BASE * num) / den;
						//m_start_pts = high_resolution_clock::now();
					}

#if 0
					if (m_bIsRerverse)
					{
						if (m_reverse_count > 0)
						{
						}
						else
						{
							m_reverse_pts = pkt.pts;
						}
						m_reverse_count++;
						if (Reverse())
						{
							//Reverse continue
							cout << "[DEMUXER.ch" << m_nChannel << "] Reverse continue" << endl;
						}
						else
						{
							i = i - 2; // 직전 인덱스로 이동
							//m_nFrameCount = last_frame;
							//m_reverse_count = 0;
							cout << "[DEMUXER.ch" << m_nChannel << "] set index : " << i << endl;
							break;
						}
					}
					else
					{
						m_reverse_count = 0;
					}
#endif

					if (m_IsPause == false)
					{
						ifs.read(audio_buf, AUDIO_BUFF_SIZE);
						m_nAudioCount++;
					}
					else
					{
						m_seek_pts = m_out_pts;
						m_start_pts = high_resolution_clock::now();
						continue;
					}
					if (m_bIsRerverse == false)
					{
						out_base = (m_nAudioCount * AV_TIME_BASE) * num / den;
						pts_diff = out_base - out_old_pts;
						m_out_pts = m_out_pts + (pts_diff / m_nSpeed);
					}

					out_old_pts = out_base;

					cout << "[SENDER.ch" << m_nChannel << "] audio : " << m_nAudioCount << ", now_tick : " << now_pts << ", out_pts : " << m_out_pts << ", out_base : " << out_base << ", pts_diff : " << pts_diff << " (" << num << "/" << den << ")" << endl;

					if (ifs.eof())
					{
						ifs.close();
						cout << "[SENDER.ch" << m_nChannel << "] audio meet eof" << endl;
						break;
					}

					if (m_IsPause == false && m_bIsRerverse == false && m_nSpeed == 1)
					{
						//legacy code
						//m_CQueue->PutAudio(audio_buf, AUDIO_BUFF_SIZE);
						if (!send_audiostream(audio_buf, AUDIO_BUFF_SIZE))
						{
							cout << "[SENDER.ch" << m_nChannel << "] send_audiotream failed" << endl;
						}
						else
						{
#if 1
							cout << "[SENDER.ch" << m_nChannel << "] send_audiotream sended(" << AUDIO_BUFF_SIZE << "), AudioCount : " << m_nAudioCount << endl;
#endif
						}

						//if meet eof then break
					}
					else
					{
					}
				}
			}
		}

		if (type == "video" && fmt_ctx)
		{
			cout << "[SENDER.ch" << m_nChannel << "] avformat_close_input (" << fmt_ctx->filename << endl;
			avformat_close_input(&fmt_ctx);
			fmt_ctx = NULL;
		}
		if (type == "audio" && ifs.is_open())
		{
			ifs.close();
		}
		cout << "[SENDER.ch" << m_nChannel << "] " << src_filename << " file is closed" << endl;
	}

	av_bsf_free(&m_bsfc);
	return true;
}
#endif

bool CSender::SetMoveSec(int nSec)
{
	double ret;
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();
	uint64_t nFrame = (nSec * den) / num;
	m_nMoveIdx = FindFileIndexFromFrame(nFrame);
	cout << "[%SENDER.ch" << m_nChannel << "] input frame : " << nFrame << ", move index : " << m_nMoveIdx << endl;

	m_nSeekFrame = nFrame;
	m_IsMove = true;
}

#if 0
bool CSender::SeekFrame(int nFrame)
{
	uint64_t num = m_info["num"].asInt();
	uint64_t den = m_info["den"].asInt();

	int ret = 0;
	double fTime = 0;
	AVStream *pStream = fmt_ctx->streams[0];
	//_d("[DEMUXER.ch%d] nbstream : %d\n", m_nChannel, fmt_ctx->nb_streams);
	//int nFrame = nSec * m_fps;
	fTime = (((double)nFrame * pStream->avg_frame_rate.den) / pStream->avg_frame_rate.num) - 0.5;
	//fTime = (double)nFrame * (double)den / (double)num;
	fTime = max(fTime, 0.);

	AVRational timeBaseQ;
	AVRational timeBase = pStream->time_base;

	timeBaseQ.num = 1;
	timeBaseQ.den = AV_TIME_BASE;

	int64_t tm = (int64_t)(fTime * AV_TIME_BASE);
	//_d("before tm : %lld\n", tm);
	tm = av_rescale_q(tm, timeBaseQ, timeBase);
	//int64_t seek_target = av_rescale((nSec * AV_TIME_BASE), fmt_ctx->streams[0]->time_base.den, fmt_ctx->streams[0]->time_base.num);

	if (m_nChannel < 6)
	{
		_d("[DEMUXER.ch%d] nFrame : %d, tm : %lld, fTime : %.3f\n", m_nChannel, nFrame, tm, fTime);
		//ret = avformat_seek_file(fmt_ctx, 0, 0, tm, tm, 0);
		//avcodec_flush_buffers(fmt_ctx->streams[0]->codec);
		ret = avformat_seek_file(fmt_ctx, 0, 0, tm, tm, AVSEEK_FLAG_FRAME);
		_d("[DEMUXER.ch%d] ret : %d , timebaseQ : %d/%d, timebase : %d/%d\n", m_nChannel, ret, timeBaseQ.num, timeBaseQ.den, timeBase.num, timeBase.den);
		if (m_bIsRerverse == true)
		{
			m_reverse_pts = tm;
		}
		m_seek_pts = tm * AV_TIME_BASE / timeBase.den;
		//m_start_pts = high_resolution_clock::now();
	}

	//m_nFrameCount = nFrame;
	//cout << "[DEMUXER.ch" << m_nChannel << "] Set m_nFrameCount : " << m_nFrameCount << endl;
	//m_nMoveSec = 0;
}
#endif

#if 0
bool CSender::Reverse()
{
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();

	//int nFrame = m_nFrameCount;
#if 0
	double fTime = 0;
	AVStream *pStream = fmt_ctx->streams[0];
	fTime = (((double)(nFrame)*pStream->avg_frame_rate.den) / pStream->avg_frame_rate.num) - 0.5;

	fTime = max(fTime, 0.);

	AVRational timeBaseQ;
	AVRational timeBase = pStream->time_base;

	timeBaseQ.num = 1;
	timeBaseQ.den = AV_TIME_BASE;

	//int64_t tm = (int64_t)(fTime * AV_TIME_BASE);
	//tm = av_rescale_q(tm, timeBaseQ, timeBase);
#endif

	//avcodec_flush_buffers(fmt_ctx->streams[0]->codec);
	//_d("[DEMUXER.ch%d] m_nFrameCount : %d, tm : %lld, fTime : %.3f\n", m_nChannel, m_nFrameCount, tm, fTime);
	uint64_t pts_diff = 0;
	int ret = 0;
	AVStream *pStream = fmt_ctx->streams[0];
	AVRational timeBase = pStream->time_base;

	pts_diff = (num * AV_TIME_BASE) / den;

	cout << "[" << m_nChannel << "] (" << timeBase.num << "/" << timeBase.den << ") first_pts : " << m_file_first_pts << ", reverse_pts(AV_TIME_BASE) : " << ((m_reverse_pts / timeBase.den) * AV_TIME_BASE) << ", reverse_count : " << m_reverse_count << ", pts_diff : " << pts_diff << endl;

	if (m_nChannel < 6)
	{
#if 0
//참고
		out_base = ((pkt.pts * AV_TIME_BASE) / timeBase.den);
		pts_diff = out_base - out_old_pts;
		m_out_pts = m_out_pts + (pts_diff / m_nSpeed);
#endif
		m_reverse_pts = m_reverse_pts - ((timeBase.den / den) * num);
		m_out_pts = ((m_reverse_pts * AV_TIME_BASE) / timeBase.den) + (pts_diff / m_nSpeed);
		m_seek_pts = m_reverse_pts * AV_TIME_BASE / timeBase.den; // m_seek_pts : 탐색시작 pts
		//m_start_pts = high_resolution_clock::now();				  // 이걸 현시간으로 바꾸면 now_pts가 0부터 시작

		ret = avformat_seek_file(fmt_ctx, 0, 0, m_reverse_pts, m_reverse_pts, AVSEEK_FLAG_FRAME);
		if (ret < 0)
		{
			_d("[REVERSE.ch%d] ret : %d, not work(m_reverse_pts : %lld)\n", m_nChannel, ret, m_reverse_pts);
			return false;
		}
#if 1
		if (m_file_first_pts > ((m_reverse_pts / timeBase.den) * AV_TIME_BASE))
		{
			_d("[REVERSE.ch%d] m_file_first_pts : %lld (m_reverse_pts(AV_TIME_BASE) : %lld)\n", m_nChannel, m_file_first_pts, ((m_reverse_pts / timeBase.den) * AV_TIME_BASE));
			return false;
		}
#endif
	}

	return true;
}
#endif

int CSender::FindFileIndexFromFrame(uint64_t nFrame)
{
	int ret_idx = 0;
	for (int i = 0; i < m_files.size(); i++)
	{
		int file_frame = m_files[i]["frame"].asInt();
		if (nFrame < file_frame)
		{
			break;
		}
		else
		{
			nFrame = nFrame - file_frame;
			ret_idx++;
		}
	}
	return ret_idx;
}
#if 0
Json::Value CSender::GetThumbnail(int nSec)
{
	Json::Value ret_json;
	ret_json = m_CThumbnail->GetThumbnail(nSec);
	return json;
}
#endif

#if 0
bool CDemuxer::GetOutputs(string basepath)
{
	string path;
	DIR *dir = opendir(basepath.c_str());
	struct dirent *ent;

	string channel;

	channel = to_string(m_nChannel);

	while ((ent = readdir(dir)) != NULL && !m_bExit)
	{
		//cout << "d_name : " << ent->d_name << endl;
		if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
		{
			if (ent->d_type == DT_DIR)
			{
				path.clear();
				path.append(basepath);
				path.append("/");
				path.append(ent->d_name);
				//cout << "path : " << path;
				//cout << "string channel : " << channel << ", ent->d_name : " << ent->d_name << ", path : " << path << endl;
				if (channel.compare(ent->d_name) == 0)
				{
					cout << "channel : " << channel << " : " << path << endl;
					GetChannelFiles(path);
				}
				GetOutputs(path);
			}
			else
			{
				//cout << "channel : " << channel << ", " << basepath << "/" << ent->d_name << endl;
			}
		}
	}
	closedir(dir);
	return EXIT_SUCCESS;
}
#endif

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
			memcpy(&buffer[24 + 5], &m_current_pts, 8);
			memcpy(&buffer[24 + 13], &m_now_pts, 8);
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
		cur_packet++;
		usleep(1);
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
	m_IsPause = !m_IsPause;
	//m_IsPause = !m_IsPause;

	cout << "[SENDER.ch" << m_nChannel << "] Set Pause : " << m_IsPause << endl;
}

void CSender::SetPause(bool state)
{
	m_IsPause = state;
	//m_IsPause = !m_IsPause;

	cout << "[SENDER.ch" << m_nChannel << "] Set Pause : " << m_IsPause << endl;
}

#if 0
void CSender::SetSyncPTS(uint64_t pts)
{
	m_sync_pts = pts;
}
#endif

#if 0
void CDemuxer::SyncCheck()
{
	if (m_sync_pts != 0)
	{
		if (m_sync_pts > m_current_pts)
		{
			m_Wait = false;
		}
		else
		{
			m_Wait = true;
		}

		cout << "[DEMUXER.ch" << m_nChannel << "] sync_pts (" << m_sync_pts << ") current_pts (" << m_current_pts << ")" << endl;

		if (m_IsForcePause == false)
		{
			m_sync_cnt++;
			if (m_sync_cnt > m_wait_frame)
			{
#if 0
				if (m_sync_pts <= m_current_pts)
				{
					m_Wait = false;
					m_sync_pts = 0;
					m_sync_cnt = 0;
				}
#endif
				m_Wait = false;
				m_sync_pts = 0;
				m_sync_cnt = 0;
			}
		}
	}
}
#endif

bool CSender::SetReverse()
{
	m_bIsRerverse = !m_bIsRerverse;
	cout << "[SENDER.ch" << m_nChannel << "] Set Reverse : " << m_bIsRerverse << endl;
	return m_bIsRerverse;
}

bool CSender::SetReverse(bool state)
{
	m_bIsRerverse = state;
	cout << "[SENDER.ch" << m_nChannel << "] Set Reverse : " << m_bIsRerverse << endl;
	return m_bIsRerverse;
}

int CSender::open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
	int ret, stream_index;
	AVStream *st;
	AVCodec *dec = NULL;
	AVDictionary *opts = NULL;

	ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0)
	{
		fprintf(stderr, "Could not find %s stream in input file");
		return ret;
	}
	else
	{
		stream_index = ret;
		st = fmt_ctx->streams[stream_index];

		/* find decoder for the stream */
		dec = avcodec_find_decoder(st->codecpar->codec_id);
		if (!dec)
		{
			fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}

		/* Allocate a codec context for the decoder */
		*dec_ctx = avcodec_alloc_context3(dec);
		if (!*dec_ctx)
		{
			fprintf(stderr, "Failed to allocate the %s codec context\n", av_get_media_type_string(type));
			return AVERROR(ENOMEM);
		}

		/* Copy codec parameters from input stream to output codec context */
		if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0)
		{
			fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(type));
			return ret;
		}

		/* Init the decoders, with or without reference counting */
		av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
		if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0)
		{
			fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
			return ret;
		}
		*stream_idx = stream_index;
	}

	return 0;
}

#if 0
int CDemuxer::get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt)
{
	int i;
	struct sample_fmt_entry
	{
		enum AVSampleFormat sample_fmt;
		const char *fmt_be, *fmt_le;
	} sample_fmt_entries[] = {
		{AV_SAMPLE_FMT_U8, "u8", "u8"},
		{AV_SAMPLE_FMT_S16, "s16be", "s16le"},
		{AV_SAMPLE_FMT_S32, "s32be", "s32le"},
		{AV_SAMPLE_FMT_FLT, "f32be", "f32le"},
		{AV_SAMPLE_FMT_DBL, "f64be", "f64le"},
	};
	*fmt = NULL;

	for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++)
	{
		struct sample_fmt_entry *entry = &sample_fmt_entries[i];
		if (sample_fmt == entry->sample_fmt)
		{
			*fmt = AV_NE(entry->fmt_be, entry->fmt_le);
			return 0;
		}
	}

	fprintf(stderr,
			"sample format %s is not supported as output format\n",
			av_get_sample_fmt_name(sample_fmt));
	return -1;
}
#endif

void CSender::Delete()
{
	if (m_sock > 0)
	{
		cout << "[SENDER.ch" << m_nChannel << "] sock " << m_sock << " closed" << endl;
		close(m_sock);
	}
	SAFE_DELETE(m_queue);
}