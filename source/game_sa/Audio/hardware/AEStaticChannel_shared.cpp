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

    if (m_bNoScalingFactor || m_bNeedsSynch)
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
    m_bNoScalingFactor = scalingFactor == 0.0f;
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
