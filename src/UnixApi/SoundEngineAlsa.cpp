// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "SoundEngineAlsa.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>

namespace GLVM::core::Sound
{
	namespace {
		constexpr unsigned int pcmLatency         = 500000;      ///< 0.5 s
		constexpr unsigned int chunkFrames        = 1024;        ///< Frames written to the device per one call
		constexpr unsigned int maxSampleFrames    = 300 * 32;    ///< Played part of a sample (keeps previous length of a shot sound)
		constexpr unsigned int maxPendingSamples  = 4;           ///< Older queued samples are dropped, so sound does not lag behind the game

		uint32_t readUint32( const unsigned char* data ) {
			return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
				(static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
		}

		uint16_t readUint16( const unsigned char* data ) {
			return static_cast<uint16_t>( data[0] | (data[1] << 8) );
		}
	}

	void CSoundEngineAlsa::OpenDevice( const char* device ) {
		const int result = snd_pcm_open(&pPcm, device, SND_PCM_STREAM_PLAYBACK, 0);
		if ( result < 0 ) {
			std::cerr << "Sound: can not open PCM device " << device << ": " << snd_strerror(result) << std::endl;
			pPcm = nullptr;
		}
	}

	void CSoundEngineAlsa::CloseDevice() {
		if ( pPcm == nullptr )
			return;

		snd_pcm_drain(pPcm);
        snd_pcm_close(pPcm);
		pPcm = nullptr;
		configuredChannels = 0;
		configuredRate     = 0;
	}

	void CSoundEngineAlsa::StopStream() {
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			isStopRequested = true;
		}
		queueCondition.notify_all();
	}

	/// Called in the sound thread. Sleeps until there is a sample to play or stop is requested.
    void CSoundEngineAlsa::SoundStream()
    {
		std::vector<CSoundSample*> samplesToPlay;
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			queueCondition.wait(lock, [this] { return isStopRequested || tSound_Contaier.GetSize() > 0; });

			const unsigned int pendingSamples = tSound_Contaier.GetSize();
			const unsigned int firstPlayedSample = pendingSamples > maxPendingSamples ? pendingSamples - maxPendingSamples : 0;
			for ( unsigned int i = 0; i < pendingSamples; ++i ) {
				if ( i < firstPlayedSample )
					delete tSound_Contaier[i];
				else
					samplesToPlay.push_back(tSound_Contaier[i]);
			}
			tSound_Contaier.clear();
		}

