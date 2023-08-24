#include "StdInc.h"

#include "AEStaticChannel.h"

#include "AESmoothFadeThread.h"

void CAEStaticChannel::InjectHooks() {
    RH_ScopedClass(CAEStaticChannel);
    RH_ScopedCategory("Audio/Hardware");

#ifdef USE_DSOUND
    constexpr bool IsIncompatible = false;
#else
    constexpr bool IsIncompatible = true;
#endif

    RH_ScopedVirtualInstall(Service, 0x4F10D0, {.locked = IsIncompatible});
    RH_ScopedVirtualInstall(IsSoundPlaying, 0x4F0F40, {.locked = IsIncompatible});
    RH_ScopedVirtualInstall(GetPlayTime, 0x4F0F70, {.locked = IsIncompatible});
    RH_ScopedVirtualInstall(GetLength, 0x4F0FA0, {.locked = IsIncompatible});
    RH_ScopedVirtualInstall(Play, 0x4F0BD0, {.locked = IsIncompatible});
    RH_ScopedVirtualInstall(SynchPlayback, 0x4F1040, {.locked = IsIncompatible});
    RH_ScopedVirtualInstall(Stop, 0x4F0FB0, {.locked = IsIncompatible});

    RH_ScopedInstall(SetAudioBuffer, 0x4F0C40, {.reversed = false});
}

CAEStaticChannel::CAEStaticChannel(void* platform, uint16 channelId, bool arg3, uint32 samplesPerSec, uint16 bitsPerSample)
    : CAEAudioChannel(platform, channelId, samplesPerSec, bitsPerSample)
{
    m_bNeedData = false;
    m_bNeedsSynch = false;
    field_8A = arg3;
}

// 0x4F0F40
bool CAEStaticChannel::IsSoundPlaying() {
#if defined(USE_DSOUND)
    if (!m_pDirectSoundBuffer)
#elif defined(USE_OPENAL)
    if (!m_pSource)
#endif
        return false;

    if (m_bPaused || m_bNeedsSynch)
        return true;

    return CAEAudioChannel::IsBufferPlaying();
}

// 0x4F0F70
int16 CAEStaticChannel::GetPlayTime() {
    if (!IsSoundPlaying())
        return -1;

    const auto curPos = CAEAudioChannel::GetCurrentPlaybackPosition();
    return CAEAudioChannel::ConvertFromBytesToMS(curPos);
}
int16 CAEStaticChannel::GetPlayTime_Reversed() {
    return CAEStaticChannel::GetPlayTime();
}

// 0x4F0FA0
uint16 CAEStaticChannel::GetLength() {
    return CAEAudioChannel::ConvertFromBytesToMS(m_nLengthInBytes);
}

// 0x4F0BD0
void CAEStaticChannel::Play(int16 timeInMs, int8 unused, float scalingFactor) {
    if (m_bLooped && m_nCurrentBufferOffset != 0 || !timeInMs) {
        m_bUnkn2 = false;
    } else {
#if defined(USE_DSOUND)
        m_pDirectSoundBuffer->SetCurrentPosition(ConvertFromMsToBytes(timeInMs));
#elif defined(USE_OPENAL)
        m_pSource->ObtainSource();
        const auto offset = ConvertFromMsToBytes(timeInMs);
        alSourcei(m_pSource->m_sourceId, AL_BYTE_OFFSET, offset);
        m_pSource->m_posOffset = offset;
#endif
        m_bUnkn2 = true;
    }
    m_bNeedsSynch = true;
    m_bPaused = scalingFactor == 0.0f;
}


// 0x4F1040
void CAEStaticChannel::SynchPlayback() {
#if defined(USE_DSOUND)
    if (!m_pDirectSoundBuffer)
#elif defined(USE_OPENAL)
    if (!m_pSource)
#endif
        return;

    if (!m_bNeedsSynch || m_bPaused)
        return;

    if (m_bUnkn2) {
#if defined(USE_DSOUND)
        m_pDirectSoundBuffer->SetVolume(-10000);
        if (!AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, m_fVolume, -2, false)) {
            const auto dwVolume = static_cast<LONG>(m_fVolume * 100.0F);
            m_pDirectSoundBuffer->SetVolume(dwVolume);
        }
#elif defined(USE_OPENAL)
        m_pSource->SetVolume(0.0f);
        if (!AESmoothFadeThread.RequestFade(m_pSource, m_fVolume, -2, false)) {
            m_pSource->SetVolume(m_fVolume);
        }
#endif
    }

