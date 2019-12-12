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

bool CSender::Create(Json::Value info, Json::Value attr, int nChannel)
{
	m_info = info;
	m_nChannel = nChannel;
	m_attr = attr;
	m_file_idx = 0;
	m_nSpeed = 1;

	if (SetSocket())
	{
		cout << "[SENDER.ch" << m_nChannel << "] SetSocket config is completed" << endl;
		Start();
	}

	return true;
}

bool CSender::SetMutex(pthread_mutex_t *mutex)
{
	m_mutex_sender = mutex;
	cout << "[SENDER.ch" << m_nChannel << "] m_mutex_sender address : " << m_mutex_sender << endl;
}

bool CSender::SetSocket()
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

void CSender::Run()
{
	while (!m_bExit)
	{
		if (!Send())
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

bool CSender::Send()
{
	//cout << "[SENDER.ch" << m_nChannel << "] Send() Alive" << endl;
	//GetOutputs(m_attr["file_dst"].asString());
	stringstream sstm;
	sstm.str("");
	sstm << m_attr["file_dst"].asString() << "/" << m_attr["target"].asString() << "/" << m_nChannel;
	cout << "[SENDER.ch" << m_nChannel << "] target : " << sstm.str() << endl;
	if (!GetChannelFiles(sstm.str()))
	{
		m_bExit = true;
		// 자원회수는 나중에
	}

	//cout << "[SENDER.ch" << m_nChannel << "] one loop completed" << endl;
	//usleep(1000);

	return true;
}

int CSender::Demux(string src_filename)
{
#if __DUMP
	string channel;
	string index = to_string(m_index);
	channel = to_string(m_nChannel);
	string es2_name = "./" + channel + "_" + index + ".es";
	cout << "es_name : " << es2_name << endl;
#endif
	m_index++;

#if __DUMP
	FILE *es2 = fopen(es2_name.c_str(), "wb");
#endif

	ifstream ifs;

	if (m_attr["type"].asString() == "video")
	{
		if (avformat_open_input(&fmt_ctx, src_filename.c_str(), NULL, NULL) < 0)
		{
			fprintf(stderr, "[SENDER] Could not open source file %s\n", src_filename.c_str());
			return EXIT_FAILURE;
		}
		else
		{
			cout << "[SENDER.ch" << m_nChannel << "] " << src_filename << " file is opened" << endl;
		}

		m_bsf = av_bsf_get_by_name("hevc_mp4toannexb");

		if (av_bsf_alloc(m_bsf, &m_bsfc) < 0)
		{
			_d("Failed to alloc bsfc\n");
		}
		if (avcodec_parameters_copy(m_bsfc->par_in, fmt_ctx->streams[0]->codecpar) < 0)
		{
			_d("Failed to copy codec param.\n");
		}
		m_bsfc->time_base_in = fmt_ctx->streams[0]->time_base;

		if (av_bsf_init(m_bsfc) < 0)
		{
			_d("Failed to init bsfc\n");
		}
	}
	else if (m_attr["type"].asString() == "audio")
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
			//
		}
	}

#if 0
	av_init_packet(&m_pkt);
	m_pkt.data = NULL;
	m_pkt.size = 0;
#endif
	double fTime = m_info["fps"].asDouble();
	double num = m_info["num"].asDouble();
	double den = m_info["den"].asDouble();
	double target_time = num / den * 1000000 / m_nSpeed;
	int64_t acc_time = 0;
	int64_t tick_diff = 0;
	high_resolution_clock::time_point begin;
	high_resolution_clock::time_point end;
	cout << "[SENDER.ch" << m_nChannel << "] " << target_time << ", " << num << ", " << den << ", type : " << m_attr["type"].asString() << endl;

	begin = high_resolution_clock::now();
	while (!m_bExit)
	{
		if (m_attr["type"].asString() == "video")
		{
			AVPacket pkt;
			av_init_packet(&pkt);
			if (av_read_frame(fmt_ctx, &pkt) < 0)
			{
				cout << "[SENDER.ch" << m_nChannel << "] meet EOF" << endl;
				avformat_close_input(&fmt_ctx);
				break;
			}

			//end = high_resolution_clock::now();
			//tick_diff = duration_cast<microseconds>(end - begin).count();
#if 0
		av_bsf_send_packet(m_bsfc, &pkt);

		while (av_bsf_receive_packet(m_bsfc, &pkt) == 0)
		{
			send_bitstream(pkt.data, pkt.size);
			begin = end;
		}
#endif
			if (!send_bitstream(pkt.data, pkt.size))
			{
				cout << "[SENDER.ch" << m_nChannel << "] send_bitstream failed" << endl;
			}
			av_packet_unref(&pkt);
		}
		else if (m_attr["type"].asString() == "audio")
		{
			char audio_buf[AUDIO_BUFF_SIZE];
			ifs.read(audio_buf, AUDIO_BUFF_SIZE);
			//cout << hex << audio_buf << endl;
			if (!send_audiostream(audio_buf, AUDIO_BUFF_SIZE))
			{
				cout << "[SENDER.ch" << m_nChannel << "] send_audiostream failed" << endl;
			}

			if (ifs.eof())
			{
				ifs.close();
				cout << "[SENDER.ch" << m_nChannel << "] audio meet eof" << endl;
				break;
			}
			//if meet eof then break
		}

		while (tick_diff < target_time)
		{
			//usleep(1); // 1000000 us = 1 sec
			end = high_resolution_clock::now();
			tick_diff = duration_cast<microseconds>(end - begin).count();
			//cout << tick_diff << endl;
			this_thread::sleep_for(microseconds(1));
			//acc_time += tick_diff;
			//begin = high_resolution_clock::now();
		}
		begin = end;
		cout << "[SENDER.ch" << m_nChannel << "] num : " << num << ", den : " << den << ", target_time : " << target_time << ", tick_diff : " << tick_diff << endl;
		tick_diff = 0;
		//cout << "[SENDER.ch" << m_nChannel << "] num : " << num << ", den : " << den << ", target_time : " << target_time << ", tick_diff : " << tick_diff << endl;
		//cout << tick_diff << ", acc_time : " << acc_time << endl;
	}

#if __DUMP
	fclose(es2);
#endif
	cout << "[SENDER.ch" << m_nChannel << "] " << src_filename << " file is closed" << endl;
	return true;
}

