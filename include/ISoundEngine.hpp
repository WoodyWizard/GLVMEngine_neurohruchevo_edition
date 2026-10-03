// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef ISOUND_ENGINE
#define ISOUND_ENGINE

#include "Vector.hpp"
#include "typenames.hpp"

namespace GLVM::core::Sound
{
    struct CSoundSample
    {
        const char* kPath_to_File_;
        unsigned int uiDuration_;
        unsigned int uiRate_;
		float volume;
    };
    
    class ISoundEngine
    {
    public:
        virtual ~ISoundEngine() {}

		virtual void OpenDevice( const char* device ) = 0;
		virtual void CloseDevice() = 0;
        virtual vector<CSoundSample*>& GetSoundContainer() = 0;
        virtual void PlaybackSoundSample(CSoundSample& _sound_sample) = 0;
        virtual void SetMasterVolume(long _lVolume) = 0;
        virtual void SoundStream() = 0;
		virtual void CreateSoundSample( const char* filePath, u32 duration, u32 rate, float volume ) = 0;
		/// Wake up the thread blocked in SoundStream() and make it return. Used on shutdown before join of the sound thread.
		virtual void StopStream() {}
    };
}

#endif