#if defined(USE_DSOUND)
    m_pDirectSoundBuffer->Play(0, 0, m_bLooped);
#elif defined(USE_OPENAL)
    m_pSource->ObtainSource();
    if (m_pSource->m_type != OALSourceType::OST_Preloop)
        alSourcei(m_pSource->m_sourceId, AL_LOOPING, m_bLooped);

    m_pSource->Play();
#endif

    m_nSyncTime = CTimer::GetTimeInMS();
    m_bNeedsSynch = false;
}

// 0x4F0FB0
void CAEStaticChannel::Stop() {
#if defined(USE_DSOUND)
    if (m_pDirectSoundBuffer && CAEAudioChannel::IsBufferPlaying() && !AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, -100.0F, -1, true)) {
        m_pDirectSoundBuffer->Stop();
    }

    if (m_pDirectSoundBuffer) {
        --g_numSoundChannelsUsed;
        m_pDirectSoundBuffer->Release();
        m_pDirectSoundBuffer = nullptr;
    }

    if (m_pDirectSound3DBuffer) {
        m_pDirectSound3DBuffer->Release();
        m_pDirectSound3DBuffer = nullptr;
    }
#elif defined(USE_OPENAL)
    if (m_pSource && CAEAudioChannel::IsBufferPlaying() && !AESmoothFadeThread.RequestFade(m_pSource, -100.0F, -1, true)) {
        m_pSource->Stop();
    }

    if (m_pSource) {
        --g_numSoundChannelsUsed;
        m_pSource->Release();
        m_pSource = nullptr;
    }
#endif
    m_bPaused = false;
}

// 0x4F10D0
void CAEStaticChannel::Service() {
#if defined(USE_DSOUND)
    if (!m_pDirectSoundBuffer) {
        m_nBufferStatus = 0;
        return;
    }

    if (m_bNeedData && (int32)(CTimer::GetTimeInMS() - m_nSyncTime) > field_74) {
        uint8* ppvAudioPtr1{};
        DWORD pdwAudioBytes{};

        VERIFY(SUCCEEDED(m_pDirectSoundBuffer->Lock(
            m_dwLockOffset,
            m_nNumLockBytes,
            reinterpret_cast<LPVOID*>(&ppvAudioPtr1),
            &pdwAudioBytes,
            nullptr,
            0,
            0
        )));

        for (auto i = 0u; i < m_nNumLoops; i++) {
            memcpy(
                &ppvAudioPtr1[i * m_nNumLockBytes],
                (uint8*)m_pBuffer + m_nCurrentBufferOffset,
                m_nNumLockBytes
            );
        }
        VERIFY(SUCCEEDED(m_pDirectSoundBuffer->Unlock(ppvAudioPtr1, pdwAudioBytes, nullptr, 0)));
        m_bNeedData = false;
    }
#elif defined(USE_OPENAL)
    if (!m_pSource)
        return;
#endif
    UpdateStatus();

    if (!m_bPaused) {
#if defined(USE_DSOUND)
        if (bufferStatus.Bit0x1)
            return;

        if (const auto buf = std::exchange(m_pDirectSoundBuffer, nullptr)) {
            --g_numSoundChannelsUsed;
            buf->Release();
        }
#elif defined(USE_OPENAL)
        m_pSource->ObtainSource();
        if (m_pSource->m_currentState == AL_NONE) {
            m_pSource->m_currentState = AL_STOPPED;
            alGetSourcei(m_pSource->m_sourceId, AL_SOURCE_STATE, &m_pSource->m_currentState);
        }

        if (m_pSource->m_currentState == AL_PLAYING)
            return;

        if (const auto buf = std::exchange(m_pSource, nullptr)) {
            --g_numSoundChannelsUsed;
            buf->Release();
        }
#endif
    }
}

void CAEStaticChannel::Service_Reversed() {
    CAEStaticChannel::Service();
}

bool CAEStaticChannel::IsSoundPlaying_Reversed() {
    return CAEStaticChannel::IsSoundPlaying();
}

uint16 CAEStaticChannel::GetLength_Reversed() {
    return CAEStaticChannel::GetLength();
}

void CAEStaticChannel::Play_Reversed(int16 a, int8 b, float c) {
    CAEStaticChannel::Play(a, b, c);
}

void CAEStaticChannel::SynchPlayback_Reversed() {
    CAEStaticChannel::SynchPlayback();
}

void CAEStaticChannel::Stop_Reversed() {
    CAEStaticChannel::Stop();
}
