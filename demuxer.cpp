#include "main.h"
#include "demuxer.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

#define MAX_NUM_bitstream 4
#define MAX_frame_size 1024 * 1024 // 1MB

CDemuxer::CDemuxer(void)
{
	m_bExit = false;
	m_CThumbnail = NULL;
	m_sock = 0;
	//pthread_mutex_init(m_mutex_demuxer, 0);
}

CDemuxer::~CDemuxer(void)
{
	m_bExit = true;

	Delete();
	_d("[DEMUXER.ch%d] Trying to exit thread\n", m_nChannel);
	Terminate();
	_d("[DEMUXER.ch%d] exited...\n", m_nChannel);
	//pthread_mutex_destroy(m_mutex_demuxer);
}

void CDemuxer::log(int type, int state)
{
}

bool CDemuxer::Create(Json::Value info, Json::Value attr, int nChannel)
{
	fmt_ctx = NULL;
	m_info = info;
	m_nChannel = nChannel;
	m_attr = attr;
	m_file_idx = 0;
	m_nSpeed = 1;
	m_nSeekFrame = 0;
	m_is_skip = false;
	m_current_pts = 0;
	m_sync_pts = 0;
	//m_compare_old_pts = 0;
	m_sync_cnt = 0;
	m_Wait = false;
	m_IsForcePause = false;
	m_bIsRerverse = false;
	m_IsMove = false;
	m_nTotalFrame = 0;
	m_nTotalSec = 0;
	m_file_cnt = 0;
	m_files = NULL;
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();

	m_nAudioCount = 0;
	refcount = 0; // 1과 0의 차이를 알아보자

	int bit_state = m_attr["bit_state"].asInt();
	int result = bit_state & (1 << m_nChannel);

	if (bit_state > 0)
	{
		cout << "[DEMUXER.ch" << m_nChannel << "] bit state is " << bit_state << ", result : " << result << endl;
		if (result < 1)
		{
			cout << "[DEMUXER.ch" << m_nChannel << "] is not use anymore" << endl;
			return false;
		}
	}

	m_CThumbnail = new CThumbnail();

	cout << "[DEMUXER.ch" << m_nChannel << "] type : " << m_info["type"].asString() << endl;

#if 0
	// sender thread
	m_CSender = new CSender();
	m_CSender->SetQueue(&m_CQueue, nChannel);
	m_CSender->Create(info, nChannel);
	//m_CSender->SetAttribute(m_info);
#endif

#if 1
	if (SetSocket())
	{
		cout << "[DEMUXER.ch" << m_nChannel << "] SetSocket config is completed" << endl;
		Start();
	}
#endif

	return true;
}

bool CDemuxer::SetMutex(pthread_mutex_t *mutex)
{
	m_mutex_demuxer = mutex;
	cout << "[DEMUXER.ch" << m_nChannel << "] m_mutex_sender address : " << m_mutex_demuxer << endl;
}
#if 1
bool CDemuxer::SetSocket()
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
#endif

void CDemuxer::Run()
{
	Play();
}

bool CDemuxer::Play()
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

bool CDemuxer::GetChannelFiles(string path)
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
		cout << "[DEMUXER.ch" << m_nChannel << "] " << path + "/" + "meta.json"
			 << " is not exist" << endl;
		return false;
	}
	else
	{
		if (reader.parse(ifs, meta, true))
		{
			// parse success
			ifs.close();
#if 0
			m_attr["type"] = meta["type"]; // video or audio
			m_CSender->SetAttribute(m_attr);
#endif
		}
		else
		{
			// parse failed
			cout << "[DEMUXER.ch" << m_nChannel << "] " << path + "/" + "meta.json"
				 << " parse failed" << endl;
			ifs.close();
			return false;
		}
	}

#if 0
	for (auto &value : meta["files"])
	{
		if (m_bExit == true)
		{
			break;
		}
		if (Demux(value["name"].asString()))
		{
			cout << "[DEMUXER.ch" << m_nChannel << "] " << value["name"].asString() << " demux success" << endl;
		}
	}
#else
	if (Demux(meta["files"]))
	{
		cout << "[DEMUXER.ch" << m_nChannel << "] success" << endl;
	}
#endif

	return true;
}

