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
	m_sync_cnt = 0;
	m_IsPause = false;
	m_bIsRerverse = false;
	m_IsMove = false;
	m_nTotalFrame = 0;
	m_files = NULL;
	m_file_first_pts = 0;
	m_reverse_count = 0;
	m_reverse_pts = 0;
	m_first_reverse_pts = 0;
	m_sync_pts = 0;
	m_b_sync_check = false;
	m_pStream = NULL;

	m_CSender = NULL;
	m_queue = NULL;

	m_current_pts = 0;
	m_bDisable = false;

	m_n_gop = m_info["gop"].asInt();
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
#if 0
	m_CThumbnail = new CThumbnail();
#endif
	int nQueueSize = 0;
	if (m_nChannel < 4)
	{
		nQueueSize = 10;
	}
	else if (m_nChannel >= 4)
	{
		nQueueSize = 2;
	}

	m_queue = new CQueue(nQueueSize);
	m_queue->SetInfo(m_nChannel, m_info["type"].asString());

	cout << "[DEMUXER.ch" << m_nChannel << "] type : " << m_info["type"].asString() << endl;

	m_CSender = new CSender();
	m_CSender->Create(info, attr, nChannel);
	m_CSender->SetQueue(&m_queue, nChannel);

	Start();
	return true;

#if 0
	if (SetSocket())
	{
		cout << "[DEMUXER.ch" << m_nChannel << "] SetSocket config is completed" << endl;
		Start();
	}
#endif
}

bool CDemuxer::SetMutex(pthread_mutex_t *mutex)
{
	m_mutex_demuxer = mutex;
	//cout << "[DEMUXER.ch" << m_nChannel << "] m_mutex_sender address : " << m_mutex_demuxer << endl;
}

uint64_t CDemuxer::GetCurrentPTS()
{
	uint64_t pts = 0;
	if (m_bIsRerverse == true)
	{
		if (m_queue)
		{
			pts = m_queue->GetCurrentVideoPTS();
		}
		return pts;
	}
	else
	{
		return m_current_pts;
	}
}

void CDemuxer::SetSyncPTS(uint64_t pts)
{
	m_sync_pts = pts;
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
	}

	return true;
}

