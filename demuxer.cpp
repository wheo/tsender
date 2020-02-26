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

bool CDemuxer::Create(Json::Value info, Json::Value attr, int nChannel)
{
	fmt_ctx = NULL;
	m_info = info;
	m_nChannel = nChannel;
	m_attr = attr;
	m_file_idx = 0;
	m_nSpeed = 0;
	m_bIsRerverse = false;
	m_IsMove = false;
	m_nTotalFrame = 0;
	m_files = NULL;

	m_pStream = NULL;

	m_CSender = NULL;
	m_queue = NULL;

	m_current_pts = 0;
	m_first_pts = 0;
	m_last_pts = 0;
	m_bDisable = false;

	m_audio_status = 0;
	m_isFOF = false;
	m_isEOF = false;
	m_offset_pts = 0;

	m_seek_pts = -1;

	m_n_gop = m_info["gop"].asInt();
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
#if 0
	m_CThumbnail = new CThumbnail();
#endif
	int nQueueSize = 0;
	if (m_nChannel < 4)
	{
		nQueueSize = MAX_NUM_ANALOG_QUEUE;
	}
	else if (m_nChannel == 4 || m_nChannel == 5)
	{
		nQueueSize = 2;
	}
	else if (m_nChannel == 6 || m_nChannel == 7)
	{
		nQueueSize = MAX_NUM_AUDIO_QUEUE;
	}

	string type = GetChannelType(m_nChannel);

	m_type = type;

	m_queue = new CQueue(nQueueSize, m_nChannel, type);
	cout << "[DEMUXER.ch" << m_nChannel << "] type : " << type << ", size : " << nQueueSize << endl;

	m_CSender = new CSender();
	m_CSender->SetQueue(&m_queue, nChannel);
	m_CSender->Create(info, attr, type, nChannel);

	Start();
	return true;
}

string CDemuxer::GetChannelType(int nChannel)
{
	string ret_type = "";
	if (nChannel < 6)
	{
		ret_type = "video";
	}
	else if (nChannel >= 6)
	{
		ret_type = "audio";
	}
	return ret_type;
}

bool CDemuxer::SetMutex(pthread_mutex_t *mutex)
{
	m_mutex_demuxer = mutex;
	//cout << "[DEMUXER.ch" << m_nChannel << "] m_mutex_sender address : " << m_mutex_demuxer << endl;
}

int64_t CDemuxer::GetCurrentOfffset()
{
	return m_offset_pts;
}

int64_t CDemuxer::GetCurrentPTS()
{
	return m_queue->GetCurrentPTS();
}

void CDemuxer::Disable()
{
	m_bDisable = true;
}

void CDemuxer::Enable()
{
	m_bDisable = false;
}

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

	if (Demux(meta["files"]))
	{
		cout << "[DEMUXER.ch" << m_nChannel << "] success" << endl;
		m_current_pts = 0;
	}
	return true;
}

