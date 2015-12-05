#ifndef _CVIDEO_H_
#define _CVIDEO_H_

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <SDL.h>
#include <CCore.h>
#include <theoraplay.h>

typedef struct AudioQueue
{
	const THEORAPLAY_AudioPacket *audio;
	int offset;
	struct AudioQueue *next;
} AudioQueue;

FMOD_RESULT F_CALLBACK  AudioCallback(FMOD_SOUND *sound, void *stream, int len);

class CVideo
{
public:
	CVideo();

	void SetCore(CCore *core);
	CCore *GetCore(void);
	
	void SetAudioLength(int seconds);
	void Play(char *filename);

	void Drop(void);

private:
	void QueueAudio(const THEORAPLAY_AudioPacket *audio);

	Uint32 m_baseticks;

	FMOD::Sound *m_sound;
	FMOD::Channel *m_channel;

	CCore *m_core;

	THEORAPLAY_Decoder *decoder;
	const THEORAPLAY_VideoFrame *m_video;
	const THEORAPLAY_AudioPacket *m_audio;

	SDL_Texture *m_texture;

	Uint32 m_framems;
};

#endif // _CVIDEO_H_