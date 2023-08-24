#include "StdInc.h"

#include "AEAudioChannel.h"

#include "AEAudioUtility.h"
#include "AESmoothFadeThread.h"

uint32& g_numSoundChannelsUsed = *(uint32*)0xB5F898;

void CAEAudioChannel::InjectHooks() {
    RH_ScopedClass(CAEAudioChannel);
    RH_ScopedCategory("Audio/Hardware");

    RH_ScopedInstall(SetPosition, 0x4D7950);
    RH_ScopedInstall(UpdateStatus, 0x4D7BD0);
    RH_ScopedInstall(Lost, 0x4D7A10);
    RH_ScopedInstall(ConvertFromBytesToMS, 0x4D79D0);
    RH_ScopedInstall(ConvertFromMsToBytes, 0x4D79F0);
    RH_ScopedInstall(SetFrequency, 0x4D7A50);
    RH_ScopedInstall(SetVolume, 0x4D7C60);
    RH_ScopedInstall(SetOriginalFrequency, 0x4D7A70);
    RH_ScopedVirtualInstall(SetFrequencyScalingFactor, 0x4D7D00);
    RH_ScopedInstall(GetCurrentPlaybackPosition, 0x4D79A0);
}

// 0x4D7890
CAEAudioChannel::CAEAudioChannel(void* platform, uint16 channelId, uint32 samplesPerSec, uint16 bitsPerSample) {
    m_nChannelId         = channelId;
    m_nFlags             = 0;
    m_nFrequency         = samplesPerSec;
    m_nOriginalFrequency = samplesPerSec;
    m_fVolume            = -100.0f;
    m_bPaused   = false;
    m_bLooped            = false;
    m_bShouldStop        = false;

#if defined(USE_DSOUND)
    m_pDirectSound = reinterpret_cast<IDirectSound*>(platform);
    m_nBufferStatus = 0;
    m_pDirectSoundBuffer = nullptr;
    m_pDirectSound3DBuffer = nullptr;
    m_WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    m_WaveFormat.nChannels = 1;
    m_WaveFormat.nSamplesPerSec = samplesPerSec;
    m_WaveFormat.nAvgBytesPerSec = samplesPerSec * (bitsPerSample / 8);
    m_WaveFormat.nBlockAlign = 2;
    m_WaveFormat.wBitsPerSample = bitsPerSample;
    m_WaveFormat.cbSize = 0;
#elif defined(USE_OPENAL)
    m_pSource = reinterpret_cast<OALSource*>(platform);
#endif
}

// 0x4D7910
CAEAudioChannel::~CAEAudioChannel() {
#if defined(USE_DSOUND)
    if (m_pDirectSoundBuffer) {
        --g_numSoundChannelsUsed;
        m_pDirectSoundBuffer->Release();
        m_pDirectSoundBuffer = nullptr;
    }

    SAFE_RELEASE(m_pDirectSound3DBuffer);
#elif defined(USE_OPENAL)
    if (m_pSource) {
        --g_numSoundChannelsUsed;
        m_pSource->Release();
        m_pSource = nullptr;
    }
#endif
}

void CAEAudioChannel::SetFrequencyScalingFactor_Reversed(float factor) {
    CAEAudioChannel::SetFrequencyScalingFactor(factor);
}

// 0x4D7950
void CAEAudioChannel::SetPosition(const CVector& vecPos) const {
#if defined(USE_DSOUND)
    if (!m_pDirectSoundBuffer)
        return;

    if (!m_pDirectSound3DBuffer)
        return;

    m_pDirectSound3DBuffer->SetPosition(vecPos.x, vecPos.y, vecPos.z, DS3D_DEFERRED);
#elif defined(USE_OPENAL)
    if (!m_pSource)
        return;

    m_pSource->ObtainSource();
    alSource3f(m_pSource->m_sourceId, AL_POSITION, vecPos.x, vecPos.y, vecPos.z);
#endif
}

bool CAEAudioChannel::IsBufferPlaying() const {
#if defined(USE_DSOUND)
    return m_nBufferStatus & DSBSTATUS_PLAYING;
#elif defined(USE_OPENAL)
    if (!m_pSource)
        return false;

    m_pSource->ObtainSource();
    if (m_pSource->m_currentState != AL_NONE)
        return false;

    m_pSource->m_currentState = AL_STOPPED;
    alGetSourcei(m_pSource->m_sourceId, AL_SOURCE_STATE, &m_pSource->m_currentState);

    return m_pSource->m_currentState == AL_PLAYING;
#endif
}