int CDemuxer::Demux(Json::Value files)
{
	string src_filename = "";
	Json::Value value;
	// file 데이터 전역변수로 복사
	m_files = files;

	for (int i = 0; i < files.size(); i++)
	{
		m_nTotalFrame += files[i]["frame"].asInt();
	}

	int codec = m_attr["codec"].asInt(); // 0 : H264, 1 : HEVC
	bool pauseOld = false;
	bool reverseOld = false;
	int speedOld = 0;
	bool MoveFirstPacket = false;
	char isvisible = 1;
	for (int i = 0; i < files.size(); i++)
	{
		value = files[i];
		if (m_bExit == true)
		{
			break;
		}

		m_IsAudioRead = false;
		src_filename = value["name"].asString();

		//현재파일의 처음 PTS
		m_first_pts = files[i]["first_pts"].asInt64();
		//현재파일의 마지막 PTS
		m_last_pts = files[i]["last_pts"].asInt64();

		ifstream ifs;

		int ret = 0;
		if (m_type == "video")
		{
			if (avformat_open_input(&fmt_ctx, src_filename.c_str(), NULL, NULL) < 0)
			{
				cout << "[DEMUXER.VIDEO.ch" << m_nChannel << "] Could not open source file : " << src_filename << endl;
				return false;
			}
			else
			{
				m_pStream = fmt_ctx->streams[0];
			}

			ret = avformat_find_stream_info(fmt_ctx, NULL);
			if (ret < 0)
			{
				cout << "[DEMUXER.VIDEO.ch" << m_nChannel << "] failed to find proper stream in FILE : " << src_filename << endl;
			}

			if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0)
			{
				video_stream = fmt_ctx->streams[video_stream_idx];

				/* dump input information to stderr */
				av_dump_format(fmt_ctx, 0, src_filename.c_str(), 0);

				if (!video_stream)
				{
					fprintf(stderr, "[DEMUXER.VIDEO] Could not find audio or video stream in the input, aborting\n");
					ret = 1;
				}
			}

			if (codec == 0) // h264
			{
				m_bsf = av_bsf_get_by_name("h264_metadata");
			}
			else if (codec == 1) //hevc
			{
				m_bsf = av_bsf_get_by_name("hevc_metadata");
			}

			if (i == 0) // 그룹폴더내의 첫번째 파일일 경우에만 필터 초기화를 할 것!!!
			{
				if (av_bsf_alloc(m_bsf, &m_bsfc) < 0)
				{
					_d("[[DEMUXER.VIDEO] Failed to alloc bsfc\n");
					return false;
				}
				if (avcodec_parameters_copy(m_bsfc->par_in, fmt_ctx->streams[0]->codecpar) < 0)
				{
					_d("[[DEMUXER.VIDEO] Failed to copy codec param.\n");
					return false;
				}
				m_bsfc->time_base_in = fmt_ctx->streams[0]->time_base;

				if (av_bsf_init(m_bsfc) < 0)
				{
					_d("[[DEMUXER.VIDEO] Failed to init bsfc\n");
					return false;
				}
			}
		}
		else if (m_type == "audio")
		{
			// fileopen
			ifs.open(src_filename, ios::in);
			if (!ifs.is_open())
			{
				cout << "[[DEMUXER.AUDIO.ch" << m_nChannel << "] " << src_filename << " file open failed" << endl;
				return false;
			}
			else
			{
				//cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << " file open success" << endl;
			}
		}

		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		pkt.pts = 0;

		while (!m_bExit)
		{
			bool isPause = m_IsPause;
			bool isReverse = m_bIsRerverse;
			bool isMove = m_IsMove;
			int64_t seek_pts = m_seek_pts;
			int nSpeed = m_nSpeed;

			if (m_type == "video")
			{
				if (isMove == true)
				{
					m_isEOF = false;
				}
				if (m_isEOF == true)
				{
					continue;
				}

				//int pkt_dup_count = 1;
				if (isMove == true)
				{
					//m_isEOF = false;
					m_IsMove = false;
					if (m_queue)
					{
						m_queue->Clear();
					}
					if (m_nMoveIdx < files.size())
					{
						i = m_nMoveIdx - 1;
					}
					else
					{
						i = files.size() - 2;
					}
					//cout << "[DEMUXER.VIDEO.ch" << m_nChannel << "] file (" << i << "/" << files.size() << ")" << endl;
					break;
				}

				//reverse가 먼저야 seek가 먼저야? 확인하자 20200221
				if (seek_pts == 0)
				{
					if (isReverse == true)
					{
						m_isEOF = false;
						if (Reverse())
						{
							//Reverse continue
						}
						else
						{
							m_isFOF = true;
						}
					}
				}

				if (seek_pts > 0)
				{
					m_seek_pts = 0;
					SeekPTS(seek_pts);
				}

				if (m_isFOF == false && m_isEOF == false)
				{
					av_packet_unref(&pkt);
					av_init_packet(&pkt);

					pkt.data = NULL;
					pkt.size = 0;
					//av_init_packet 에 포함되었으므로 초기화 할 필요 없음
					//pkt.pts = 0;

					if (av_read_frame(fmt_ctx, &pkt) < 0)
					{
						if (i + 1 < files.size())
						{
							cout << "[DEMUXER.VIDEO.ch" << m_nChannel << "] meet EOF(" << fmt_ctx->filename << "), (" << i + 1 << "/" << files.size() << ")" << endl;
							avformat_close_input(&fmt_ctx);
							fmt_ctx = NULL;
							//cout << "[DEMUXER.ch" << m_nChannel << "] meet index(" << i + 1 << "/" << files.size() << ")" << endl;
							break;
						}
						else
						{
							m_isEOF = true;
							cout << "[DEMUXER.VIDEO.ch" << m_nChannel << "] meet END OF FILELIST (" << i + 1 << "/" << files.size() << "), " << m_isEOF << ", " << m_isFOF << endl;
							continue;
						}
					}
					else
					{
						m_current_pts = pkt.pts;
						m_pStream = fmt_ctx->streams[0];
#if 0
						cout << "[DEMUXER.ch" << m_nChannel << "] av_read_frame (" << pkt.pts << "), size : " << pkt.size << endl;
#endif
						if (seek_pts > 0)
						{
							//m_offset_pts = pkt.pts; // 이렇게 하면 offset이 각 채널마다 달라지기 때문에 싱크가 맞지 않는다.
							//offset_pts 로 싱크를 맞추는 부분 SEEK를 할 경우 탐색 기준 PTS로 싱크를 맞추려고 시도함
							m_offset_pts = seek_pts + (16667 * 5);
						}
					}

					if ((ret = av_bsf_send_packet(m_bsfc, &pkt)) < 0)
					{
						av_log(fmt_ctx, AV_LOG_ERROR, "Failed to send packet to filter %s for stream %d\n", m_bsfc->filter->name, pkt.stream_index);
						cout << "[DEMUXER.VIDEO.ch" << m_nChannel << "] current pts : " << pkt.pts << endl;
					}
					// TODO: when any automatically-added bitstream filter is generating multiple
					// output packets for a single input one, we'll need to call this in a loop
					// and write each output packet.
					if ((ret = av_bsf_receive_packet(m_bsfc, &pkt)) < 0)
					{
						if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
						{
							av_log(fmt_ctx, AV_LOG_ERROR, "Failed to receive packet from filter %s for stream %d\n", m_bsfc->filter->name, pkt.stream_index);
							cout << "[DEMUXER.VIDEO.ch" << m_nChannel << "] current pts : " << pkt.pts << endl;
						}
						if (fmt_ctx->error_recognition & AV_EF_EXPLODE)
						{
							//
						}
					}
				}

				if (pkt.size > 0)
				{
					while (m_bExit == false)
					{
						if (nSpeed != speedOld)
						{
							m_queue->Clear();
						}
						if (nSpeed > 0)
						{
							if (pkt.flags != AV_PKT_FLAG_KEY)
							{
								break;
							}
						}
						if (m_queue->PutVideo(&pkt, m_offset_pts, isvisible) > 0)
						{
							//비디오 패킷 put 완료 후 시점
							//pkt_dup_count--;
							break;
						}
						else
						{
							//버퍼가 꽉 차서 버퍼에 pkt을 넣을 수 없을 때 1 usec 휴식
							usleep(1);
						}
					}
				}
				speedOld = nSpeed;
				if (m_isFOF == true && i > 0)
				{
					i = i - 2; // 직전 인덱스로 이동
					cout << "[DEMUXER.VIDEO.ch" << m_nChannel << "] File first pos and set index : " << i << endl;
					m_isFOF = false;
					break;
				}
				else if (m_isFOF == true && i == 0)
				{
					i = 0;
					cout << "[DEMUXER.VIDEO.ch" << m_nChannel << "] File first pos and set index : " << i << endl;
					m_isFOF = false;
				}
			}
			else if (m_type == "audio")
			{
				char audio_buf[AUDIO_BUFF_SIZE];

				if (isMove == true)
				{
					m_isEOF = false;
					if (m_nMoveIdx < files.size())
					{
						i = m_nMoveIdx - 1;
						m_IsMove = false;
						break;
					}
					else
					{
						i = files.size() - 2;
						m_isEOF = true;
						m_IsMove = false;
						continue;
					}
				}
				if (m_isEOF == true)
				{
					//cout << "[DEMUXER:AUDIO.ch" << m_nChannel << "] move eof , seekframe (" << m_nSeekFrame << ")" << endl;
					continue;
				}
#if 1
				if (seek_pts > 0)
				{
					//m_nSeekFrame = m_nSeekFrame - (last_frame * m_nMoveIdx);
					//MoveFrame(m_nMoveFrame);

					if (ifs.is_open())
					{
						m_queue->Clear();
						while (!m_bExit)
						{
							int64_t audio_pts;
							ifs.read(audio_buf, AUDIO_BUFF_SIZE);

							memcpy(&audio_pts, audio_buf, 8);
							if (audio_pts > seek_pts)
							{
								cout << "[DEMUXER:AudioSeek.ch" << m_nChannel << "] move audio idx(" << m_nMoveIdx << "), pts(" << audio_pts << "), (" << seek_pts << ")" << endl;
								m_current_pts = audio_pts;
								if (seek_pts > 0)
								{
									m_offset_pts = audio_pts;
								}
								m_seek_pts = 0;
								break;
							}
							else
							{
								//cout << "[DEMUXER:AudioSeek.ch" << m_nChannel << "] seeking ...... (" << audio_pts << "), (" << seek_pts << ")" << endl;
							}
						}
					}
					m_audio_status = 1;
				}
#endif
				if (ifs.is_open())
				{
					if (m_IsAudioRead == false)
					{
						//int64_t audio_pts;
						ifs.read(audio_buf, AUDIO_BUFF_SIZE);
						//memcpy(&audio_pts, audio_buf, 8);
						//cout << "[DEMUXER.AUDIO.ch" << m_nChannel << "] read audio from file : " << src_filename.c_str() << ", (" << audio_pts << ")" << endl;
						m_IsAudioRead = true;
					}
					if (m_queue->PutAudio(audio_buf, AUDIO_BUFF_SIZE, m_offset_pts, m_audio_status) > 0)
					{
						m_IsAudioRead = false;
						m_audio_status = 0;
					}
					else
					{
						usleep(1);
					}
				}

				if (ifs.eof())
				{
					ifs.close();
					if (i < files.size() - 1)
					{
						cout << "[DEMUXER.AUDIO.ch" << m_nChannel << "] audio (" << src_filename << ") meet EOF (" << i + 1 << "/" << files.size() << ")" << endl;
						break;
					}
					else
					{
						m_isEOF = true;
						cout << "[DEMUXER.AUDIO.ch" << m_nChannel << "] (" << src_filename << ") meet END OF FILELIST (" << i + 1 << "/" << files.size() << "), " << m_isEOF << ", " << m_isFOF << endl;
						continue;
					}
				}
			}
		}

		if (m_type == "video" && fmt_ctx)
		{
			//cout << "[DEMUXER.ch" << m_nChannel << "] avformat_close_input (" << fmt_ctx->filename << endl;
			avformat_close_input(&fmt_ctx);
			fmt_ctx = NULL;
		}
		if (m_type == "audio" && ifs.is_open())
		{
			ifs.close();
		}
		//cout << "[DEMUXER.ch" << m_nChannel << "] " << src_filename << " file is closed" << endl;
	}

	av_bsf_free(&m_bsfc);
	return true;
}

