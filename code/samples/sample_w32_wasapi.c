#define BASE_NO_ENTRYPOINT 1
#include "base/base.h"
#include "base/base.c"
#include "lib/win32_wasapi.h"

//
// example
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment (lib, "mfplat")
#pragma comment (lib, "mfreadwrite")

typedef struct {
	short* samples;
	size_t count;
	size_t pos;
	bool loop;
} Sound;

// loads any supported sound file, and resamples to mono 16-bit audio with specified sample rate
static Sound S_Load(const WCHAR* path, size_t sampleRate)
{
	Sound sound = { NULL, 0, 0, false };
	HR(MFStartup(MF_VERSION, MFSTARTUP_LITE));
    
	IMFSourceReader* reader;
	HR(MFCreateSourceReaderFromURL(path, NULL, &reader));
    
	// read only first audio stream
	HR(IMFSourceReader_SetStreamSelection(reader, (DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE));
	HR(IMFSourceReader_SetStreamSelection(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE));
    
	const size_t kChannelCount = 1;
	const WAVEFORMATEXTENSIBLE format =
	{
		.Format =
		{
			.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
			.nChannels = (WORD)kChannelCount,
			.nSamplesPerSec = (WORD)sampleRate,
			.nAvgBytesPerSec = (DWORD)(sampleRate * kChannelCount * sizeof(short)),
			.nBlockAlign = (WORD)(kChannelCount * sizeof(short)),
			.wBitsPerSample = (WORD)(8 * sizeof(short)),
			.cbSize = sizeof(format) - sizeof(format.Format),
		},
		.Samples.wValidBitsPerSample = 8 * sizeof(short),
		.dwChannelMask = SPEAKER_FRONT_CENTER,
		.SubFormat = MEDIASUBTYPE_PCM,
	};
    
	// Media Foundation in Windows 8+ allows reader to convert output to different format than native
	IMFMediaType* type;
	HR(MFCreateMediaType(&type));
	HR(MFInitMediaTypeFromWaveFormatEx(type, &format.Format, sizeof(format)));
	HR(IMFSourceReader_SetCurrentMediaType(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, type));
	IMFMediaType_Release(type);
    
	size_t used = 0;
	size_t capacity = 0;
    
	for (;;)
	{
		IMFSample* sample;
		DWORD flags = 0;
		HRESULT hr = IMFSourceReader_ReadSample(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, NULL, &sample);
		if (FAILED(hr))
		{
			break;
		}
        
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			break;
		}
		Assert(flags == 0);
        
		IMFMediaBuffer* buffer;
		HR(IMFSample_ConvertToContiguousBuffer(sample, &buffer));
        
		BYTE* data;
		DWORD size;
		HR(IMFMediaBuffer_Lock(buffer, &data, NULL, &size));
		{
			size_t avail = capacity - used;
			if (avail < size)
			{
				sound.samples = realloc(sound.samples, capacity += 64 * 1024);
			}
			memcpy((char*)sound.samples + used, data, size);
			used += size;
		}
		HR(IMFMediaBuffer_Unlock(buffer));
        
		IMediaBuffer_Release(buffer);
		IMFSample_Release(sample);
	}
    
	IMFSourceReader_Release(reader);
    
	HR(MFShutdown());
    
	sound.pos = sound.count = used / format.Format.nBlockAlign;
	return sound;
}

static void S_Update(Sound* sound, size_t samples)
{
	sound->pos += samples;
	if (sound->loop)
	{
		sound->pos %= sound->count;
	}
	else
	{
		sound->pos = min(sound->pos, sound->count);
	}
}

static void S_Mix(float* outSamples, size_t outSampleCount, float volume, const Sound* sound)
{
	const short* inSamples = sound->samples;
	size_t inPos = sound->pos;
	size_t inCount = sound->count;
	bool inLoop = sound->loop;
    
	for (size_t i = 0; i < outSampleCount; i++)
	{
		if (inLoop)
		{
			if (inPos == inCount)
			{
				// reset looping sound back to start
				inPos = 0;
			}
		}
		else
		{
			if (inPos >= inCount)
			{
				// non-looping sounds stops playback when done
				break;
			}
		}
        
		float sample = inSamples[inPos++] * (1.f / 32768.f);
		outSamples[0] += volume * sample;
		outSamples[1] += volume * sample;
		outSamples += 2;
	}
}

int main()
{
	WasapiAudio audio;
	WA_Start(&audio, 48000, 2, SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
	size_t sampleRate = audio.bufferFormat->nSamplesPerSec;
	size_t bytesPerSample = audio.bufferFormat->nBlockAlign;
    
	// background "music" that will be looping
	Sound background = S_Load(L"C:/Windows/Media/Ring10.wav", sampleRate);
	background.loop = true;
    
	// simple sound effect, won't be looping
	Sound effect = S_Load(L"C:/Windows/Media/tada.wav", sampleRate);
    
	printf("Press SPACE for sound effect, D for small delay, or ESC to stop\n");
    
	HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    
	for (;;)
	{
		bool escPressed = false;
		bool spacePressed = false;
		bool delayPressed = false;
        
		while (WaitForSingleObject(input, 0) == WAIT_OBJECT_0)
		{
			INPUT_RECORD record;
			DWORD read;
			if (ReadConsoleInputW(input, &record, 1, &read)
				&& read == 1
				&& record.EventType == KEY_EVENT
				&& record.Event.KeyEvent.bKeyDown)
			{
				switch (record.Event.KeyEvent.wVirtualKeyCode)
				{
                    case VK_ESCAPE: escPressed = true; break;
                    case VK_SPACE: spacePressed = true; break;
                    case 'D': delayPressed = true; break;
				}
			}
		}
        
		if (escPressed)
		{
			printf("stop!\n");
			break;
		}
        
		if (spacePressed)
		{
			printf("tada!\n");
			effect.pos = 0;
		}
        
		{
			WA_LockBuffer(&audio);
            
			// write at least 100msec of samples into buffer (or whatever space available, whichever is smaller)
			// this is max amount of time you expect code will take until the next iteration of loop
			// if code will take more time then you'll hear discontinuity as buffer will be filled with silence
			size_t writeCount = min(sampleRate/10, audio.sampleCount);
            
			// alternatively you can write as much as "audio.sampleCount" to fully fill the buffer (~1 second)
			// then you can try to increase delay below to 900+ msec, it still should sound fine
			//writeCount = audio.sampleCount;
            
			// advance sound playback positions
			size_t playCount = audio.playCount; 
			S_Update(&background, playCount);
			S_Update(&effect, playCount);
            
			// initialize output with 0.0f
			float* output = audio.sampleBuffer;
			memset(output, 0, writeCount * bytesPerSample);
            
			// mix sounds into output
			S_Mix(output, writeCount, 0.3f, &background);
			S_Mix(output, writeCount, 0.8f, &effect);
            
			WA_UnlockBuffer(&audio, writeCount);
		}
        
		if (delayPressed)
		{
			printf("delay!\n");
			Sleep(5 * 17); // large delay for ~5 frames = ~68 msec
			//Sleep(900);
		}
		else
		{
			// just a small delay, pretend this is your normal rendering code
			Sleep(17); // "60" fps
		}
        
		printf(".");
		fflush(stdout);
	}
    
	WA_Stop(&audio);
    
	printf("Done!\n");
    
	return 0;
}