int CDemuxer::Demux(Json::Value files)
{
	string src_filename = "";
	Json::Value value;
	//m_nFrameCount = 0;
	m_seek_pts = 0;
	// file 데이터 전역변수로 복사
	m_files = files;

	int pts_diff = 0;
	int64_t lldur = 0;
	uint64_t old_pts = 0;

	for (int i = 0; i < files.size(); i++)
	{
		m_nTotalFrame += files[i]["frame"].asInt();
		m_file_cnt++;
	}

	int codec = m_attr["codec"].asInt(); // 0 : H264, 1 : HEVC
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();

	bool isPause = false;
	bool pauseOld = false;
	bool isReverse = false;
	bool reverseOld = false;

	for (int i = 0; i < files.size(); i++)
	{
		value = files[i];

		if (m_bExit == true)
		{
			break;
		}
		uint64_t last_frame = value["frame"].asInt64();
		//int keyframe_speed = m_attr["keyframe_speed"].asInt();

		if (last_frame == 0)
		{
			break;
		}

		src_filename = value["name"].asString();
		string type = m_info["type"].asString();
		ifstream ifs;

		m_IsAudioRead = false;

		int ret = 0;

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

				m_pStream = fmt_ctx->streams[0];
				m_timeBase = m_pStream->time_base;
				lldur = num * m_timeBase.den / den;
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
				else
				{
					m_CSender->SetAVFormatContext(&fmt_ctx);
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

		m_file_first_pts = 0;

		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		pkt.pts = 0;

		while (!m_bExit)
		{
			isPause = m_IsPause;
			isReverse = m_bIsRerverse;
			if (type == "video")
			{
				int pkt_dup_count = 1;
				if (m_IsMove == true)
				{
					m_IsMove = false;
					if (isReverse == false)
					{
						i = m_nMoveIdx - 1;
					}
					cout << "[DEMUXER.ch" << m_nChannel << "] set index : " << i << endl;
					break;
				}

				if (m_nSeekFrame > 0)
				{
					SeekFrame(m_nSeekFrame);
					m_nSeekFrame = 0;
				}
				if (isPause == false)
				{
					if (isReverse == true)
					{
						m_reverse_count++;
						if (m_reverse_count == 1)
						{
							//reverse가 시작하면 현재 pts를 기준으로 역탐색을 시작함
							m_reverse_pts = pkt.pts;
						}

						if (Reverse())
						{
							//Reverse continue
						}
						else
						{
							i = i - 2; // 직전 인덱스로 이동
							//m_nFrameCount = last_frame;
							cout << "[DEMUXER.ch" << m_nChannel << "] set index : " << i << endl;
							break;
						}
					}
				}
				if (isReverse != reverseOld)
				{
					//Reverse 상태 (역배 -> 정배)
					if (isReverse == false)
					{
						m_reverse_count = 0;
						if (isPause == false)
						{
							//역배 -> 정배 & 멈춤 상태가 아닌경우
							if (m_queue)
							{
								m_queue->Clear();
								uint64_t nFrame = m_queue->GetCurrentVideoPTS() / (m_timeBase.den * num / den);
								cout << "[DEMUXER.ch" << m_nChannel << "] moveFrame " << nFrame << endl;
								SetMoveFrame(nFrame);
							}
						}
						else if (isPause == true)
						{
							//정배로 돌아올 땐 sync_pts로 seek를 함
							cout << "[DEMUXER.ch" << m_nChannel << "] !!!!!!!!!!!!!!!!!!!!!!!!!!! " << m_sync_pts << endl;
							//SeekPTS(m_queue->GetCurrentVideoPTS());
							//SeekPTS(m_sync_pts);
							uint64_t nFrame = m_sync_pts / (m_timeBase.den * num / den);
							cout << "[DEMUXER.ch" << m_nChannel << "] moveFrame " << nFrame << endl;
							SetMoveFrame(nFrame);

							m_queue->Enable();
						}
					}
					else if (isReverse == true)
					{
						//Reverse 상태 (정배 -> 역배)
						if (isPause == false)
						{
							//정배 -> 역배 & 멈춤 상태가 아닌경우
							m_queue->Clear();
							//SeekPTS(m_current_pts);
						}
						else if (isPause == true)
						{
							//
						}
					}
					cout << "[DEMUXER.ch" << m_nChannel << "] reverse status change : " << isReverse << endl;
				}

				reverseOld = isReverse;

				if (isPause == false || (m_sync_pts > 0 && m_sync_pts > m_current_pts))
				{
					if (m_sync_pts < m_current_pts)
					{
						m_sync_pts = 0;
					}
					av_packet_unref(&pkt);
					av_init_packet(&pkt);

					pkt.data = NULL;
					pkt.size = 0;
					//init에 포함
					//pkt.pts = 0;

					if (av_read_frame(fmt_ctx, &pkt) < 0)
					{
						cout << "[DEMUXER.ch" << m_nChannel << "] meet EOF(" << fmt_ctx->filename << endl;
						avformat_close_input(&fmt_ctx);
						fmt_ctx = NULL;
						break;
					}
					else
					{
						m_current_pts = pkt.pts;
						m_pStream = fmt_ctx->streams[0];
						m_timeBase = m_pStream->time_base;
						//cout << "[DEMUXER.ch" << m_nChannel << "] av_read_frame : " << pkt.pts << endl;

						if (isReverse == true)
						{
							//cout << "[DEMUXER.ch" << m_nChannel << "] reverse pts(" << m_reverse_count << ") : " << pkt.pts << endl;
							m_reverse_pts = m_reverse_pts - ((m_timeBase.den / den) * num * m_n_gop);
						}

						if (m_file_first_pts == 0)
						{
							m_file_first_pts = fmt_ctx->start_time;
						}
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
					//cout << "[DEMUXER.ch" << m_nChannel << "] m_nSeekFrame : " << m_nSeekFrame << ", seek_pts : " << m_seek_pts << ", current pts : " << pkt.pts << ", size : " << pkt.size << endl;
				}

				if (isPause != pauseOld)
				{
					cout << "[DEMUXER.ch" << m_nChannel << "] current_pts : " << m_current_pts << endl;
					cout << "[DEMUXER.ch" << m_nChannel << "] m_sync_pts : " << m_sync_pts << endl;

					if (isPause == true)
					{
						if (isReverse == true)
						{
							m_queue->Disable();
							m_queue->Clear();
							m_bIsRerverse = false;
							if (m_CSender)
							{
								m_CSender->SetReverse(false);
							}
							cout << "[DEMUXER.ch" << m_nChannel << "] reverse & pause , reverse_pts : " << m_reverse_pts << ", current_pts " << m_current_pts << endl;
						}
					}
#if 0
					if (isPause == false)
					{
						m_queue->Clear();
					}
					else if (isPause == true)
					{
						m_queue->Clear();
					}
#endif
				}
				pauseOld = isPause;

				if (pkt.size > 0)
				{
					if (m_nChannel > 3)
					{
						pts_diff = llabs(pkt.pts - old_pts);
						if (pts_diff == lldur * 2 && isPause == false)
						{
							//여기서 같은 pkt 버퍼 한개 더 보충함
							pkt_dup_count++;
							cout << "[DEMUXER.ch" << m_nChannel << "]  diff (" << pts_diff << "), (" << lldur << "), pts : (" << pkt.pts << "), old (" << old_pts << ")" << endl;
						}
					}

					while (m_bExit == false)
					{
						if (pkt.pts != 0 && pkt.pts == old_pts)
						{
							//cout << "[DEMUXER.ch" << m_nChannel << "] (" << pkt.pts << ") same" << endl;
							break;
						}
						else
						{
							//cout << "[DEMUXER.ch" << m_nChannel << "] (" << pkt.pts << ") put" << endl;
						}
						if (pkt.pts > -1)
						{
							if (m_queue->PutVideo(&pkt, m_seek_pts) > 0)
							{
								//비디오 패킷 put 완료 후 시점
								pkt_dup_count--;
								cout << "[DEMUXER.ch" << m_nChannel << "] put!!!!!!!, diff (" << pts_diff << "), (" << lldur << "), pts : (" << pkt.pts << "), old (" << old_pts << ")" << endl;
								if (pkt_dup_count == 0)
								{
									usleep(10);
									break;
								}
							}
							else
							{
								//버퍼가 꽉 차서 버퍼에 pkt을 넣을 수 없을 때 1000 usec 휴식
								usleep(1000);
							}
						}
					}
					old_pts = pkt.pts;
				}
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

					cout << "[DEMUXER.ch" << m_nChannel << "] move audio frame (" << m_nSeekFrame << ")" << endl;
					//MoveFrame(m_nMoveFrame);
					ifs.seekg(m_nSeekFrame * AUDIO_BUFF_SIZE);
					m_nSeekFrame = 0;
					m_seek_pts = (m_nAudioCount * AV_TIME_BASE * num) / den;
					m_IsAudioRead = true;
					//m_queue->Clear();
					//m_start_pts = high_resolution_clock::now();
				}

				if (isPause == false)
				{
					if (m_IsAudioRead == false)
					{
						ifs.read(audio_buf, AUDIO_BUFF_SIZE);
						m_nAudioCount++;
						m_IsAudioRead = true;
						//cout << "[DEMUXER.ch" << m_nChannel << "] read audio from file : " << m_nAudioCount << endl;
					}
				}

				isPause = pauseOld;

				if (m_IsAudioRead == true)
				{
					if (m_queue->PutAudio(audio_buf, AUDIO_BUFF_SIZE, m_seek_pts) > 0)
					{
						m_IsAudioRead = false;
					}
				}

				if (ifs.eof())
				{
					ifs.close();
					cout << "[DEMUXER.ch" << m_nChannel << "] audio meet eof" << endl;
					break;
				}
			}
		}

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
	cout << "[DEMUXER.ch" << m_nChannel << "] input frame : " << nFrame << ", move index : " << m_nMoveIdx << endl;

	m_nSeekFrame = nFrame;
	m_IsMove = true;
}

bool CDemuxer::SetMoveFrame(uint64_t nFrame)
{
	double ret;
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();
	//uint64_t nFrame = (nSec * den) / num;
	m_nMoveIdx = FindFileIndexFromFrame(nFrame);
	cout << "[DEMUXER.ch" << m_nChannel << "] input frame : " << nFrame << ", move index : " << m_nMoveIdx << endl;

	m_nSeekFrame = nFrame;
	m_IsMove = true;
}

bool CDemuxer::SetMoveAudioCount(uint64_t audioCount)
{
	double ret;
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();
	m_nMoveIdx = FindFileIndexFromFrame(audioCount);
	cout << "[DEMUXER.ch" << m_nChannel << "] input audioCount : " << audioCount << ", move index : " << m_nMoveIdx << endl;

	m_nSeekFrame = audioCount;
	m_IsMove = true;
}

bool CDemuxer::SeekFrame(int nFrame)
{
	uint64_t num = m_info["num"].asInt();
	uint64_t den = m_info["den"].asInt();

	int ret = 0;
	//double fTime = 0;
	//AVStream *pStream = fmt_ctx->streams[0];
	//fTime = (((double)nFrame * pStream->avg_frame_rate.den) / pStream->avg_frame_rate.num) - 0.5;
	//fTime = (((double)nFrame * pStream->avg_frame_rate.den) / pStream->avg_frame_rate.num);
	//fTime = max(fTime, 0.);

	//AVRational timeBaseQ;
	//AVRational timeBase = pStream->time_base;

	//timeBaseQ.num = 1;
	//timeBaseQ.den = AV_TIME_BASE;

	AVPacket pkt;
	av_init_packet(&pkt);
	av_read_frame(fmt_ctx, &pkt);

	if (m_file_first_pts == 0)
	{
		m_file_first_pts = fmt_ctx->start_time;
		cout << "[DEMUXER.SeekFrame.ch" << m_nChannel << "] first pts : " << (m_file_first_pts / AV_TIME_BASE) * den << endl;
		cout << "[DEMUXER.SeekFrame.ch" << m_nChannel << "] start : " << fmt_ctx->start_time << ", end : " << fmt_ctx->start_time + fmt_ctx->duration << endl;
	}
	av_packet_unref(&pkt);

#if 0
	int64_t tm = (int64_t)(fTime * AV_TIME_BASE);
	tm = av_rescale_q(tm, timeBaseQ, timeBase);
#endif
	int64_t tm = nFrame * lldur;

	if (m_nChannel < 6)
	{
		//ret = avformat_seek_file(fmt_ctx, 0, 0, tm, tm, 0);
		//avcodec_flush_buffers(fmt_ctx->streams[0]->codec);
		ret = avformat_seek_file(fmt_ctx, 0, 0, tm, tm, AVSEEK_FLAG_FRAME);
		if (ret > -1)
		{
			_d("[DEMUXER:SeekFrame.ch%d] ret : %d , tm(%lld) seek completed\n", m_nChannel, ret, tm);
			if (m_bIsRerverse == true)
			{
				m_reverse_pts = tm;
			}
		}
		else
		{
			cout << "[DEMUXER.SeekFrame.ch" << m_nChannel << "] seek error " << endl;
		}
		m_seek_pts = tm * AV_TIME_BASE / m_timeBase.den;
		//m_start_pts = high_resolution_clock::now();
	}
}

bool CDemuxer::SeekPTS(uint64_t pts)
{
	uint64_t num = m_info["num"].asInt();
	uint64_t den = m_info["den"].asInt();

	int ret = 0;
	if (m_nChannel < 4)
	{
		//ret = avformat_seek_file(fmt_ctx, 0, 0, tm, tm, 0);
		//avcodec_flush_buffers(fmt_ctx->streams[0]->codec);
		ret = avformat_seek_file(fmt_ctx, 0, 0, pts, pts, AVSEEK_FLAG_FRAME);
		if (ret > -1)
		{
			_d("[DEMUXER:SeekPTS.ch%d] ret : %d , pts (%lld) seek completed\n", m_nChannel, ret, pts);
		}
		else
		{
			cout << "[DEMUXER.SeekPTS.ch" << m_nChannel << "] seek error " << endl;
		}
	}
}

bool CDemuxer::SyncNReset()
{
	// 버퍼를 비움
	if (m_queue)
	{
		m_queue->Clear();
	}
	// 싱크를 맞춤
}

int CDemuxer::AudioSeek(uint64_t audioCount)
{
	return 0;
}

bool CDemuxer::Reverse()
{
	uint64_t num = m_info["num"].asUInt64();
	uint64_t den = m_info["den"].asUInt64();
	//avcodec_flush_buffers(fmt_ctx->streams[0]->codec);
	uint64_t pts_diff = 0;
	int ret = 0;
	AVStream *pStream = fmt_ctx->streams[0];
	AVRational timeBase = pStream->time_base;

	pts_diff = (num * AV_TIME_BASE) / den;
	if (m_nChannel < 6)
	{
		//m_reverse_pts = m_reverse_pts - ((timeBase.den / den) * num);
		//m_reverse_pts = m_reverse_pts - ((timeBase.den / den) * num);
		ret = avformat_seek_file(fmt_ctx, 0, 0, m_reverse_pts, m_reverse_pts, AVSEEK_FLAG_BACKWARD);
		if (ret < 0)
		{
			_d("[DEMUXER:REVERSE.ch%d] ret : %d, not work, first(%lld), (m_reverse_pts : %lld)\n", m_nChannel, ret, m_file_first_pts, m_reverse_pts);
			return false;
		}
		else
		{
			_d("[DEMUXER:REVERSE.ch%d] ret : %d , tm(%lld) seek completed\n", m_nChannel, ret, m_reverse_pts);
		}
#if 1
		if (m_file_first_pts > ((m_reverse_pts / timeBase.den) * AV_TIME_BASE))
		{
			_d("[DEMUXER:REVERSE.ch%d] file first arrived, file_first_pts : %lld, reverse_seek pts : %lld), (%lld)\n", m_nChannel, m_file_first_pts, ((m_reverse_pts / timeBase.den) * AV_TIME_BASE), m_reverse_pts);
			return false;
		}
#endif
	}

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
	cout << "[DEMUXER.ch" << m_nChannel << "] Set speed : " << m_nSpeed << endl;
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

	cout << "[DEMUXER.ch" << m_nChannel << "] Set Pause : " << m_IsPause << endl;
}

void CDemuxer::SetReverse(bool state)
{
	m_bIsRerverse = state;
	cout << "[DEMUXER.ch" << m_nChannel << "] Set Reverse : " << m_bIsRerverse << endl;
	if (m_CSender)
	{
		m_CSender->SetReverse(m_bIsRerverse);
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