int CDemuxer::Demux(Json::Value files)
{
	string src_filename = "";
	Json::Value value;
	m_nFrameCount = 0;
	m_seek_pts = 0;
	// file 데이터 전역변수로 복사
	m_files = files;

	for (int i = 0; i < files.size(); i++)
	{
		m_nTotalFrame += files[i]["frame"].asInt();
		m_file_cnt++;
	}

	int codec = m_attr["codec"].asInt(); // 0 : H264, 1 : HEVC
	double num = m_info["num"].asDouble();
	double den = m_info["den"].asDouble();

	m_nTotalSec = m_nTotalFrame * num / den;

	cout << "[DEMUXER.ch" << m_nChannel << "] totalFrame : " << m_nTotalFrame << ", totalSec : " << m_nTotalSec << ", filecnt : " << m_file_cnt << endl;

	high_resolution_clock::time_point start_pts;
	high_resolution_clock::time_point current_pts;
	start_pts = high_resolution_clock::now();
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

		//m_start_pts = 0;

		src_filename = value["name"].asString();
		ifstream ifs;
		//double fTime = m_info["fps"].asDouble();

		string type = m_info["type"].asString();

		double target_time = num / den * AV_TIME_BASE;
		double fps = den / num;
		double fTime;
		int64_t tick_diff = 0;
		int64_t now_pts = 0;
		int64_t out_speed_pts = 0;
		int64_t out_pts = 0;
		int ret = 0;
		//m_fps = fps;
		double fFPS = 0.;
		uint64_t old_pts = 0;
		uint16_t skip_cnt = 0;

		//cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << ", " << num << ", " << den << ", type : " << type << endl;

		if (type == "video")
		{
			if (avformat_open_input(&fmt_ctx, src_filename.c_str(), NULL, NULL) < 0)
			{
				cout << "[DEMUXER.ch" << m_nChannel << "] Could not open source file : " << src_filename << endl;
				return false;
			}
			else
			{
				m_currentDuration = fmt_ctx->duration;
				cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << " file is opened (" << m_currentDuration << ")" << endl;
			}

			ret = avformat_find_stream_info(fmt_ctx, NULL);
			if (ret < 0)
			{
				cout << "[DEMUXER.ch" << m_nChannel << "] failed to find proper stream in FILE : " << src_filename << endl;
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
					_d("[DEMUXER] Failed to alloc bsfc\n");
					return false;
				}
				if (avcodec_parameters_copy(m_bsfc->par_in, fmt_ctx->streams[0]->codecpar) < 0)
				{
					_d("[DEMUXER] Failed to copy codec param.\n");
					return false;
				}
				m_bsfc->time_base_in = fmt_ctx->streams[0]->time_base;

				if (av_bsf_init(m_bsfc) < 0)
				{
					_d("[DEMUXER] Failed to init bsfc\n");
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
				cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << " file open failed" << endl;
				return false;
			}
			else
			{
				cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << " file open success" << endl;
			}
			//_d("[DEMUXER.ch%d] fduration : %3f\n", m_nChannel, m_fDuration);
		}
#if 0
		high_resolution_clock::time_point begin;
		high_resolution_clock::time_point end;
		begin = high_resolution_clock::now();
#endif
		int64_t ts = 0;
		int readcnt = 0;

		m_CThumbnail->Create(m_info, m_attr, m_nChannel);

		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		pkt.pts = 0;

		while (!m_bExit)
		{
			current_pts = high_resolution_clock::now();
			now_pts = duration_cast<microseconds>(current_pts - start_pts).count();
			now_pts = m_seek_pts + (now_pts * m_nSpeed);
			out_speed_pts = out_pts * m_nSpeed;
			this_thread::sleep_for(microseconds(1));
			//cout << "[DEMUXER.ch" << m_nChannel << "] frame : " << m_nFrameCount << ", pts : " << pkt.pts << ", now_pts : " << now_pts << ", out_speed_pts : " << out_speed_pts << endl;

			if (out_speed_pts <= now_pts)
			{
				if (type == "video")
				{
					if (m_IsMove == true)
					{
						m_IsMove = false;
						i = m_nMoveIdx - 1;
						break;
					}

					if (m_nSeekFrame > 0)
					{
						SeekFrame(m_nSeekFrame);
						m_nSeekFrame = 0;
					}
#if __OLD_SENDER

					if (m_bIsRerverse)
					{
						m_sync_pts = 0;
						if (Reverse())
						{
							//true
						}
						else
						{
							i = i - 2; // 직전 인덱스로 이동
							//m_nFrameCount = last_frame;
							cout << "[DEMUXER.ch" << m_nChannel << "set index : " << i << ", m_nFrameCount : " << m_nFrameCount << endl;
							break;
						}
					}
#endif
					av_packet_unref(&pkt);
					av_init_packet(&pkt);

					pkt.data = NULL;
					pkt.size = 0;
					pkt.pts = 0;
					if (av_read_frame(fmt_ctx, &pkt) < 0)
					{
						cout << "[DEMUXER.ch" << m_nChannel << "] meet EOF(" << fmt_ctx->filename << endl;
						avformat_close_input(&fmt_ctx);
						fmt_ctx = NULL;
						break;
					}
					else
					{
						m_nFrameCount++;

						AVStream *pStream = fmt_ctx->streams[0];
						AVRational timeBase = pStream->time_base;
						out_pts = pkt.pts * AV_TIME_BASE / timeBase.den;

						cout << "[DEMUXER.ch" << m_nChannel << "] frame : " << m_nFrameCount << ", pts : " << pkt.pts << ", now_tick : " << now_pts << ", out_pts : " << out_pts << " (" << timeBase.num << "/" << timeBase.den << ")" << endl;
					}

					if ((ret = av_bsf_send_packet(m_bsfc, &pkt)) < 0)
					{
						av_log(fmt_ctx, AV_LOG_ERROR, "Failed to send packet to filter %s for stream %d\n", m_bsfc->filter->name, pkt.stream_index);
						cout << "[DEMUXER.ch" << m_nChannel << "] current pts : " << pkt.pts << endl;
					}
					// TODO: when any automatically-added bitstream filter is generating multiple
					// output packets for a single input one, we'll need to call this in a loop
					// and write each output packet.
					if ((ret = av_bsf_receive_packet(m_bsfc, &pkt)) < 0)
					{
						if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
						{
							av_log(fmt_ctx, AV_LOG_ERROR, "Failed to receive packet from filter %s for stream %d\n", m_bsfc->filter->name, pkt.stream_index);
							cout << "[DEMUXER.ch" << m_nChannel << "] current pts : " << pkt.pts << endl;
						}
						if (fmt_ctx->error_recognition & AV_EF_EXPLODE)
						{
							//
						}
					}

#if 0
				cout << "[DEMUXER.ch" << m_nChannel << "] (" << nFrame << ")pkt.flag : " << pkt.flags << ", size : " << pkt.size << " ";
				_d("%02x %02x %02x %02x\n", pkt.data[0], pkt.data[1], pkt.data[2], pkt.data[3]);
#endif

#if __OLD_SENDER
					if (m_nSpeed > keyframe_speed && pkt.flags != AV_PKT_FLAG_KEY)
					{
						m_is_skip = true;
					}

					if (pkt.flags == AV_PKT_FLAG_KEY)
					{
						m_is_skip = false;
					}

					if (old_pts == pkt.pts && m_IsForcePause == false)
					{
						//같은 PTS가 연속해서 나갈 때
						m_is_skip = true;
					}

					if (m_is_skip == false)
					{
						if (!send_bitstream(pkt.data, pkt.size))
						{
							cout << "[DEMUXER.ch" << m_nChannel << "] send_bitstream failed" << endl;
						}
						else
						{
							cout << "[DEMUXER.ch" << m_nChannel << "] sync_pts (" << m_sync_pts << ") send_bitstream (" << m_current_pts << ") sended, ForcePause(" << m_IsForcePause << "), nFrame : " << m_nFrameCount << endl;
						}
					}
					//cout << "[DEMUXER.ch" << m_nChannel << "] m_is_skip (" << m_is_skip << "), m_sync_pts : " << m_sync_pts << ", m_current_pts : " << m_current_pts << endl;
					old_pts = pkt.pts;
					//cout << "[DEMUXER.ch" << m_nChannel << "] pkt unref" << endl;
#endif
					if (!send_bitstream(pkt.data, pkt.size))
					{
						cout << "[DEMUXER.ch" << m_nChannel << "] send_bitstream failed" << endl;
					}
					else
					{
						cout << "[DEMUXER.ch" << m_nChannel << "] send_bitstream (" << pkt.pts << ") sended, nFrame : " << m_nFrameCount << endl;
					}
				}
				else if (type == "audio")
				{
					if (m_IsForcePause == false && m_bIsRerverse == false && m_nSpeed == 1)
					{
						char audio_buf[AUDIO_BUFF_SIZE];
#if 0
						if (m_IsMove == true)
						{
							m_IsMove = false;
							i = m_nMoveIdx - 1;
							break;
						}

						if (m_nMoveFrame > 0)
						{
							m_nAudioCount = m_nMoveFrame;
							m_nMoveFrame = m_nMoveFrame - (last_frame * m_nMoveIdx);

							cout << "[DEMUXER.ch" << m_nChannel << "] move audio frame (" << m_nMoveFrame << ")" << endl;
							//MoveFrame(m_nMoveFrame);
							ifs.seekg(m_nMoveFrame * AUDIO_BUFF_SIZE);
							m_nMoveFrame = 0;
						}
#endif

						ifs.read(audio_buf, AUDIO_BUFF_SIZE);

						out_pts = m_nAudioCount * AV_TIME_BASE / den;
						m_nAudioCount++;

						cout << "[DEMUXER.ch" << m_nChannel << "] audio : " << m_nAudioCount << ", now_tick : " << now_pts << ", out_pts : " << out_pts << " (" << num << "/" << den << ")" << endl;

						//legacy code
						//m_CQueue->PutAudio(audio_buf, AUDIO_BUFF_SIZE);
						if (!send_audiostream(audio_buf, AUDIO_BUFF_SIZE))
						{
							cout << "[DEMUXER.ch" << m_nChannel << "] send_audiotream failed" << endl;
						}
						else
						{
#if 1
							cout << "[DEMUXER.ch" << m_nChannel << "] send_audiotream sended(" << AUDIO_BUFF_SIZE << "), AudioCount : " << m_nAudioCount << endl;
#endif
						}
						if (ifs.eof())
						{
							ifs.close();
							cout << "[DEMUXER.ch" << m_nChannel << "] audio meet eof" << endl;
							break;
						}
						//if meet eof then break
					}
					else
					{
						//cout << "[DEMUXER.ch" << m_nChannel << "] audio sync required" << endl;
					}
				}
#if 0
				if (type == "video")
				{
					target_time = num / den * AV_TIME_BASE / m_nSpeed;
				}
#endif
#if 0
			while (tick_diff < target_time)
			{
				//usleep(1); // 1000000 us = 1 sec
				end = high_resolution_clock::now();
				tick_diff = duration_cast<microseconds>(end - begin).count();
				this_thread::sleep_for(microseconds(1));
			}			
			begin = end;
			tick_diff = 0;
#endif
			}
		}

		//전체 타이머 하나 돌려야함

		//this_thread::sleep_for(microseconds(100000));
		//_d("[%d] fmt_ctx : %x\n", m_nChannel, fmt_ctx);
		if (type == "video" && fmt_ctx)
		{
			cout << "[DEMUXER.ch" << m_nChannel << "] avformat_close_input (" << fmt_ctx->filename << endl;
			avformat_close_input(&fmt_ctx);
			fmt_ctx = NULL;
		}
		if (type == "audio" && ifs.is_open())
		{
			ifs.close();
		}
		cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << " file is closed" << endl;
		m_index++;
	}

	av_bsf_free(&m_bsfc);
	return true;
}

