#include "main.h"
#include "queue.h"

CQueue::CQueue(int nMaxSize)
{
	m_nMaxQueue = nMaxSize;
	m_nMaxAudioQueue = MAX_NUM_AUDIO_QUEUE;

	m_nSizeQueue = 0;

	m_bEnable = false;
	m_bExit = false;
	m_nPacket = 0;
	m_nAudio = 0;

	m_nReadPos = 0;
	m_nWritePos = 0;

	m_nReadAudioPos = 0;
	m_nWriteAudioPos = 0;

	m_seek_pts = 0;
	m_current_video_pts = 0;

	m_audio_status = 0;

	pthread_mutex_init(&m_mutex, NULL);

	for (int i = 0; i < m_nMaxQueue; i++)
	{
		av_init_packet(&m_pkt[i]);
		m_pkt[i].size = 0;
		m_pkt[i].data = NULL;
	}

	for (int i = 0; i < m_nMaxAudioQueue; i++)
	{
		m_e[i].p = nullptr;
		m_e[i].len = 0;
	}
}

CQueue::~CQueue()
{
	Clear();
	pthread_mutex_destroy(&m_mutex);
	_d("[QUEUE.ch%d] mutex destoryed(%x)\n", m_nChannel, &m_mutex);
}

void CQueue::SetInfo(int nChannel, string type)
{
	m_nChannel = nChannel;
	m_type = type;
}

void CQueue::Clear()
{
	//m_bExit = true;
	for (int i = 0; i < m_nMaxQueue; i++)
	{
		//cout << "[QUEUE.ch" << m_nChannel << "] " << i << ", pts : " << m_pkt[i].pts << endl;
		av_packet_unref(&m_pkt[i]);
	}

	for (int i = 0; i < m_nMaxAudioQueue; i++)
	{
		if (m_e[i].len > 0)
		{
			if (m_e[i].p)
			{
				delete m_e[i].p;
				m_e[i].p = NULL;
				m_e[i].len = 0;
			}
		}
	}

	m_nReadPos = 0;
	m_nWritePos = 0;

	m_nWriteAudioPos = 0;
	m_nReadAudioPos = 0;

	m_bEnable = false;
	m_nPacket = 0;
	m_nAudio = 0;

	cout << "[QUEUE.ch" << m_nChannel << "] Queue Clear()" << endl;
}

void CQueue::Enable()
{
	m_bEnable = true;

	//m_nReadPos = m_nWritePos;
	//_d("[QUEUE] Enable packet outputing now...%d\n", m_nPacket);
	//cout << "[QUEUE.ch" << m_nChannel << "] Enable packet outputing now... " << m_nPacket << endl;
}

void CQueue::Disable()
{
	m_bEnable = false;
	//cout << "[QUEUE.ch" << m_nChannel << "] diabled now... " << m_nPacket << endl;
}

int CQueue::PutVideo(AVPacket *pkt, char isvisible)
{
	//int nCount = 0; // timeout 위한 용도
	int ret_size = 0;

	if (m_nMaxQueue <= m_nPacket)
	{
		//cout << "[QUEUE.ch" << m_nChannel << "] wait video put : " << m_nWritePos << endl;
		//this_thread::sleep_for(microseconds(1000));
		return 0;
	}

	pthread_mutex_lock(&m_mutex);
	if (pkt->size > 0)
	{
		//av_init_packet(&m_pkt[m_nWritePos]);
		av_packet_ref(&m_pkt[m_nWritePos], pkt);
		//av_packet_unref(pkt);
		m_ve[m_nWritePos].pkt = &m_pkt[m_nWritePos];
		m_ve[m_nWritePos].isvisible = isvisible;
#if 0
		_d("[QUEUE.ch%d] put pos : ( %d ), stream_index : %d, flags : %d, pts ( %d ), visible(%d), (%x)\n", m_nChannel, m_nWritePos, pkt->stream_index, m_pkt[m_nWritePos].flags, m_ve[m_nWritePos].pkt->pts, m_ve[m_nWritePos].isvisible, &m_ve[m_nWritePos].isvisible);
#endif
		ret_size = m_pkt[m_nWritePos].size;
		m_nWritePos++;
		if (m_nWritePos >= m_nMaxQueue)
		{
			m_nWritePos = 0;
		}
		m_nPacket++;
		pthread_mutex_unlock(&m_mutex);

		if (m_nWritePos > 0)
		{
			m_bEnable = true;
		}
		return ret_size;
	}
	pthread_mutex_unlock(&m_mutex);
	//usleep(10);
	return 0;
}

