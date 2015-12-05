#include <CVideo.h>

volatile AudioQueue *audio_queue = NULL;
volatile AudioQueue *audio_queue_tail = NULL;

FMOD_RESULT F_CALLBACK AudioCallback(FMOD_SOUND *sound, void *stream, int len) {
	Sint16 *dst = (Sint16 *)stream;

	while (audio_queue && (len > 0)) {
		volatile AudioQueue *item = audio_queue;
		AudioQueue *next = item->next;
		const int channels = item->audio->channels;

		const float *src = item->audio->samples + (item->offset * channels);
		int cpy = (item->audio->frames - item->offset) * channels;
		int i;

		if (cpy > (len / sizeof(Sint16))) {
			cpy = len / sizeof(Sint16);
		}

		for (i = 0; i < cpy; i++) {
			const float val = *(src++);

			if (val < -1.0f)
				*(dst++) = -32768;
			else if (val > 1.0f)
				*(dst++) = 32767;
			else
				*(dst++) = (Sint16)(val * 32767.0f);
		}

		item->offset += (cpy / channels);
		len -= cpy * sizeof(Sint16);

		if (item->offset >= item->audio->frames) {
			THEORAPLAY_freeAudio(item->audio);
			free((void *)item);
			audio_queue = next;
		}
	}

	if ( ! audio_queue) {
		audio_queue_tail = NULL;
	}

	if (len > 0) {
		memset(dst, '\0', len);
	}

	return FMOD_OK;
}

CVideo::CVideo() {
	m_baseticks = 0;
	m_audio = NULL;
	m_video = NULL;
	decoder = NULL;
	m_texture = NULL;
}

void CVideo::SetCore(CCore *core) {
	m_core = core;
}

CCore *CVideo::GetCore(void) {
	return m_core;
}

void CVideo::QueueAudio(const THEORAPLAY_AudioPacket *audio) {
	AudioQueue *item = (AudioQueue *)malloc(sizeof(AudioQueue));
	if ( ! item) {
		THEORAPLAY_freeAudio(audio);
		return;
	}

	item->audio = audio;
	item->offset = 0;
	item->next = NULL;

	if (audio_queue_tail) {
		audio_queue_tail->next = item;
	} else {
		audio_queue = item;
	}

	audio_queue_tail = item;
}

void CVideo::SetAudioLength(int seconds) {
	int sampleRate = 44100;
	int channels = 2;

	FMOD_CREATESOUNDEXINFO soundInfo;
	memset(&soundInfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
	soundInfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
	soundInfo.decodebuffersize = sampleRate;
	soundInfo.length = sampleRate * channels * sizeof(signed short)* seconds;
	soundInfo.numchannels = channels;
	soundInfo.defaultfrequency = sampleRate;
	soundInfo.format = FMOD_SOUND_FORMAT_PCM16;
	soundInfo.pcmreadcallback = (FMOD_SOUND_PCMREAD_CALLBACK)AudioCallback;
	soundInfo.pcmsetposcallback = NULL;

	m_core->GetAudio()->setStreamBufferSize(65536, FMOD_TIMEUNIT_RAWBYTES);
	m_core->GetAudio()->createStream(NULL, FMOD_OPENUSER, &soundInfo, &m_sound);
	m_core->GetAudio()->playSound(m_sound, 0, false, &m_channel);
}

void CVideo::Play(char *filename) {
	int w = m_core->GetLogicalWidth();
	int h = m_core->GetLogicalHeight();
	SDL_Renderer *renderer = m_core->GetRenderer();

	m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, w, h);

	decoder = THEORAPLAY_startDecodeFile(filename, 25, THEORAPLAY_VIDFMT_IYUV);
	if ( ! decoder) {
		printf("Failed to start decoding '%s'!\n", filename);
		return;
	}

	// wait until we have video and/or audio data, so we can set up hardware.
	while ( ! m_audio || ! m_video) {
		if ( ! m_audio) m_audio = THEORAPLAY_getAudio(decoder);
		if ( ! m_video) m_video = THEORAPLAY_getVideo(decoder);
		SDL_Delay(10);
	}

	m_framems = (m_video->fps == 0.0) ? 0 : ((Uint32)(1000.0 / m_video->fps));

	while (m_audio) {
		QueueAudio(m_audio);
		m_audio = THEORAPLAY_getAudio(decoder);
	}

	m_baseticks = SDL_GetTicks();

	while (THEORAPLAY_isDecoding(decoder)) {
		m_core->GetEvent();
		const Uint32 now = SDL_GetTicks() - m_baseticks;

		if ( ! m_video) {
			m_video = THEORAPLAY_getVideo(decoder);
		}

		if (m_video && (m_video->playms <= now)) {
			if (m_framems && ((now - m_video->playms) >= m_framems)) {
				const THEORAPLAY_VideoFrame *last = m_video;
				while ((m_video = THEORAPLAY_getVideo(decoder)) != NULL) {
					THEORAPLAY_freeVideo(last);
					last = m_video;
					if ((now - m_video->playms) < m_framems)
						break;
				}

				if ( ! m_video) {
					m_video = last;
				}
			}

			if (m_video) {
				SDL_UpdateTexture(m_texture, NULL, (void**)m_video->pixels, m_video->width);

				m_core->BeginScene();
				m_core->Draw(m_texture);
				m_core->EndScene();
			}

			THEORAPLAY_freeVideo(m_video);
			m_video = NULL;
		} else {
			SDL_Delay(10);
		}

		while ((m_audio = THEORAPLAY_getAudio(decoder)) != NULL) {
			QueueAudio(m_audio);
		}
	}
}

void CVideo::Drop(void) {
	m_core->DropTexture(m_texture);
	m_core->DropSound(m_sound);

	if (m_video) THEORAPLAY_freeVideo(m_video);
	if (m_audio) THEORAPLAY_freeAudio(m_audio);
	if (decoder) THEORAPLAY_stopDecode(decoder);
}