bool CDemuxer::SetMoveSec(int nSec)
{
	double ret;
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();
	uint64_t nFrame = (nSec * den) / num;
	m_nMoveIdx = FindFileIndexFromFrame(nFrame);
	cout << "[%DEMUXER.ch" << m_nChannel << "] input frame : " << nFrame << ", move index : " << m_nMoveIdx << endl;

	m_nSeekFrame = nFrame;
	m_IsMove = true;
}

bool CDemuxer::SeekFrame(int nFrame)
{
	int num = m_info["num"].asInt();
	int den = m_info["den"].asInt();

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
	}
	m_nFrameCount = nFrame;
	m_seek_pts = tm * AV_TIME_BASE / timeBase.den;
	cout << "[DEMUXER.ch" << m_nChannel << "] Set m_nFrameCount : " << m_nFrameCount << endl;
	//m_nMoveSec = 0;
}

bool CDemuxer::Reverse()
{
	int num = m_info["num"].asInt();
	int den = m_info["den"].asInt();

	int nFrame = m_nFrameCount;

	int ret = 0;
	double fTime = 0;
	AVStream *pStream = fmt_ctx->streams[0];
	fTime = (((double)(nFrame)*pStream->avg_frame_rate.den) / pStream->avg_frame_rate.num) - 0.5;

	fTime = max(fTime, 0.);

	AVRational timeBaseQ;
	AVRational timeBase = pStream->time_base;

	timeBaseQ.num = 1;
	timeBaseQ.den = AV_TIME_BASE;

	int64_t tm = (int64_t)(fTime * AV_TIME_BASE);
	tm = av_rescale_q(tm, timeBaseQ, timeBase);

	//_d("[REVERSE.ch%d] (tm : %lld)\n", m_nChannel, tm);

	//_d("[channel.%d] reverse frame count : %d, ftime : %.3f\n", m_nChannel, m_nFrameCount, fTime);

	if (m_nChannel < 6)
	{
		//avcodec_flush_buffers(fmt_ctx->streams[0]->codec);
		_d("[DEMUXER.ch%d] m_nFrameCount : %d, tm : %lld, fTime : %.3f\n", m_nChannel, m_nFrameCount, tm, fTime);
		if (m_nChannel < 4)
		{
			ret = avformat_seek_file(fmt_ctx, 0, 0, tm, tm, AVSEEK_FLAG_FRAME);
		}
		else
		{
			ret = avformat_seek_file(fmt_ctx, 0, 0, tm, tm, AVSEEK_FLAG_BYTE);
		}
		if (ret < 0)
		{
			_d("[REVERSE.ch%d] ret : %d, not work(tm : %lld)\n", m_nChannel, ret, tm);
			return false;
		}
	}
	m_nFrameCount = m_nFrameCount - 2;

	if (m_nFrameCount < 1)
	{
		return false;
	}
#if 0
	if (m_start_pts < tm && m_start_pts != 0)
	{
		_d("[REVERSE.ch%d] start_pts : %lld, not work(tm : %lld)\n", m_nChannel, m_start_pts, tm);
		return false;
	}
#endif
	return true;
}

