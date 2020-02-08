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

	vector<string> v;

	//output이라는 이름으로 하드코딩함
	string except_output = "output/";

	string srcfile = "deletelist.txt";
	string undelete_file;

	ifstream ifs_d;
	ifs_d.open(srcfile);

	if (ifs_d.is_open())
	{
		while (getline(ifs_d, undelete_file))
		{
			v.push_back(undelete_file);
		}
	}
	ifs_d.close();

	bool is_continue = false;

	DIR *dir = opendir(basepath.c_str());
	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL)
	{
		if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
		{
			if (ent->d_type == DT_DIR)
			{
				stringstream sstm;
				sstm << basepath << "/" << ent->d_name;

				cout << "v : " << v.size() << endl;

				for (vector<string>::iterator iter = v.begin(); iter != v.end(); iter++)
				{
					cout << *iter << ", " << sstm.str() << endl;
					if (*iter == sstm.str())
					{
						cout << "already delete list file" << endl;
						is_continue = true;
						break;
					}
				}

				if (is_continue == true)
				{
					continue;
				}

				b["idx"] = idx;
				b["path"] = ent->d_name;
				sstm.str("");

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

static int rmdir_helper(const char *path, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
	string srcfile = "deletelist.txt";

	ofstream ofs;
	ofs.open(srcfile);
	switch (tflag)
	{
	case FTW_D:
	case FTW_DP:
		if (rmdir(path) == -1)
		{
			perror("rmdir");
			//cout << path << " not rmdir" << endl;
			ofs << path << endl;
		}
		else
		{
			cout << "rmdir : " << path << endl;
		}
		break;
	case FTW_F:
	case FTW_SL:
		if (unlink(path) == -1)
		{
			//unlink 시 에러
			perror("unlink");
			cout << path << " not unlink" << endl;
			ofs << path << endl;
		}
		else
		{
			cout << "unlink : " << path << endl;
		}

		break;
	default:
		cout << "[rmhelper] do nothing" << endl;
	}

	ofs.close();

	return 0;
}

int rmdir_rf(string dir_to_remove)
{
	int flags = 0;
	if (dir_to_remove.c_str() == NULL)
	{
		return 1;
	}

	flags |= FTW_DEPTH;

	if (nftw(dir_to_remove.c_str(), rmdir_helper, 10, flags) == -1)
	{
		perror("nftw");
		return 1;
	}
	return 0;
}

void delete_line(const char *file_name, int n)
{
	// open file in read mode or in mode
	ifstream is(file_name);

	// open file in write mode or out mode
	ofstream ofs;
	ofs.open("temp.txt", ofstream::out);

	// loop getting single characters
	char c;
	int line_no = 1;
	while (is.get(c))
	{
		// if a newline character
		if (c == '\n')
			line_no++;

		// file content not to be deleted
		if (line_no != n)
			ofs << c;
	}

	// closing output file
	ofs.close();

	// closing input file
	is.close();

	// remove the original file
	remove(file_name);

	// rename the file
	rename("temp.txt", file_name);
	cout << "delete_line completed" << endl;
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