int CQueue::PutAudio(char *pData, int nSize, char status)
{
	int nCount = 0;
	if (m_nMaxAudioQueue <= m_nAudio)
	{
		//cout << "[QUEUE.ch" << m_nChannel << "] wait audio put : " << m_nWritePos << endl;
		//this_thread::sleep_for(microseconds(1000));
		return 0;
	}
	ELEM *pe = &m_e[m_nWriteAudioPos];
	pe->p = new char[nSize];
	pthread_mutex_lock(&m_mutex);
	if (pe->len == 0)
	{
		memcpy(pe->p, pData, nSize);
		pe->len = nSize;
		pe->state = status;
		m_nWriteAudioPos++;
		if (m_nWriteAudioPos >= m_nMaxAudioQueue)
		{
			m_nWriteAudioPos = 0;
		}
		m_nAudio++;
		m_audio_status = status;
#if 0
		if (m_nAudio > 0)
		{
			m_bEnable = true;
		}
#endif
#if 0
		cout << "[QUEUE.ch" << m_nChannel << "] m_nWriteAudioPos : " << m_nWriteAudioPos << ", size : " << pe->len << ", nAudio : " << m_nAudio << endl;
#endif
		pthread_mutex_unlock(&m_mutex);
		return pe->len;
	}
	pthread_mutex_unlock(&m_mutex);
	//usleep(10);
	return 0;
}

int CQueue::GetVideo(AVPacket *pkt, char *isvisible)
{
#if 1
	if (m_bEnable == false)
	{
		//cout << "[QUEUE.ch" << m_nChannel << "] m_nReadPos : " << m_nReadPos << ", size : " << m_pkt[m_nReadPos].size << endl;
		return NULL;
	}
#endif

	pthread_mutex_lock(&m_mutex);

	//if (m_pkt[m_nReadPos].size > 0)
	if (m_ve[m_nReadPos].pkt->size > 0)
	{
		av_init_packet(pkt);
		av_packet_ref(pkt, &m_pkt[m_nReadPos]);
		av_packet_unref(&m_pkt[m_nReadPos]);
		m_current_video_pts = pkt->pts;
		*isvisible = m_ve[m_nReadPos].isvisible;
#if 0
		cout << "[QUEUE.ch" << m_nChannel << "] get pos (" << m_nReadPos << "), size (" << pkt->size << "), pts (" << pkt->pts << ") m_nPacket : " << m_nPacket << ", visible : " << (int)*isvisible << endl;
#endif
		pthread_mutex_unlock(&m_mutex);

		return pkt->size;
	}
	else
	{
		pthread_mutex_unlock(&m_mutex);
		return 0;
	}
	//pthread_mutex_unlock(&m_mutex);
	//usleep(5);
}

void *CQueue::GetAudio()
{
#if 1
	if (m_bEnable == false)
	{
		return NULL;
	}
#endif
	ELEM *pe = &m_e[m_nReadAudioPos];
	while (pe->len > 0)
	{
		//cout << "[QUEUE.ch" << m_nChannel << "] m_nReadAudioPos : " << m_nReadAudioPos << ", pe->len : " << pe->len << endl;
		pthread_mutex_lock(&m_mutex);
		if (pe->len)
		{
			pthread_mutex_unlock(&m_mutex);
			return pe;
		}
		pthread_mutex_unlock(&m_mutex);
		//usleep(10);
	}
	return 0;
}

uint64_t CQueue::GetCurrentVideoPTS()
{
	//cout << "[QUEUE.ch" << m_nChannel << "] current pts : " << m_current_video_pts << endl;
	return m_current_video_pts;
}

void CQueue::RetAudio(void *p)
{
	ELEM *pe = (ELEM *)p;
	pthread_mutex_lock(&m_mutex);

	pe->len = 0;
	m_nReadAudioPos++;
	if (m_nReadAudioPos >= m_nMaxAudioQueue)
	{
		m_nReadAudioPos = 0;
	}
	m_nAudio--;
	if (pe->p)
	{
		delete pe->p;
	}
	pthread_mutex_unlock(&m_mutex);
}

void CQueue::RetVideo(AVPacket *pkt)
{
	if (this == NULL)
	{
		return;
	}

	pthread_mutex_lock(&m_mutex);

	av_packet_unref(pkt);
	//av_packet_unref(&m_pkt[m_nReadPos]);

	m_nReadPos++;
	if (m_nReadPos >= m_nMaxQueue)
	{
		m_nReadPos = 0;
	}
	m_nPacket--;
	pthread_mutex_unlock(&m_mutex);
}

bool CQueue::Exit()
{
	m_bExit = true;
}
