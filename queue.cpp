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

	pthread_mutex_init(&m_mutex, NULL);
}

CQueue::~CQueue()
{
	pthread_mutex_destroy(&m_mutex);
}

void CQueue::SetInfo(int nChannel, string type)
{
	m_nChannel = nChannel;
	m_type = type;
}

void CQueue::Clear()
{
	for (int i = 0; i < m_nMaxQueue; i++)
	{
		//avpacket unref 해야 함
	}

	m_nReadPos = 0;
	m_nWritePos = 0;

	m_nReadFramePos = 0;
	m_nWriteFramePos = 0;

	m_bEnable = false;
	m_nPacket = 0;
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

int CQueue::PutAudio(char *pData, int nSize)
{
	int nCount = 0;

	if (m_nMaxAudioQueue < m_nAudio)
	{
		return 0;
	}

	while (true)
	{
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
			//cout << "[QUEUE.ch" << m_nChannel << "] m_nWriteAudioPos : " << m_nWriteAudioPos << endl;
			pthread_mutex_unlock(&m_mutex);
			return 0;
		}
		pthread_mutex_unlock(&m_mutex);
		usleep(10);

		nCount++;
		if (nCount >= 100000)
		{
			cout << "[QUEUE.ch" << m_nChannel << " ] PutAudio TImeout" << endl;
			break;
		}
	}
}

int CQueue::Put(AVPacket *pkt)
{
	int nCount = 0; // timeout 위한 용도

	while (!m_bExit)
	{
		if ((m_nMaxQueue < m_nPacket) && !m_bExit)
		{
			cout << "[QUEUE.ch" << m_nChannel << "] put video wait : " << m_nWritePos << endl;
			this_thread::sleep_for(microseconds(10000));
			continue;
		}

		pthread_mutex_lock(&m_mutex);
		if (pkt->size > 0)
		{
			av_init_packet(&m_pkt[m_nWritePos]);
			av_packet_ref(&m_pkt[m_nWritePos], pkt);
			av_packet_unref(pkt);
#if __DEBUG
			_d("put pos : ( %d ), stream_index : %d, data ( %p ), size ( %d )\n", m_nWritePos, pkt->stream_index, m_pkt[m_nWritePos].data, m_pkt[m_nWritePos].size);
#endif
			m_nWritePos++;
			if (m_nWritePos >= m_nMaxQueue)
			{
				m_nWritePos = 0;
			}
			m_nPacket++;
			pthread_mutex_unlock(&m_mutex);
			return m_pkt[m_nWritePos - 1].size;
		}
		pthread_mutex_unlock(&m_mutex);
		usleep(10);

		nCount++;
		if (nCount >= 100000)
		{
			cout << "[QUEUE.ch" << m_nChannel << "] PutVideo Timeout" << endl;
			break;
		}
	}
	return 0;
}

void *CQueue::GetAudio()
{
	if (m_bEnable == false)
	{
		return NULL;
	}

	while (true)
	{
		ELEM *pe = &m_e[m_nReadAudioPos];
		//cout << "[QUEUE.ch" << m_nChannel << "] m_nReadAudioPos : " << m_nReadAudioPos << ", pe->len : " << pe->len << endl;
		pthread_mutex_lock(&m_mutex);
		if (pe->len)
		{
			pthread_mutex_unlock(&m_mutex);
			return pe;
		}
		pthread_mutex_unlock(&m_mutex);
		usleep(10);
	}
}

int CQueue::Get(AVPacket *pkt)
{
	if (m_bEnable == false)
	{
		return NULL;
	}

	pthread_mutex_lock(&m_mutex);

	if (m_pkt[m_nReadPos].size > 0)
	{
#if __DEBUG
		cout << "[QUEUE.ch" << m_nChannel << "] m_nReadPos : " << m_nReadPos << ", size : " << m_pkt[m_nReadPos].size << endl;
#endif

		av_init_packet(pkt);
		av_packet_ref(pkt, &m_pkt[m_nReadPos]);
		av_packet_unref(&m_pkt[m_nReadPos]);
#if 0
			if (m_nPacket < m_nDelay)
			{
				pthread_mutex_unlock(&m_mutex);
				return NULL;
			}
#endif
#if 0
		_d("[QUEUE.ch%d] get pos ( %d ), size ( %d ), data ( %p ),  type : %d, m_nPacket : %d\n", m_nChannel, m_nReadPos, m_pkt[m_nReadPos].size, m_pkt[m_nReadPos].data, m_pkt[m_nReadPos].stream_index, m_nPacket);
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

void CQueue::Ret(AVPacket *pkt)
{
	pthread_mutex_lock(&m_mutex);

	av_packet_unref(pkt);

	m_nReadPos++;
#if __DEBUG
	if (m_nChannel == 0)
	{
		cout << "m_nReadPos : " << m_nReadPos << endl;
	}
#endif
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