int CDemuxer::FindFileIndexFromFrame(uint64_t nFrame)
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

Json::Value CDemuxer::GetThumbnail(int nSec)
{
	Json::Value ret_json;
	ret_json = m_CThumbnail->GetThumbnail(nSec);
	return json;
}

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

bool CDemuxer::send_audiostream(char *buff, int size)
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

bool CDemuxer::send_bitstream(uint8_t *stream, int size)
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
			cout << "[DEMUXER.ch" << m_nChannel << "] "
				 << "tot_size : " << tot_size << ", cur_size : " << cur_size << ", tot_packet : " << tot_packet << ", cur_packet : " << cur_packet << ", nSendto : " << nSendto << endl;
#endif
		}

		remain -= cur_size;
		p += cur_size;
		cur_packet++;
	}

	return true;
}

void CDemuxer::SetSpeed(int speed)
{
	m_nSpeed = speed;
	cout << "[DEMUXER.ch" << m_nChannel << "] Set speed : " << m_nSpeed << endl;
}

void CDemuxer::SetPause()
{
	m_IsForcePause = !m_IsForcePause;
	//m_IsPause = !m_IsPause;

	cout << "[DEMUXER.ch" << m_nChannel << "] Set Pause : " << m_IsForcePause << endl;
}

void CDemuxer::SetPause(bool state)
{
	m_IsForcePause = state;
	//m_IsPause = !m_IsPause;

	cout << "[DEMUXER.ch" << m_nChannel << "] Set Pause : " << m_IsForcePause << endl;
}

void CDemuxer::SetSyncPTS(uint64_t pts)
{
	m_sync_pts = pts;
}

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

bool CDemuxer::SetReverse()
{
	m_bIsRerverse = !m_bIsRerverse;
	cout << "[DEMUXER.ch" << m_nChannel << "] Set Reverse : " << m_bIsRerverse << endl;
	return m_bIsRerverse;
}

bool CDemuxer::SetReverse(bool state)
{
	m_bIsRerverse = state;
	cout << "[DEMUXER.ch" << m_nChannel << "] Set Reverse : " << m_bIsRerverse << endl;
	return m_bIsRerverse;
}

int CDemuxer::open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
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

void CDemuxer::Delete()
{
	//cout << "[DEMUXER.ch" << m_nChannel << "] sock " << m_sock << " closed" << endl;
	if (m_sock > 0)
	{
		close(m_sock);
	}
	//SAFE_DELETE(m_CSender);
	//SAFE_DELETE(m_CSwitch);
}