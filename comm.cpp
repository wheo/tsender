#include "comm.h"

CCommMgr::CCommMgr()
{
	m_bExit = false;
	pthread_mutex_init(&m_mutex_comm, 0);
}

CCommMgr::~CCommMgr()
{
	m_bExit = true;
	pthread_mutex_destroy(&m_mutex_comm);
	cout << "[COMM] Trying to exit thread" << endl;
	Delete();
	if (m_sdRecv > 0)
	{
		close(m_sdRecv);
		cout << "[COMM] RECV socket(" << m_sdRecv << ") has been closed" << endl;
	}
	if (m_sdSend > 0)
	{
		close(m_sdSend);
		cout << "[COMM] SEND socket(" << m_sdSend << ") has been closed" << endl;
	}
	//cout << "[COMM] before Terminate()" << endl;
	Terminate();
	cout << "[COMM] Exited..." << endl;
}

bool CCommMgr::SetSocket()
{
	int t = 1;
	struct sockaddr_in sin;

	m_sdRecv = socket(PF_INET, SOCK_DGRAM, 0);
	if (m_sdRecv < 0)
	{
		cout << "[COMM] Failed to open rx socket" << endl;
		return false;
	}

	m_sdSend = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (m_sdSend < 0)
	{
		cout << "[COMM] failed to open tx socket" << endl;
		return false;
	}

	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(m_nPort);

	setsockopt(m_sdRecv, SOL_SOCKET, SO_REUSEADDR, (const char *)&t, sizeof(t));

	if (bind(m_sdRecv, (const sockaddr *)&sin, sizeof(sin)) < 0)
	{
		cout << "[COMM] Failed to bind to rx port : " << sin.sin_port << endl;
		return false;
	}

	struct timeval read_timeout;
	read_timeout.tv_sec = 1;
	read_timeout.tv_usec = 0;
	if (setsockopt(m_sdRecv, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0)
	{
		cout << "[COMM] set timeout error" << endl;
		return false;
	}

	return true;
}

bool CCommMgr::Open(int nPort, Json::Value attr)
{
	m_nChannel = 0;
	m_nSpeed = 1;
	m_bIsRunning = false;
	m_bIsRerverse = false;
	m_nPort = nPort;
	m_attr = attr;

	if (!SetSocket())
	{
		cout << "[COMM] SetSocket is failed" << endl;
		return false;
	}
	//cout << "[COMM] Before Thread Start" << endl;
	Start();

	return true;
}

void CCommMgr::Run()
{
	RX();
}

bool CCommMgr::RX()
{
	char buff[READ_SIZE];
	int rd = 0;

	struct sockaddr_in sin;

	socklen_t sin_size = sizeof(sin);

	cout << "[COMM] udp RX ready (" << m_nPort << ")" << endl;

	Json::Reader reader;
	Json::Value root;
	stringstream sstm;

	while (!m_bExit)
	{
		memset(buff, 0x00, sizeof(buff));
		rd = recvfrom(m_sdRecv, buff, sizeof(buff), 0, (struct sockaddr *)&sin, &sin_size);
		if (rd < 1)
		{
			usleep(10);
			continue;
		}
		cout << "[COMM] recv size : " << rd << endl;
		//수신 받은 sin 구조체를 전역으로 설정
		m_sin = sin;

		//send echo
		TX(buff, rd);

		string strbuf;
		strbuf = (char *)buff;

		if (reader.parse(strbuf, root, true))
		{
			//parse success
			if (root["cmd"] == "play_start")
			{
				//cout << root["info"]["target"].asString() << endl;

				m_attr["target"] = root["info"]["target"].asString();
				m_attr["bit_state"] = root["info"]["bit_state"];
				if (!m_bIsRunning)
				{
					m_bIsRunning = true;
					m_nChannel = 0;
					for (auto &value : m_attr["output_channels"])
					{
						m_CDemuxer[m_nChannel] = new CDemuxer();
						m_CDemuxer[m_nChannel]->SetMutex(&m_mutex_comm);
						m_CDemuxer[m_nChannel]->Create(m_attr["output_channels"][m_nChannel], m_attr, m_nChannel);
						m_nChannel++;
					}
				}
				else
				{
					cout << "[COMM] is running" << endl;
					for (int i = 0; i < m_nChannel; i++)
					{
						if (m_CDemuxer[i])
						{
							m_CDemuxer[i]->SetPause(false);
							m_CDemuxer[i]->SetReverse(false);
							m_CDemuxer[i]->SetSpeed(1);
						}
					}
				}
			}
			else if (root["cmd"] == "play_reverse")
			{
#if 0
                // 역재생 흑역사!!!! 이렇게 만들면 안된다!!!!
                if (m_bIsRunning)
                {
                    for (int i = 0; i < m_nChannel; i++)
                    {
                        Delete();
                    }
                }
                m_bIsRunning = true;
                m_bIsRerverse = true;
                m_nChannel = 0;
                for (auto &value : m_attr["output_channels"])
                {
                    m_CDemuxer[m_nChannel] = new CDemuxer();
                    m_CDemuxer[m_nChannel]->SetMutex(&m_mutex_comm);
                    m_CDemuxer[m_nChannel]->Create(m_attr["output_channels"][m_nChannel], m_attr, m_nChannel);
                    m_nChannel++;
                }
#endif
				if (m_bIsRunning)
				{
					for (int i = 0; i < m_nChannel; i++)
					{
						m_bIsRerverse = m_CDemuxer[i]->SetReverse();
					}

#if 0
					if (m_bIsRerverse == false)
					{
						cout << "[COMM] Set Reverse : " << m_bIsRerverse << endl;
						uint64_t max_pts = 0;
						for (int i = 0; i < m_nChannel; i++)
						{
							cout << "[COMM] Channel : " << i << ", PTS : " << m_CDemuxer[i]->GetCurrentPTS() << endl;
							if (max_pts < m_CDemuxer[i]->GetCurrentPTS())
							{
								max_pts = m_CDemuxer[i]->GetCurrentPTS();
							}
						}
						cout << "[COMM] sync pts : " << max_pts << endl;
						for (int i = 0; i < 3; i++)
						{
							m_CDemuxer[i]->SetSyncPTS(max_pts);
						}
					}
#endif
				}
				else
				{
					cout << "[COMM] (" << root["cmd"].asString() << ") player is not running" << endl;
				}
			}
			else if (root["cmd"] == "play_close")
			{
				if (m_bIsRunning)
				{
					Delete();
					cout << "[COMM] sender is deleted" << endl;
					m_nChannel = 0;
					m_bIsRunning = false;
				}
				else
				{
					cout << "[COMM] (" << root["cmd"].asString() << ") player is not running" << endl;
				}
			}
			else if (root["cmd"] == "play_speed_up")
			{
				//배속 재생
				if (m_bIsRunning)
				{
					//실행 중이어야 배속 재생이 됨
					m_nSpeed = root["info"]["speed"].asInt();
					for (int i = 0; i < m_nChannel; i++)
					{
						m_CDemuxer[i]->SetSpeed(m_nSpeed);
					}

#if 0 

					uint64_t max_pts = 0;
					for (int i = 0; i < m_nChannel; i++)
					{
						cout << "[COMM] Channel : " << i << ", PTS : " << m_CDemuxer[i]->GetCurrentPTS() << endl;
						if (max_pts < m_CDemuxer[i]->GetCurrentPTS())
						{
							max_pts = m_CDemuxer[i]->GetCurrentPTS();
						}
					}
					cout << "[COMM] sync pts : " << max_pts << endl;
					for (int i = 0; i < 4; i++)
					{
						m_CDemuxer[i]->SetSyncPTS(max_pts);
					}
#endif
				}
				else
				{
					cout << "[COMM] (" << root["cmd"].asString() << ") player is not running" << endl;
				}
			}
			else if (root["cmd"] == "play_pause")
			{
				//멈춤
				if (m_bIsRunning)
				{
					uint64_t max_pts = 0;
					for (int i = 0; i < m_nChannel; i++)
					{
						cout << "[COMM] Channel : " << i << ", PTS : " << m_CDemuxer[i]->GetCurrentPTS() << endl;
						if (max_pts < m_CDemuxer[i]->GetCurrentPTS())
						{
							max_pts = m_CDemuxer[i]->GetCurrentPTS();
						}
					}
					cout << "[COMM] sync pts : " << max_pts << endl;
					for (int i = 0; i < m_nChannel; i++)
					{
						m_CDemuxer[i]->SetPause();
					}
					for (int i = 0; i < 4; i++)
					{
						m_CDemuxer[i]->SetSyncPTS(max_pts);
					}
				}
				else
				{
					cout << "[COMM] (" << root["cmd"].asString() << ") player is not running" << endl;
				}
			}
			else if (root["cmd"] == "play_seek")
			{
				if (m_bIsRunning)
				{
					m_nMoveSec = root["info"]["move_sec"].asInt();
					cout << "[COMM] Move sec : " << m_nMoveSec << endl;
					for (int i = 0; i < m_nChannel; i++)
					{
						m_CDemuxer[i]->SetMoveSec(m_nMoveSec);
					}
#if 0
					usleep(100000);

					uint64_t max_pts = 0;
					for (int i = 0; i < 4; i++)
					{
						cout << "[COMM] Channel : " << i << ", PTS : " << m_CDemuxer[i]->GetCurrentPTS() << endl;
						if (max_pts < m_CDemuxer[i]->GetCurrentPTS())
						{
							max_pts = m_CDemuxer[i]->GetCurrentPTS();
						}
					}
					cout << "[COMM] sync pts : " << max_pts << endl;
					for (int i = 0; i < 4; i++)
					{
						m_CDemuxer[i]->SetSyncPTS(max_pts);
					}
#endif
				}
				else
				{
					cout << "[COMM] (" << root["cmd"].asString() << ") player is not running" << endl;
				}
			}
			else if (root["cmd"] == "get_thumbnail")
			{
				Json::Value ret_json;
				if (m_bIsRunning)
				{
					cout << "[COMM] Set Reverse" << m_nSpeed << endl;
					int channel = root["info"]["channel"].asInt();
					int sec = root["info"]["sec"].asInt();
					ret_json = m_CDemuxer[channel]->GetThumbnail(sec);
				}
				else
				{
					cout << "[COMM] (" << root["cmd"].asString() << ") player is not running" << endl;
				}

				ret_json["cmd"] = "get_thumbnail";
				string strbuf;
				Json::StreamWriterBuilder builder;
				builder["commentStyle"] = "None";
				builder["indentation"] = "";
				strbuf = Json::writeString(builder, ret_json);
				cout << strbuf << ", len : " << strlen(strbuf.c_str()) << endl;
				TX((char *)strbuf.c_str(), strlen(strbuf.c_str()));
			}
			else if (root["cmd"] == "get_play_list")
			{
				//재생 list 제공
				string strbuf;
				root = GetOutputFileList(m_attr["file_dst"].asString());
				root["cmd"] = "get_play_list";

				Json::StreamWriterBuilder builder;
				builder["commentStyle"] = "None";
				builder["indentation"] = "";
				//std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
				strbuf = Json::writeString(builder, root);
				//cout << strbuf << ", len : " << strlen(strbuf.c_str()) << endl;
				TX((char *)strbuf.c_str(), strlen(strbuf.c_str()));
			}
			else if (root["cmd"] == "play_delete")
			{
				// target 삭제
				string strbuf;
				string target;
				sstm.str("");
				sstm << m_attr["file_dst"].asString() << "/" << root["info"]["target"].asString();
				target = sstm.str();
				cout << "[COMM] delete target : " << target << endl;
				sstm.str("");
				sstm << "rm -rf " << target;
				cout << sstm.str() << endl;
				if (target.length() > 0)
				{
					//첫 문자가 / 면 안됨, 1글자여서도 안됨 (삭제 사고 방지)
					if (target.substr(0, 1).compare("/") != 0)
					{
						system(sstm.str().c_str()); // 실제 삭제
					}
				}
				root = GetOutputFileList(m_attr["file_dst"].asString());
				root["cmd"] = "get_play_delete"; // key
				Json::StreamWriterBuilder builder;
				builder["commentStyle"] = "None";
				builder["indentation"] = "";
				strbuf = Json::writeString(builder, root);
				TX((char *)strbuf.c_str(), strlen(strbuf.c_str()));
			}
		}
	}
	_d("[COMM] exit loop\n");
}

bool CCommMgr::TX(char *buff, int size)
{
	if (m_sdSend < 0)
	{
		cout << "[COMM] send socket not created" << endl;
		return false;
	}

	socklen_t sin_size = sizeof(m_sin);

#if 0
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(m_attr["udp_target_ip"].asString().c_str());
	sin.sin_port = htons(m_attr["udp_target_port"].asInt());
#endif

	m_sin.sin_port = htons(m_nPort);
	//cout << "[COMM] buff size : " << sizeof(buff) << endl;

	sendto(m_sdSend, buff, size, 0, (struct sockaddr *)&m_sin, sin_size);
	cout << "[COMM] ip : " << inet_ntoa(m_sin.sin_addr) << " port : " << htons(m_sin.sin_port) << ", send message(" << size << ") : " << buff << endl;
}

void CCommMgr::Delete()
{
	cout << "[COMM] open channel count : " << m_nChannel << endl;
	for (int i = 0; i < m_nChannel; i++)
	{
		SAFE_DELETE(m_CDemuxer[i]);
		cout << "[COMM] channel " << i << " has been deleted" << endl;
	}
}

CCommBase::CCommBase()
{
	m_sd = -1;
	m_bIsAlive = false;
}

CCommBase::~CCommBase()
{
	if (m_sd >= 0)
	{
		shutdown(m_sd, 2);
		close(m_sd);

		m_sd = -1;
		m_bIsAlive = false;
	}
}

int CCommBase::SR(char *pBuff, int nLen)
{
	int w = 0, local = nLen;

	while (local)
	{
		int r = recv(m_sd, &pBuff[w], local, 0);
		if (r > 0)
		{
			w += r;
			local -= r;
		}
		else
		{
			m_bIsAlive = false;
			return -1;
		}
	}

	return w;
}

int CCommBase::SS(char *pBuff, int nLen)
{
	int w = 0, local = nLen;

	while (local)
	{
		int s = send(m_sd, &pBuff[w], local, 0);
		if (s > 0)
		{
			w += s;
			local -= s;
		}
		else
		{
			m_bIsAlive = false;
			return -1;
		}
	}

	return w;
}