bool CDemuxer::SetMoveSec(int nSec)
{
	int64_t pts = nSec * AV_TIME_BASE;
#if FRAME
	m_nMoveIdx = FindFileIndexFromFrame(nFrame, &restFrame);
#else
	m_nMoveIdx = FindFileIndexFromPTS(pts);
#endif
	m_IsMove = true;
	m_seek_pts = pts;
	cout << "[DEMUXER.SetMoveSec.ch" << m_nChannel << "] input pts : " << pts << ", move index : " << m_nMoveIdx << endl;
}

bool CDemuxer::SetMovePTS(int64_t pts)
{
	m_nMoveIdx = FindFileIndexFromPTS(pts);
	m_IsMove = true;
	m_seek_pts = pts;
	cout << "[DEMUXER.SetMovePTS.ch" << m_nChannel << "] input pts : " << pts << ", move index : " << m_nMoveIdx << endl;
}

bool CDemuxer::SeekPTS(int64_t pts)
{
	int ret = 0;

	if (m_nChannel < 6)
	{
		ret = avformat_seek_file(fmt_ctx, 0, 0, pts, pts, AVSEEK_FLAG_FRAME);
		if (ret > -1)
		{
			_d("[DEMUXER:SeekPTS.ch%d] target_pts (%lld), current_pts(%lld) seek completed\n", m_nChannel, pts, m_current_pts);
		}
		else
		{
			cout << "[DEMUXER.SeekPTS.ch" << m_nChannel << "] seek error " << endl;
		}
	}
}

