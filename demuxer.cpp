#include "main.h"
#include "demuxer.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

#define MAX_NUM_bitstream 4
#define MAX_frame_size 1024 * 1024 // 1MB

CDemuxer::CDemuxer(void)
{
	m_bExit = false;
	//pthread_mutex_init(&m_mutex_sender, 0);
}

CDemuxer::~CDemuxer(void)
{
	m_bExit = true;

	Delete();
	_d("[SENDER.ch%d] Trying to exit thread\n", m_nChannel);
	Terminate();
	_d("[SENDER.ch%d] exited...\n", m_nChannel);
	//pthread_mutex_destroy(&m_mutex_sender);
}

void CDemuxer::log(int type, int state)
{
}

bool CDemuxer::Create(Json::Value info, Json::Value attr, int nChannel)
{
	m_info = info;
	m_nChannel = nChannel;
	m_attr = attr;
	m_file_idx = 0;
	m_nSpeed = 1;
	m_pause = false;
	m_IsRerverse = false;
	m_CQueue = new CQueue();

	// sender thread
	m_CSender = new CSender();
	m_CSender->SetQueue(&m_CQueue, nChannel);
	m_CSender->Create(info, nChannel);

	Start();
#if 0
	if (SetSocket())
	{
		cout << "[DEMUXER.ch" << m_nChannel << "] SetSocket config is completed" << endl;
		Start();
	}
#endif

	return true;
}

