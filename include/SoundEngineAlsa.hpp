// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef SOUND_ENGINE
#define SOUND_ENGINE

#include "Vector.hpp"
#include "Event.hpp"
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include "ISoundEngine.hpp"
#include "typenames.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace GLVM::core::Sound
{
	/// PCM data of a wave file, loaded once and played from memory
	struct WaveData {
		bool isLoaded         = false;
		unsigned int channels = 0;
		unsigned int rate     = 0;
		std::vector<int16_t> samples;                     ///< Interleaved signed 16 bit samples
	};

	/*
	  Threading: CreateSoundSample() is called from the game thread, SoundStream() and PlaybackSoundSample()
	  are called from the sound thread. The queue (tSound_Contaier) is guarded by queueMutex, the sound thread
	  sleeps on queueCondition while the queue is empty. waveCache and PCM configuration are used only by the sound thread.
	*/
    class CSoundEngineAlsa : public ISoundEngine
    {
		snd_pcm_t *pPcm = nullptr;
        vector<CSoundSample*> tSound_Contaier;            ///< Queue of samples to play. Guarded by queueMutex
		std::mutex queueMutex;
		std::condition_variable queueCondition;
		std::atomic<bool> isStopRequested{ false };

		std::unordered_map<std::string, WaveData> waveCache;
		std::vector<int16_t> chunkBuffer;                 ///< Reused buffer for samples with applied volume
		unsigned int configuredChannels = 0;
		unsigned int configuredRate     = 0;

		const WaveData& LoadWave( const CSoundSample& _sound_sample );
    public:
		void OpenDevice( const char* device ) override;
		void CloseDevice() override;
        void SoundStream() override;
        void PlaybackSoundSample(CSoundSample& _sound_sample) override;
        void SetMasterVolume(long _lVolume) override;
		/// Not thread safe. Use it only when the sound thread is stopped.
        vector<CSoundSample*>& GetSoundContainer() override;
		void CreateSoundSample( const char* filePath, u32 duration, u32 rate, float volume ) override;
		void StopStream() override;

		~CSoundEngineAlsa();
    };
}

#endif