// 0x4D79A0
uint32 CAEAudioChannel::GetCurrentPlaybackPosition() const {
#if defined(USE_DSOUND)
    if (!m_pDirectSoundBuffer)
        return 0;

    uint32 outPos = 0;
    m_pDirectSoundBuffer->GetCurrentPosition(reinterpret_cast<LPDWORD>(&outPos), nullptr);
    return outPos;
#elif defined(USE_OPENAL)
    if (!m_pSource)
        return 0;

    ALint offset{};
    alGetSourcei(m_pSource->m_sourceId, AL_BYTE_OFFSET, &offset);

    return m_pSource->m_posOffset + offset;
#endif
}

// 0x4D79D0
uint32 CAEAudioChannel::ConvertFromBytesToMS(uint32 bytes) const {
#if defined(USE_DSOUND)
    return CAEAudioUtility::ConvertFromBytesToMS(bytes, m_WaveFormat.nSamplesPerSec, m_WaveFormat.nChannels);
#elif defined(USE_OPENAL)
    return CAEAudioUtility::ConvertFromBytesToMS(bytes, m_nOriginalFrequency, 1u); // TODO: 1 channel?
#endif
}

// 0x4D79F0
uint32 CAEAudioChannel::ConvertFromMsToBytes(uint32 ms) const {
#if defined(USE_DSOUND)
    return CAEAudioUtility::ConvertFromMSToBytes(ms, m_WaveFormat.nSamplesPerSec, m_WaveFormat.nChannels);
#elif defined(USE_OPENAL)
    return CAEAudioUtility::ConvertFromMSToBytes(ms, m_nOriginalFrequency, 1u); // TODO: 1 channel?
#endif
}

// 0x4D7A50
void CAEAudioChannel::SetFrequency(uint32 freq) {
    if (m_nFrequency == freq)
        return;

    m_nFrequency = freq;

#if defined(USE_DSOUND)
    if (m_pDirectSoundBuffer) {
        VERIFY_TODO_FIX(SUCCEEDED(m_pDirectSoundBuffer->SetFrequency(freq)));
    }
#elif defined(USE_OPENAL)
    if (!m_pSource)
        return;

    m_pSource->ObtainSource();
    alSourcef(m_pSource->m_sourceId, AL_PITCH, float(freq) / float(m_nOriginalFrequency));
#endif
}

// 0x4D7C60
void CAEAudioChannel::SetVolume(float volume) {
#if defined(USE_DSOUND)
    if (!m_pDirectSoundBuffer)
#elif defined(USE_OPENAL)
    if (!m_pSource || m_fVolume == volume)
#endif
        return;

    if (IsBufferPlaying() && fabs(volume - m_fVolume) > 60.0F) {
        if (volume <= m_fVolume) {
#if defined(USE_DSOUND)
            if (AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, volume, -1, false)) {
#elif defined(USE_OPENAL)
            if (AESmoothFadeThread.RequestFade(m_pSource, volume, -1, false)) {
#endif
                m_fVolume = volume;
                return;
            }
        }
#if defined(USE_DSOUND)
        else if (AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, volume, -2, false))
#elif defined(USE_OPENAL)
        else if (AESmoothFadeThread.RequestFade(m_pSource, volume, -2, false))
#endif
        {
            m_fVolume = volume;
            return;
        }
    }

#if defined(USE_DSOUND)
    AESmoothFadeThread.SetBufferVolume(m_pDirectSoundBuffer, volume);
#elif defined(USE_OPENAL)
    AESmoothFadeThread.SetBufferVolume(m_pSource, volume);
#endif
    m_fVolume = volume;
}

// 0x4D7A70
void CAEAudioChannel::SetOriginalFrequency(uint32 freq) {
    SetFrequency(freq);
    m_nOriginalFrequency = freq;
}

// 0x4D7BD0
void CAEAudioChannel::UpdateStatus() {
#if defined(USE_DSOUND)
    m_pDirectSoundBuffer->GetStatus(reinterpret_cast<LPDWORD>(&m_nBufferStatus));
    if (m_nBufferStatus & DSBSTATUS_BUFFERLOST) {
        Lost();
    }
#elif defined(USE_OPENAL)
    /* nop */
#endif
}

// 0x4D7A10
bool CAEAudioChannel::Lost() const {
#ifdef USE_DSOUND
    while (m_pDirectSoundBuffer->Restore() == DSERR_BUFFERLOST) { // BUG: Infinite loop if we don't restore
        OS_ThreadSleep(10);
    }
#endif

    return true;
}

bool CAEAudioChannel::SetReverbAndDepth(uint32 reverb, uint32 depth) {
    return plugin::CallMethodAndReturn<bool, 0x4D7AA0>(this, reverb, depth);
}
