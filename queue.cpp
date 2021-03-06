#include "main.h"
#include "queue.h"

CQueue::CQueue()
{
	m_nMaxQueue = MAX_NUM_QUEUE;
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

	m_start_pts = 0;

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

	cout << "[QUEUE.ch" << m_nChannel << "] Queue Clear" << endl;
}

void CQueue::Enable()
{
	m_bEnable = true;

	//m_nReadPos = m_nWritePos;
	//_d("[QUEUE] Enable packet outputing now...%d\n", m_nPacket);
	cout << "[QUEUE.ch" << m_nChannel << "] Enable packet outputing now... " << m_nPacket << endl;
}

void CQueue::Disable()
{
	m_bEnable = false;
	cout << "[QUEUE.ch" << m_nChannel << "] diabled now... " << m_nPacket << endl;
}

int CQueue::PutVideo(AVPacket *pkt, uint64_t start_pts)
{
	if (m_start_pts != start_pts)
	{
		Clear();
	}
	m_start_pts = start_pts;
	//int nCount = 0; // timeout 위한 용도
	int ret_size = 0;

	if (m_nMaxQueue <= m_nPacket)
	{
		//cout << "[QUEUE.ch" << m_nChannel << "] wait video put : " << m_nWritePos << endl;
		this_thread::sleep_for(microseconds(10000));
		return 0;
	}

	pthread_mutex_lock(&m_mutex);
	if (pkt->size > 0)
	{
		//av_init_packet(&m_pkt[m_nWritePos]);
		av_packet_ref(&m_pkt[m_nWritePos], pkt);
		//av_packet_unref(pkt);
#if 0
		_d("[QUEUE.ch%d] put pos : ( %d ), stream_index : %d, flags : %d, data ( %p ), pts ( %d )\n", m_nChannel, m_nWritePos, pkt->stream_index, m_pkt[m_nWritePos].flags, m_pkt[m_nWritePos].data, m_pkt[m_nWritePos].pts);
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
	usleep(10);
	return 0;
}

int CQueue::PutAudio(char *pData, int nSize, uint64_t start_pts)
{
	int nCount = 0;
	m_start_pts = start_pts;

	if (m_nMaxAudioQueue <= m_nAudio)
	{
		//cout << "[QUEUE.ch" << m_nChannel << "] wait audio put : " << m_nWritePos << endl;
		this_thread::sleep_for(microseconds(10000));
		return 0;
	}
	ELEM *pe = &m_e[m_nWriteAudioPos];
	pe->p = new char[nSize];
	pthread_mutex_lock(&m_mutex);
	if (pe->len == 0)
	{
		memcpy(pe->p, pData, nSize);
		pe->len = nSize;

		m_nWriteAudioPos++;
		if (m_nWriteAudioPos >= m_nMaxAudioQueue)
		{
			m_nWriteAudioPos = 0;
		}
		m_nAudio++;
		cout << "[QUEUE.ch" << m_nChannel << "] m_nWriteAudioPos : " << m_nWriteAudioPos << ", size : " << pe->len << endl;
		pthread_mutex_unlock(&m_mutex);
		return 0;
	}
	pthread_mutex_unlock(&m_mutex);
	usleep(10);
	return 0;
}

int CQueue::GetVideo(AVPacket *pkt, uint64_t *start_pts)
{
	*start_pts = m_start_pts;
#if 1
	if (m_bEnable == false)
	{
		//cout << "[QUEUE.ch" << m_nChannel << "] m_nReadPos : " << m_nReadPos << ", size : " << m_pkt[m_nReadPos].size << endl;
		return NULL;
	}
#endif

	pthread_mutex_lock(&m_mutex);

	if (m_pkt[m_nReadPos].size > 0)
	{
#if 0
		cout << "[QUEUE.ch" << m_nChannel << "] m_nReadPos : " << m_nReadPos << ", size : " << m_pkt[m_nReadPos].size << endl;
#endif
		av_init_packet(pkt);
		av_packet_ref(pkt, &m_pkt[m_nReadPos]);
		av_packet_unref(&m_pkt[m_nReadPos]);

#if 0
		cout << "[QUEUE.ch" << m_nChannel << "] get pos (" << m_nReadPos << "), size (" << pkt->size << "), m_nPacket : " << m_nPacket << ", start_pts : " << *start_pts << endl;
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

void *CQueue::GetAudio(uint64_t *start_pts)
{
	start_pts = &m_start_pts;
#if 1
	if (m_bEnable == false)
	{
		return NULL;
	}
#endif
	ELEM *pe = &m_e[m_nReadAudioPos];
	while (pe->len > 0)
	{
		cout << "[QUEUE.ch" << m_nChannel << "] m_nReadAudioPos : " << m_nReadAudioPos << ", pe->len : " << pe->len << endl;
		pthread_mutex_lock(&m_mutex);
		if (pe->len)
		{
			pthread_mutex_unlock(&m_mutex);
			return pe;
		}
		pthread_mutex_unlock(&m_mutex);
		usleep(10);
	}
	return 0;
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