bool CDemuxer::Reverse()
{
	//avcodec_flush_buffers(fmt_ctx->streams[0]->codec);
	int ret = 0;
	int64_t reverse_target_pts = m_current_pts - 1000; // 지금보다 1000 전의 PTS

	if (m_nChannel < 6)
	{
		ret = avformat_seek_file(fmt_ctx, 0, 0, reverse_target_pts, reverse_target_pts, AVSEEK_FLAG_FRAME);
		if (ret < 0)
		{
			_d("[DEMUXER:REVERSE.ch%d] not work, target (%lld)\n", m_nChannel, reverse_target_pts);
			return false;
		}
		else
		{
			//_d("[DEMUXER:REVERSE.ch%d] (%lld) seek completed, (%lld ~ %lld)\n", m_nChannel, reverse_target_pts, m_first_pts, m_last_pts);
		}
#if 1
		if (m_first_pts > reverse_target_pts)
		{
			_d("[DEMUXER:REVERSE.ch%d] file first arrived, (%lld)/(%lld)\n", m_nChannel, m_first_pts, reverse_target_pts);
			return false;
		}
#endif
	}

	return true;
}

int CDemuxer::FindFileIndexFromPTS(int64_t pts)
{
	int ret_idx = 0;
	for (int i = 0; i < m_files.size(); i++)
	{
		int64_t first_pts = m_files[i]["first_pts"].asInt64();
		int64_t last_pts = m_files[i]["last_pts"].asInt64();
		//cout << "[FindPTS:ch" << m_nChannel << "] first : " << first_pts << ", last : " << last_pts << endl;
		if (pts < last_pts && pts >= first_pts)
		{
			break;
		}
		else
		{
			ret_idx++;
		}
	}
	return ret_idx;
}
#if 0
Json::Value CDemuxer::GetThumbnail(int nSec)
{
	Json::Value ret_json;
	ret_json = m_CThumbnail->GetThumbnail(nSec);
	return json;
}
#endif

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

void CDemuxer::SetSpeed(int speed)
{
	m_nSpeed = speed;
	//cout << "[DEMUXER.ch" << m_nChannel << "] Set speed : " << m_nSpeed << endl;
	if (m_CSender)
	{
		m_CSender->SetSpeed(speed);
	}
}

void CDemuxer::SetPause(bool state)
{
	m_IsPause = state;
	if (m_CSender)
	{
		m_CSender->SetPause(state);
	}
	//cout << "[DEMUXER.ch" << m_nChannel << "] Set Pause : " << m_IsPause << endl;
}

void CDemuxer::SetPauseSync(int64_t sync_pts)
{
	if (m_CSender)
	{
		m_CSender->SetPauseSync(sync_pts);
	}
}

void CDemuxer::SetSync(int64_t sync_pts)
{
	m_offset_pts = sync_pts;
}

void CDemuxer::SetReverse(bool state)
{
	m_bIsRerverse = state;
	//cout << "[DEMUXER.ch" << m_nChannel << "] Set Reverse : " << m_bIsRerverse << endl;
	if (m_CSender)
	{
		m_CSender->SetReverse(state);
	}
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

void CDemuxer::Delete()
{
	SAFE_DELETE(m_CSender);
}