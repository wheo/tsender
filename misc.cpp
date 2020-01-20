#include "main.h"
#include "misc.h"

Json::Value GetOutputFileList(string basepath)
{
	Json::Reader reader;
	Json::Reader sub_reader;
	Json::Value root;
	Json::Value info;
	Json::Value meta;
	Json::Value b;
	Json::Value channel;
	int idx = 0;
	string infopath;
	string metabase;
	string metapath;
	DIR *dir = opendir(basepath.c_str());
	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL)
	{
		if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
		{
			if (ent->d_type == DT_DIR)
			{
				b["idx"] = idx;
				b["path"] = ent->d_name;
#if 1
				stringstream sstm;

				sstm << basepath << "/" << ent->d_name << "/"
					 << "info.json";
				infopath = sstm.str();

				cout << "[MISC] file open : " << infopath << endl;

				sstm.str("");

				sstm << basepath << "/" << ent->d_name << "/";

				string metabase = sstm.str();
				sstm.str();
				sstm.str("");

				ifstream ifs(infopath, ifstream::binary);
				if (!reader.parse(ifs, info, true))
				{
					ifs.close();
					cout << "[MISC] Failed to parse info.json" << endl
						 << reader.getFormatedErrorMessages() << endl;
				}
				else
				{
					ifs.close();
					b["bit_state"] = info["info"]["bit_state"];
				}
#endif

				DIR *sub_dir = opendir(metabase.c_str());
				struct dirent *sub_ent;
				while ((sub_ent = readdir(sub_dir)) != NULL)
				{
					int sub_idx = 0;
					if (strcmp(sub_ent->d_name, ".") != 0 && strcmp(sub_ent->d_name, "..") != 0)
					{
						if (sub_ent->d_type == DT_DIR)
						{
							uint64_t total_frame = 0;
							sstm << metabase << sub_ent->d_name << "/"
								 << "meta.json";
							metapath = sstm.str();
							sstm.str("");

							//cout << "[MISC] file open : " << metapath << endl;

							ifstream ifs(metapath, ifstream::binary);
							if (!sub_reader.parse(ifs, meta, true))
							{
								ifs.close();
								cout << "[MISC] Failed to parse " << metapath << endl
									 << reader.getFormatedErrorMessages() << endl;
							}
							else
							{
								ifs.close();
								//b["bit_state"] = info["info"]["bit_state"];
								for (int i = 0; i < meta["files"].size(); i++)
								{
									total_frame += meta["files"][i]["frame"].asInt64();
								}
								channel["total_frame"] = total_frame;
								channel["filename"] = metapath;
								channel["idx"] = sub_ent->d_name;
								channel["num"] = meta["num"];
								channel["den"] = meta["den"];
								//channel["idx"] = sub_idx;
								total_frame = 0;
								//sub_idx++;
							}
							b["channels"].append(channel);
							channel.clear();
						}
					}
				}
				root["files"].append(b);
				b.clear();
				closedir(sub_dir);
			}
			idx++;
		}
	}
	closedir(dir);
	return root;
}

bool GetOutputCheck(string basepath, string path)
{
	DIR *dir = opendir(basepath.c_str());
	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL)
	{
		if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
		{
			if (strcmp(ent->d_name, path.c_str()) != 0)
			{
				//일치
				closedir(dir);
				cout << "[일치] : " << path << endl;
				return true;
			}
		}
	}
	closedir(dir);
	return false;
}

bool IsDirExist(string path)
{
	struct stat st;
	if (stat(path.c_str(), &st) == 0)
	{
		// is present
		return true;
	}
	else
	{
		return false;
	}
}

std::string get_current_time_and_date()
{
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	std::stringstream ss;
	//ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d%H%M%S");
	ss << tm.tm_year + 1900 << tm.tm_mon + 1 << tm.tm_mday << tm.tm_hour << tm.tm_min << tm.tm_sec;

	return ss.str();
}

std::ifstream::pos_type getFilesize(const char *filename)
{
	std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
	return in.tellg();
}

double rnd(double x, int digit)
{
	return (floor((x)*pow(float(10), digit) + 0.5f) / pow(float(10), digit));
}

unsigned long GetTickCount()
{
	timeval tv;
	static time_t sec = timeStart.tv_.tv_sec;
	static time_t usec = timeStart.tv_.tv_usec;

	gettimeofday(&tv, NULL);
	return (tv.tv_sec - sec) * 1000 + (tv.tv_usec - usec) / 1000;
}

PThread::PThread(char *a_szName, THREAD_EXIT_STATE a_eExitType)
{
	m_nID = 0;
	m_eState = eREADY;
	m_eExitType = a_eExitType;
	m_szName = NULL;
	m_bExit = false;
	if (a_szName)
	{
		int nLength = strlen(a_szName);
		m_szName = new char[nLength + 1];
		strcpy(m_szName, a_szName);
	}
}

PThread::~PThread()
{
	if (m_eState == eZOMBIE)
	{
		Join(m_nID);
	}

	if (m_szName)
		delete[] m_szName;
}

int PThread::Start()
{
	int nResult = 0;
	m_bExit = false;
	if ((pthread_create(&m_nID, NULL, StartPoint, reinterpret_cast<void *>(this))) == 0)
	{
		//_d("[MISC] Thread Create : %d, m_eExitType : %d\n", m_nID, m_eExitType);
		if (m_eExitType == eDETACHABLE)
		{
			pthread_detach(m_nID);
		}
	}
	else
	{
		SetState(eABORTED);
		nResult = -1;
	}

	return nResult;
}

void *PThread::StartPoint(void *a_pParam)
{
	PThread *pThis = reinterpret_cast<PThread *>(a_pParam);
	pThis->SetState(eRUNNING);

	pThis->Run();

	if (pThis->GetExitType() == eDETACHABLE)
	{
		pThis->SetState(eTERMINATED);
	}
	else
	{
		pThis->SetState(eZOMBIE);
	}

	pthread_exit((void *)NULL);
	return NULL;
}

void PThread::Terminate()
{
	//_d("[MISC] m_eState : %d\n", m_eState);
	if (m_eState == eRUNNING)
	{
		m_bExit = true;
		if (m_eExitType == eJOINABLE)
		{
			OnTerminate();
			Join(m_nID);
		}
		else if (m_eExitType == eDETACHABLE)
		{
			OnTerminate();
		}
	}
	else if (m_eState == eZOMBIE)
	{
		Join(m_nID);
	}
}

// this function will be blocked until the StartPoint terminates
void PThread::Join(pthread_t a_nID)
{
	//_d("[MISC] Try to join : %d\n", a_nID);
	if (!pthread_join(a_nID, NULL))
	{
		SetState(eTERMINATED);
	}
	else
	{
		printf("Failed to join thread [%s: %d]\n", __FUNCTION__, __LINE__);
	}
}

void PThread::SetState(THREAD_STATE a_eState)
{
	m_eState = a_eState;
}

THREAD_STATE PThread::GetState() const
{
	return m_eState;
}

THREAD_EXIT_STATE PThread::GetExitType() const
{
	return m_eExitType;
}

char *PThread::GetName() const
{
	return m_szName ? m_szName : NULL;
}

bool PThread::IsTerminated() const
{
	return m_eState == eTERMINATED ? true : false;
}

bool PThread::IsRunning() const
{
	return m_eState == eRUNNING ? true : false;
}