		for ( CSoundSample* sample : samplesToPlay ) {
			if ( !isStopRequested )
				PlaybackSoundSample(*sample);

			delete sample;
		}
    }

	/// Load wave file once (called only in the sound thread). Header is parsed, only PCM data is kept.
	const WaveData& CSoundEngineAlsa::LoadWave( const CSoundSample& _sound_sample ) {
		const std::string path = _sound_sample.kPath_to_File_;
		auto cachedWave = waveCache.find(path);
		if ( cachedWave != waveCache.end() )
			return cachedWave->second;

		WaveData& wave = waveCache[path];                  ///< Failed load is cached too, so file is not reopened on every shot

		FILE* file = fopen(path.c_str(), "rb");
		if ( file == nullptr ) {
			std::cerr << "Sound: can not open file " << path << ": " << std::strerror(errno) << std::endl;
			return wave;
		}

		std::vector<unsigned char> fileData;
		unsigned char readBuffer[4096];
		size_t readBytes = 0;
		while ( (readBytes = fread(readBuffer, 1, sizeof(readBuffer), file)) > 0 )
			fileData.insert(fileData.end(), readBuffer, readBuffer + readBytes);
		fclose(file);

		const unsigned char* data = fileData.data();
		const size_t size = fileData.size();
		size_t pcmOffset = 0;
		size_t pcmSize   = 0;
		unsigned int channels = 0;
		unsigned int rate = 0;
		unsigned int bitsPerSample = 0;
		unsigned int audioFormat = 0;

		if ( size >= 12 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WAVE", 4) == 0 ) {
			size_t offset = 12;
			while ( offset + 8 <= size ) {
				const uint32_t chunkSize = readUint32(data + offset + 4);
				const size_t chunkDataOffset = offset + 8;
				const size_t availableSize = std::min<size_t>(chunkSize, size - chunkDataOffset);
				if ( std::memcmp(data + offset, "fmt ", 4) == 0 && availableSize >= 16 ) {
					audioFormat   = readUint16(data + chunkDataOffset);
					channels      = readUint16(data + chunkDataOffset + 2);
					rate          = readUint32(data + chunkDataOffset + 4);
					bitsPerSample = readUint16(data + chunkDataOffset + 14);
				} else if ( std::memcmp(data + offset, "data", 4) == 0 ) {
					pcmOffset = chunkDataOffset;
					pcmSize   = availableSize;
					break;
				}

				offset = chunkDataOffset + chunkSize + (chunkSize & 1);   ///< Chunks are aligned to 2 bytes
			}

			if ( audioFormat != 1 || bitsPerSample != 16 || channels == 0 || rate == 0 || pcmSize == 0 ) {
				std::cerr << "Sound: unsupported wave format in " << path << " (only PCM 16 bit is supported)" << std::endl;
				return wave;
			}
		} else {
			/// Not a RIFF file: raw 16 bit stereo PCM with the rate of the sample
			pcmOffset = 0;
			pcmSize   = size;
			channels  = 2;
			rate      = _sound_sample.uiRate_;
		}

		wave.channels = channels;
		wave.rate     = rate;
		wave.samples.resize(pcmSize / sizeof(int16_t));
		std::memcpy(wave.samples.data(), data + pcmOffset, wave.samples.size() * sizeof(int16_t));
		wave.isLoaded = !wave.samples.empty();
		return wave;
	}

    void CSoundEngineAlsa::PlaybackSoundSample(CSoundSample& _sound_sample)
    {
		if ( pPcm == nullptr || _sound_sample.kPath_to_File_ == nullptr )
			return;

		const WaveData& wave = LoadWave(_sound_sample);
		if ( !wave.isLoaded )
			return;

		/// Device is reconfigured only when format changes, so samples of the same format are played continuously
		if ( wave.channels != configuredChannels || wave.rate != configuredRate ) {
			if ( configuredRate != 0 )
				snd_pcm_drain(pPcm);

			const int result = snd_pcm_set_params(pPcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
												  wave.channels, wave.rate, 1, pcmLatency);
			if ( result < 0 ) {
				std::cerr << "Sound: can not set PCM parameters: " << snd_strerror(result) << std::endl;
				configuredChannels = 0;
				configuredRate     = 0;
				return;
			}
			configuredChannels = wave.channels;
			configuredRate     = wave.rate;
		}

		const size_t channels    = wave.channels;
		const size_t totalFrames = std::min<size_t>(wave.samples.size() / channels, maxSampleFrames);
		chunkBuffer.resize(chunkFrames * channels);

		for ( size_t frameOffset = 0; frameOffset < totalFrames && !isStopRequested; ) {
			const size_t frames = std::min<size_t>(chunkFrames, totalFrames - frameOffset);
			const int16_t* source = wave.samples.data() + frameOffset * channels;
			for ( size_t i = 0; i < frames * channels; ++i ) {
				int32_t sample = static_cast<int32_t>(source[i] * _sound_sample.volume);
				sample = std::clamp(sample, -32768, 32767);
				chunkBuffer[i] = static_cast<int16_t>(sample);
			}

			size_t writtenFrames = 0;
			while ( writtenFrames < frames ) {
				snd_pcm_sframes_t result = snd_pcm_writei(pPcm, chunkBuffer.data() + writtenFrames * channels, frames - writtenFrames);
				if ( result == -EAGAIN )
					continue;

				if ( result < 0 ) {
					/// Underrun (-EPIPE) or suspend (-ESTRPIPE): recover the stream and repeat the write
					const int recoverResult = snd_pcm_recover(pPcm, static_cast<int>(result), 1);
					if ( recoverResult < 0 ) {
						std::cerr << "Sound: write to PCM failed: " << snd_strerror(recoverResult) << std::endl;
						return;
					}
					continue;
				}

				writtenFrames += static_cast<size_t>(result);
			}

			frameOffset += frames;
		}
    }

    void CSoundEngineAlsa::SetMasterVolume(long _lVolume)
    {
        long lMin, lMax;
        snd_mixer_t* pHandle;
        snd_mixer_selem_id_t* pSid;
        const char* pCard = "default";
        const char* pSelem_Name = "Master";

        snd_mixer_open(&pHandle, 0);
        snd_mixer_attach(pHandle, pCard);
        snd_mixer_selem_register(pHandle, NULL, NULL);
        snd_mixer_load(pHandle);

        snd_mixer_selem_id_alloca(&pSid);
        snd_mixer_selem_id_set_index(pSid, 0);
        snd_mixer_selem_id_set_name(pSid, pSelem_Name);
        snd_mixer_elem_t* pElem = snd_mixer_find_selem(pHandle, pSid);

        snd_mixer_selem_get_playback_volume_range(pElem, &lMin, &lMax);
        snd_mixer_selem_set_playback_volume_all( pElem, lMin + (_lVolume * (lMax - lMin)) / 100 );

        snd_mixer_close(pHandle);
    }

    vector<CSoundSample*>& CSoundEngineAlsa::GetSoundContainer() { return tSound_Contaier; }

	/// Called in the game thread
	void CSoundEngineAlsa::CreateSoundSample( const char* filePath, u32 duration, u32 rate, float volume ) {
		core::Sound::CSoundSample* pSound_Sample = new core::Sound::CSoundSample();
		pSound_Sample->kPath_to_File_ = filePath;
		pSound_Sample->uiDuration_ = duration;
		pSound_Sample->uiRate_ = rate;
		pSound_Sample->volume  = volume;
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			tSound_Contaier.Push(pSound_Sample);
		}
		queueCondition.notify_one();
	}

	CSoundEngineAlsa::~CSoundEngineAlsa() {
		CloseDevice();
		for( u32 i = 0; i < tSound_Contaier.GetSize(); ++i ) {
			delete tSound_Contaier[i];
			tSound_Contaier[i] = nullptr;
		}
	}
}
