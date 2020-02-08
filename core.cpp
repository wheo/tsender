#include "main.h"
#include "core.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

using namespace std;

CCore::CCore(void)
{
	m_bExit = false;
	pthread_mutex_init(&m_mutex_core, 0);
}

CCore::~CCore(void)
{
	m_bExit = true;
	pthread_mutex_destroy(&m_mutex_core);
	_d("[CORE] Trying to exit thread\n");
	Delete();
	cout << "[CORE] Before Terminate()" << endl;
	Terminate();
	cout << "[CORE] has been exited..." << endl;
}

bool CCore::Create()
{
	m_nChannel = 0;

	ifstream ifs("./setting.json", ifstream::binary);

	if (!ifs.is_open())
	{
		_d("\n ******************************************* \n setting.json file is not found\n Put in setting.json file in your directory\n ******************************************* \n\n");
	}
	else
	{
		//check done
		//ifs >> m_root;
		if (!m_reader.parse(ifs, m_root, true))
		{
			ifs.close();
			_d("Failed to parse setting.json configuration\n%s\n", m_reader.getFormatedErrorMessages().c_str());
			_d("[CORE.ch%d] Exit code\n");
			exit(EXIT_FAILURE);
		}
		else
		{
			ifs.close();

			Json::Value attr;
			attr = m_root;
#if 0
			attr["version"] = m_root["version"];
			attr["file_dst"] = m_root["file_dst"];
#endif
			// file_dst 디렉토리 없으면 생성
			stringstream sstm;
			sstm << "mkdir -p " << m_root["file_dst"].asString();
			system(sstm.str().c_str());

			cout << "[CORE] version : " << attr["version"].asString() << endl;
			cout << "[CORE] file_dst : " << attr["file_dst"].asString() << endl;

			UndeleteFileDelete();

			//make core thread
			//cout << "[CORE] Before Thread start" << endl;
			Start();
			//usleep(100000);
			m_comm = new CCommMgr();
			if (!m_comm->Open(attr["udp_sender_port"].asInt(), attr))
			{
				cout << "[CORE] udp_sender_port is not exist" << endl;
				exit(EXIT_FAILURE);
			}
			ifstream ifs_init("./init.json", ifstream::binary);
			if (!ifs_init.is_open())
			{
				// do notthing
			}
			else
			{
				Json::Reader reader;
				if (!reader.parse(ifs_init, m_init, true))
				{
					ifs_init.close();
					_d("Failed to parse setting.json configuration\n%s\n", reader.getFormatedErrorMessages().c_str());
				}
				else
				{
					ifs_init.close();
				}
			}
		}
	}

	return true;
}

void CCore::UndeleteFileDelete()
{
	cout << "[CORE] deletelist read " << endl;

	vector<string> v;

	string undelete_file;
	string deleteline;
	string srcfile = "deletelist.txt";

	ifstream ifs_d;
	ifs_d.open(srcfile);

	if (ifs_d.is_open())
	{
		while (getline(ifs_d, undelete_file))
		{
			cout << "deletelist : " << undelete_file << endl;
			rmdir_rf(undelete_file);
#if 0
			if (unlink(undelete_file.c_str()) == -1)
			
			{
				cout << "[CORE] delete error : " << undelete_file << endl;
				v.push_back(undelete_file);
			}
			else
			{
				cout << "[CORE] delete completed : " << undelete_file << endl;
			}
#endif
		}
	}
	ifs_d.close();
	unlink(srcfile.c_str());

	ofstream ofs(srcfile.data());
	if (ofs.is_open())
	{
		for (vector<string>::iterator iter = v.begin(); iter != v.end(); iter++)
		{
			ofs << *iter << endl;
		}
	}
	ofs.close();
}

void CCore::Run()
{
	while (!m_bExit)
	{
#if __DEBUG
		cout << "[CORE] Thread is alive" << endl;
#endif
		this_thread::sleep_for(milliseconds(1000));
	}
}

#if 0
bool CCore::GetOutputs(string basepath) {
	string path;
	DIR *dir = opendir(basepath.c_str());
	struct dirent *ent;
	
	while ((ent = readdir(dir)) != NULL) {
		//cout << "d_name : " << ent->d_name << endl;
		if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
			if ( ent->d_type == DT_DIR) {
				path.clear();
				path.append(basepath);
				path.append("/");
				path.append(ent->d_name);
				//cout << "path : " << path;
				GetOutputs(path);
			} else {
				cout << basepath << "/" << ent->d_name << endl;
			}
		}
	}
	closedir (dir);
	return EXIT_SUCCESS;
}
#endif

void CCore::Delete()
{
	SAFE_DELETE(m_comm);
}