bool CDemuxer::CreateReverse(Json::Value info, Json::Value attr, int nChannel)
{
	m_info = info;
	m_nChannel = nChannel;
	m_attr = attr;
	m_file_idx = 0;
	m_nSpeed = 1;
	m_pause = false;
	m_IsRerverse = true;
	m_CQueue = new CQueue();

	// sender thread
	m_CSender = new CSender();
	m_CSender->SetQueue(&m_CQueue, nChannel);
	m_CSender->Create(info, nChannel);

	Start();
#if 0
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
#if 0
bool CDemuxer::SetSocket()
{
	cout << "[SENDER.ch" << m_nChannel << "] : " << m_attr["file_dst"].asString() << endl;

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

	uint ttl = 16;
	state = setsockopt(m_sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
	if (state < 0)
	{
		cout << "[SENDER.ch" << m_nChannel << "] Setting IP_MULTICAST_TTL error" << endl;
		;
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
#endif

void CDemuxer::Run()
{
	while (!m_bExit)
	{
		if (!Play())
		{
			//error send() return false
		}
		else
		{
			// cout << "[SENDER.ch" << m_nChannel << "] sender loop completed" << endl;
			// send() eturn true
		}
		//usleep(100);
	}
}

bool CDemuxer::Play()
{
	//cout << "[SENDER.ch" << m_nChannel << "] Send() Alive" << endl;
	//GetOutputs(m_attr["file_dst"].asString());
	stringstream sstm;
	sstm.str("");
	sstm << m_attr["file_dst"].asString() << "/" << m_attr["target"].asString() << "/" << m_nChannel;
	//cout << "[DEMUXER.ch" << m_nChannel << "] target : " << sstm.str() << endl;
	if (!GetChannelFiles(sstm.str()))
	{
		m_bExit = true;
		// 자원회수는 나중에
	}

	//cout << "[SENDER.ch" << m_nChannel << "] one loop completed" << endl;
	//usleep(1000);

	return true;
}

int CDemuxer::Demux(string src_filename)
{
	m_index++;
	ifstream ifs;

	if (m_attr["type"].asString() == "video")
	{
		if (avformat_open_input(&fmt_ctx, src_filename.c_str(), NULL, NULL) < 0)
		{
			cout << "[DEMUXER.ch" << m_nChannel << "] Could not open source file : " << src_filename << endl;
			return false;
		}
		else
		{
			cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << " file is opened" << endl;
		}
		m_bsf = av_bsf_get_by_name("hevc_mp4toannexb");

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
	else if (m_attr["type"].asString() == "audio")
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
			//
		}
	}
#if 0
	av_init_packet(&m_pkt);
	m_pkt.data = NULL;
	m_pkt.size = 0;
#endif
	high_resolution_clock::time_point begin;
	high_resolution_clock::time_point end;

	begin = high_resolution_clock::now();
	while (!m_bExit)
	{
		if (m_attr["type"].asString() == "video")
		{
			AVPacket pkt;
			av_init_packet(&pkt);
			if (av_read_frame(fmt_ctx, &pkt) < 0)
			{
				cout << "[DEMUXER.ch" << m_nChannel << "] meet EOF" << endl;
				avformat_close_input(&fmt_ctx);
				// 메모리닉 나면 아래 free_context 추가할 것
				//avformat_free_context(fmt_ctx);
				break;
			}
			while (!m_bExit)
			{
				if (!m_CQueue->Put(&pkt))
				{
					//cout << "[DEMUXER.ch" << m_nChannel << "] Put Video failed(" << m_CQueue->GetVideoPacketSize() << ")" << endl;
					this_thread::sleep_for(microseconds(10000));
					// and retry
				}
				else
				{
					break;
				}
			}
		}
		else if (m_attr["type"].asString() == "audio")
		{
			char audio_buf[AUDIO_BUFF_SIZE];
			ifs.read(audio_buf, AUDIO_BUFF_SIZE);
			//cout << hex << audio_buf << endl;
			if (!m_pause)
			{
				while (!m_bExit)
				{
					if (!m_CQueue->PutAudio(audio_buf, AUDIO_BUFF_SIZE))
					{
						//cout << "[DEMUXER.ch" << m_nChannel << "] Put Audio failed(" << m_CQueue->GetAudioPacketSize() << ")" << endl;
						this_thread::sleep_for(microseconds(10000));
						// and retry
					}
					else
					{
						break;
					}
				}
			}

			if (ifs.eof())
			{
				ifs.close();
				cout << "[DEMUXER.ch" << m_nChannel << "] audio meet eof" << endl;
				break;
			}
			//if meet eof then break
		}

#if 0
		if (m_attr["type"].asString() == "video")
		{
			target_time = num / den * 1000000 / m_nSpeed;
		}
		while (tick_diff < target_time)
		{
			//usleep(1); // 1000000 us = 1 sec
			end = high_resolution_clock::now();
			tick_diff = duration_cast<microseconds>(end - begin).count();
			this_thread::sleep_for(microseconds(1));
		}
		begin = end;
#endif
	}
	cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << " file is closed" << endl;
	return true;
}

int CDemuxer::DemuxRerverse(string src_filename)
{
	m_index++;
	ifstream ifs;

	if (m_attr["type"].asString() == "video")
	{
		if (avformat_open_input(&fmt_ctx, src_filename.c_str(), NULL, NULL) < 0)
		{
			cout << "[DEMUXER.ch" << m_nChannel << "] Could not open source file : " << src_filename << endl;
			return false;
		}
		else
		{
			cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << " file is opened" << endl;
		}
		m_bsf = av_bsf_get_by_name("hevc_mp4toannexb");

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

#if 0
	av_init_packet(&m_pkt);
	m_pkt.data = NULL;
	m_pkt.size = 0;
#endif
	high_resolution_clock::time_point begin;
	high_resolution_clock::time_point end;

	begin = high_resolution_clock::now();
	while (!m_bExit)
	{
		if (m_attr["type"].asString() == "video")
		{
			AVPacket pkt;
			av_init_packet(&pkt);
			// 타임스탬프로 변환해서 감소
			//int ret = avformat_seek_file(fmt_ctx, m_nVideoStream, 0, tm, tm, 0);
			int64_t ts;
			//av_seek_frame (fmt_ctx), 0, ts, int flags) // 이걸로 할 수 있을듯
			if (av_read_frame(fmt_ctx, &pkt) < 0)
			{
				cout << "[DEMUXER.ch" << m_nChannel << "] meet EOF" << endl;
				avformat_close_input(&fmt_ctx);
				// 메모리릭 나면 아래 free_context 추가할 것
				//avformat_free_context(fmt_ctx);
				break;
			}
			while (!m_bExit)
			{
				if (pkt.flags == AV_PKT_FLAG_KEY)
				{
					// keyframe
					if (!m_CQueue->Put(&pkt))
					{
						//cout << "[DEMUXER.ch" << m_nChannel << "] Put Video failed(" << m_CQueue->GetVideoPacketSize() << ")" << endl;
						this_thread::sleep_for(microseconds(10000));
						// and retry
					}
					else
					{
						break;
					}
				}
			}
		}
	}
	cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << " file is closed" << endl;
	return true;
}

bool CDemuxer::MoveFileTime(int nSec)
{
	double fTime = m_info["fps"].asDouble();
	double num = m_info["num"].asDouble();
	double den = m_info["den"].asDouble();
	bool bMoved = false;
	cout << "[DEMUXER.ch" << m_nChannel << "] 여기까지 왔군 .... " << nSec << " 초" << endl;
	return false;
	ReadStop();

	int m_nVideoStream = 0; // 스트림 비디오 (0이 아닐 수도 있음 INDEX로 알고 있음)

	AVStream *pStream = fmt_ctx->streams[m_nVideoStream];
	if (!pStream)
	{
		cout << "[DEMUXER.ch" << m_nChannel << "] pStream error " << endl;
		return false;
	}

	//> 이동하려 하는 시간정보 구하고
	//int nFrame = nSec * m_fFPS;
	int nFrame = nSec * (den / num);
	fTime = (((double)nFrame * den) / num) - 0.5;
//	double fTime = (((double)nFrame*pStream->avg_frame_rate.den)/pStream->avg_frame_rate.num);
#if 0
	if (m_fFirstVideoTime >= 0.)
	{
		fTime += m_fFirstVideoTime;
	}
#endif

	//	fTime = max(fTime, 0.1);
	fTime = max(fTime, 0.);

	AVRational timeBaseQ;
	AVRational timeBase = pStream->time_base;

	timeBaseQ.num = 1;
	timeBaseQ.den = AV_TIME_BASE;

	int64_t tm = (int64_t)(fTime * AV_TIME_BASE);
	tm = av_rescale_q(tm, timeBaseQ, timeBase);

	//> 파일 이동
	avcodec_flush_buffers(fmt_ctx->streams[m_nVideoStream]->codec);
	int ret = avformat_seek_file(fmt_ctx, m_nVideoStream, 0, tm, tm, 0);
	if (ret < 0)
	{
		_d("[DMUXER.%d] Failed to seek frame (%.3f)\n", m_nChannel, tm);
		return false;
	}

	bMoved = true;
	if (bMoved)
	{
		//	InitLKFSData();
		ReadStart();
	}
}

void CDemuxer::ReadStart()
{
	Start();
}
void CDemuxer::ReadStop()
{
	Terminate();

	//m_bFrameInfoCopy = false;

	//이게 뭔지??
	//SAFE_DELETE(m_pSharFrame);

	//m_videoPktQue.Clear();
	m_CQueue->Clear();
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

bool CDemuxer::GetChannelFiles(string path)
{
	if (!IsDirExist(path))
	{
		cout << "[DEMUXER.ch" << m_nChannel << "] " << path << " is not exist" << endl;
		return false;
	}

	ifstream ifs(path + "/" + "meta.json");
	if (!ifs.is_open())
	{
		//is not open or not exist
		cout << "[DEMUXER.ch" << m_nChannel << "] " << path + "/" + "meta.json"
			 << " is not exist" << endl;
		return false;
	}
	else
	{
		Json::Reader reader;
		Json::Value meta;
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
			cout << "[DEMUXER.ch" << m_nChannel << "] " << path + "/" + "meta.json"
				 << " parse failed" << endl;
			ifs.close();
			return false;
		}
	}

	DIR *dir = opendir(path.c_str());
	struct dirent *ent;
	stringstream ss;

	while ((ent = readdir(dir)) != NULL && !m_bExit)
	{
		if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
		{
			if (ent->d_type != DT_DIR)
			{
				string fn = ent->d_name;
				if (fn.substr(fn.find_last_of(".") + 1) == "es" || fn.substr(fn.find_last_of(".") + 1) == "audio")
				{
					ss.str("");
					//cout << "[DEMUXER.ch" << m_nChannel << "] path : " << path << "/" << ent->d_name << endl;
					ss << path << "/" << ent->d_name;
					if (getFilesize(ss.str().c_str()) == 0)
					{
						//cout << "[DEMUXER.ch" << m_nChannel << "] " << ss.str() << " size is 0" << endl;
						continue;
					}
					if (!m_IsRerverse)
					{
						if (Demux(ss.str()))
						{
							cout << "[DEMUXER.ch" << m_nChannel << "] " << ss.str() << " demux success" << endl;
						}
					}
					else
					{
						if (Demux(ss.str()))
						{
							cout << "[DEMUXER.ch" << m_nChannel << "] " << ss.str() << " demux success" << endl;
						}
					}
				}
			}
		}
	}
	closedir(dir);
	return true;
}

#if 0
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
#endif

void CDemuxer::SetSpeed(int speed)
{
	//m_nSpeed = speed;
	cout << "[DEMUXER.ch" << m_nChannel << "] Set speed : " << m_nSpeed << endl;
	m_CSender->SetSpeed(speed);
}

void CDemuxer::SetPause()
{
	m_CSender->SetPause();
}

void CDemuxer::SetReverse()
{
	m_CQueue->Clear();
	m_CSender->SetReverse();
}
#if 0
int CDemuxer::open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
	int ret, stream_index;
	AVStream *st;
	AVCodec *dec = NULL;
	AVDictionary *opts = NULL;

	ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0)
	{
		fprintf(stderr, "Could not find %s stream in input file '%s'\n",
				av_get_media_type_string(type), src_filename);
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
			fprintf(stderr, "Failed to find %s codec\n",
					av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}

		/* Allocate a codec context for the decoder */
		*dec_ctx = avcodec_alloc_context3(dec);
		if (!*dec_ctx)
		{
			fprintf(stderr, "Failed to allocate the %s codec context\n",
					av_get_media_type_string(type));
			return AVERROR(ENOMEM);
		}

		/* Copy codec parameters from input stream to output codec context */
		if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0)
		{
			fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
					av_get_media_type_string(type));
			return ret;
		}

		/* Init the decoders, with or without reference counting */
		av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
		if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0)
		{
			fprintf(stderr, "Failed to open %s codec\n",
					av_get_media_type_string(type));
			return ret;
		}
		*stream_idx = stream_index;
	}

	return 0;
}
#endif

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
	//close(m_sock);
	SAFE_DELETE(m_CSender);
}