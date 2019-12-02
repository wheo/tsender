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

	_d("[SENDER.ch%d] Trying to exit thread\n", m_nChannel);
	Terminate();
	_d("[SENDER.ch%d] exited...\n", m_nChannel);
	Delete();

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

	Start();

	return true;
}

bool CSender::SetMutex(pthread_mutex_t *mutex)
{
	m_mutex_sender = mutex;
	cout << "m_mutex_sender address : " << m_mutex_sender << endl;
}

bool CSender::SetSocket()
{
	cout << "[ch." << m_nChannel << "] : " << m_attr["file_dst"].asString() << endl;

	struct ip_mreq mreq;
	int state;

	memset(&m_mcast_group, 0x00, sizeof(m_mcast_group));
	m_mcast_group.sin_family = AF_INET;
	m_mcast_group.sin_port = htons(m_info["port"].asInt());
	m_mcast_group.sin_addr.s_addr = inet_addr(m_info["ip"].asString().c_str());

	m_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == m_sock)
	{
		_d("[RECV.ch.%d] socket createion error\n", m_nChannel);
		return false;
	}

	if (-1 == bind(m_sock, (struct sockaddr *)&m_mcast_group, sizeof(m_mcast_group)))
	{
		perror("RECV] bind error");
		return false;
	}

	uint reuse = 1;
	state = setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if (state < 0)
	{
		perror("[RECV] Setting SO_REUSEADDR error\n");
		return false;
	}

	uint ttl = 16;
	state = setsockopt(m_sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
	if (state < 0)
	{
		perror("[RECV] Setting IP_MULTICAST_TTL error\n");
		return false;
	}

	mreq.imr_multiaddr = m_mcast_group.sin_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	if (setsockopt(m_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	{
		perror("[RECV] add membership setsocket opt");
		return false;
	}

	struct timeval read_timeout;
	read_timeout.tv_sec = 1;
	read_timeout.tv_usec = 0;
	if (setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0)
	{
		perror("[RECV] set timeout error");
		return false;
	}
}

void CSender::Run()
{
	//m_pMuxer = new CTSMuxer();
	//m_pPktPool = new CMyPacketPool();

#if __DEBUG
	pthread_mutex_lock(&m_mutex_sender);
	cout << "[ch." << m_nChannel << "] : " << m_info["ip"].asString() << endl;
	cout << "[ch." << m_nChannel << "] : " << m_info["port"].asInt() << endl;
	cout << "[ch." << m_nChannel << "] : " << m_info["fps"].asDouble() << endl;
	pthread_mutex_unlock(&m_mutex_sender);
#endif

	SetSocket();

	while (!m_bExit)
	{
		if (!Send())
		{
			//error send() return false
		}
		else
		{
			cout << "[SENDER] sender loop" << endl;
			// send() eturn true
		}
		usleep(1000);
	}
	//SAFE_DELETE(m_pMuxer);
	//SAFE_DELETE(m_pPktPool);
}

bool CSender::GetOutputs(string basepath)
{
	string path;
	DIR *dir = opendir(basepath.c_str());
	struct dirent *ent;

	string channel;

	channel = to_string(m_nChannel);

	pthread_mutex_lock(m_mutex_sender);
	while ((ent = readdir(dir)) != NULL && !m_bExit)
	{
		pthread_mutex_unlock(m_mutex_sender);
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
	pthread_mutex_unlock(m_mutex_sender);
	closedir(dir);
	return EXIT_SUCCESS;
}

bool CSender::GetChannelFiles(string path)
{
	DIR *dir = opendir(path.c_str());
	struct dirent *ent;
	stringstream ss;
	cout << "path : " << path << endl;

	while ((ent = readdir(dir)) != NULL && !m_bExit)
	{
		if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
		{
			if (ent->d_type != DT_DIR)
			{
				cout << "channel : " << m_nChannel << ", path : " << path << "/" << ent->d_name << endl;
				ss << path << "/" << ent->d_name;
				Demux(ss.str());
				ss.str("");
			}
		}
	}
	return EXIT_SUCCESS;
}

bool CSender::Send()
{
	if (m_nChannel == 0)
	{
		//cout << "[SENDER.ch" << m_nChannel << "] Send() Alive" << endl;
		GetOutputs(m_attr["file_dst"].asString());
		cout << "[SENDER.ch" << m_nChannel << "] one loop completed" << endl;
		usleep(10);
	}

	return true;
}

int CSender::Demux(string src_filename)
{
	string channel;
	string index = to_string(m_index);
	channel = to_string(m_nChannel);
	string es2_name = "/opt/tnmtech/" + channel + "_" + index + ".264";
	cout << "es_name : " << es2_name << endl;
	m_index++;

#if __DUMP
	FILE *es2 = fopen(es2_name.c_str(), "wb");
#endif

	if (avformat_open_input(&fmt_ctx, src_filename.c_str(), NULL, NULL) < 0)
	{
		fprintf(stderr, "Could not open source file %s\n", src_filename.c_str());
		return EXIT_FAILURE;
	}
	else
	{
		cout << "[SENDER.ch" << m_nChannel << "] " << src_filename << " file is opened" << endl;
	}

	m_bsf = av_bsf_get_by_name("h264_mp4toannexb");

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

#if 0
	av_init_packet(&m_pkt);
	m_pkt.data = NULL;
	m_pkt.size = 0;
#endif

	double fTime = m_info["fps"].asDouble();
	long long wait_until;

	high_resolution_clock::time_point begin = high_resolution_clock::now();
	while (!m_bExit)
	{
		AVPacket pkt;
		av_init_packet(&pkt);
		if (av_read_frame(fmt_ctx, &pkt) < 0)
		{
			cout << "[SENDER] meet EOF" << endl;
			break;
		}

		high_resolution_clock::time_point end = high_resolution_clock::now();
		int64_t tick_diff = duration_cast<microseconds>(end - begin).count();
		//cout << "tick-diff : " << tick_diff << " ";

		av_bsf_send_packet(m_bsfc, &pkt);

		while (av_bsf_receive_packet(m_bsfc, &pkt) == 0)
		{
			send_bitstream(pkt.data, pkt.size);
			begin = end;
#if __DUMP
			fwrite(pkt.data, 1, pkt.size, es2);
#endif
		}
		usleep(3333);
		av_packet_unref(&pkt);
	}
#if __DUMP
	fclose(es2);
#endif
	avformat_close_input(&fmt_ctx);
	cout << "[SENDER.ch" << m_nChannel << "] " << src_filename << " file is closed" << endl;

	return EXIT_SUCCESS;
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

		nSendto = sendto(m_sock, buffer, PACKET_SIZE + 16, 0, (struct sockaddr *)&m_mcast_group, sizeof(m_mcast_group));
		if (nSendto < 0)
		{
			//printf("%s : failed to send\n", __func__);
			return false;
		}
		else
		{
#if __DEBUG
			cout << "[SENDER.ch" << m_nChannel << "] "
				 << "tot_size : " << tot_size << ", cur_size : " << cur_size << ", tot_packet : " << tot_packet << ", cur_packet : " << cur_packet << endl;
#endif
		}

		remain -= cur_size;
		p += cur_size;
		cur_packet++;
	}

	return true;
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
	cout << "sock " << m_sock << " closed" << endl;
	close(m_sock);
}