bool CSender::GetOutputs(string basepath)
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

bool CSender::GetChannelFiles(string path)
{
	if (!IsDirExist(path))
	{
		cout << "[SENDER.ch" << m_nChannel << "] " << path << " is not exist" << endl;
		return false;
	}

	ifstream ifs(path + "/" + "meta.json");
	if (!ifs.is_open())
	{
		//is not open or not exist
		cout << "[SENDER.ch" << m_nChannel << "] " << path + "/" + "meta.json"
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
					cout << "[SENDER.ch" << m_nChannel << "] path : " << path << "/" << ent->d_name << endl;
					ss << path << "/" << ent->d_name;
					if (getFilesize(ss.str().c_str()) == 0)
					{
						cout << "[SENDER.ch" << m_nChannel << "] " << ss.str() << " size is 0" << endl;
						continue;
					}
					if (Demux(ss.str()))
					{
						cout << "[SENDER.ch" << m_nChannel << "] " << ss.str() << " demux success" << endl;
					}
					ss.str("");
				}
			}
		}
	}
	closedir(dir);
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

	uint8_t *p = stream;
	uint8_t buffer[PACKET_SIZE + 16];

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
		memcpy(&buffer[16], p, cur_size);

		nSendto = sendto(m_sock, buffer, cur_size + 16, 0, (struct sockaddr *)&m_mcast_group, sizeof(m_mcast_group));
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
#if 0
int CSender::open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
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
int CSender::get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt)
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
	cout << "[SENDER.ch" << m_nChannel << "] sock " << m_sock << " closed" << endl;
	close(m_sock);
}
