#include "StdInc.h"

#include <extensions/utility.hpp>

#include "AEStreamingChannel.h"
#include "AEAudioUtility.h"

// 0x4F2200
CAEStreamingChannel::~CAEStreamingChannel() {
    FadeToSilence();

    m_nState = StreamingChannelState::UNK_MINUS_5;
    field_60088 = 0;

#if defined(USE_DSOUND)
    if (m_pDirectSoundBuffer8)
        m_pDirectSoundBuffer8->SetFX(0, nullptr, nullptr);

    if (m_pSilenceBuffer)
        m_pSilenceBuffer->Release();

    if (m_pDirectSoundBuffer)
        m_pDirectSoundBuffer->Release();
#elif defined(USE_OPENAL)
    if (m_pSource) {
        m_pSource->Release();
        m_pSource = nullptr;
    }
#endif

    if (m_pStreamingDecoder)
        delete m_pStreamingDecoder;

    if (m_pNextStreamingDecoder)
        delete m_pNextStreamingDecoder;
}

// 0x4F1FF0
void CAEStreamingChannel::SetReady() {
    switch (m_nState) {
    case StreamingChannelState::UNK_MINUS_5:
#if defined(USE_DSOUND)
        AESmoothFadeThread.CancelFade(m_pDirectSoundBuffer);
        m_pDirectSoundBuffer->Stop();
#elif defined(USE_OPENAL)
        AESmoothFadeThread.CancelFade(m_pSource);
        m_pSource->Stop();
#endif
        m_nState = StreamingChannelState::UNK_MINUS_6;

        break;
    case StreamingChannelState::UNK_MINUS_6:
        if (m_pStreamingDecoder)
            m_nState = StreamingChannelState::UNK_MINUS_2;
        break;
    }
}

// 0x4F1870
void CAEStreamingChannel::SynchPlayback() {
    // NOP
}

// 0x4F1E20
uint32 CAEStreamingChannel::FillBuffer(void* buffer, uint32 size) {
    auto filled = m_pStreamingDecoder->FillBuffer(buffer, size);
    if (filled < size) {
        if (field_61) {
            while (filled < size) {
                m_pStreamingDecoder->SetCursor(1);
                filled += m_pStreamingDecoder->FillBuffer((uint8*)buffer + filled, size - filled);
            }
        } else {
            if (m_pNextStreamingDecoder) {
                if (m_pNextStreamingDecoder->GetSampleRate() == m_pStreamingDecoder->GetSampleRate()) {
                    if (m_pStreamingDecoder)
                        delete m_pStreamingDecoder;

                    m_pStreamingDecoder = m_pNextStreamingDecoder;
                    m_pNextStreamingDecoder = nullptr;

                    filled += m_pStreamingDecoder->FillBuffer((uint8*)buffer + filled, size - filled);
                } else {
                    memset((uint8*)buffer + filled, 0, size - filled);
                    filled = size;
                    m_bWrongSampleRate = true;
                }
            } else {
                m_nState = StreamingChannelState::UNK_MINUS_4;
            }
        }
    }

    m_nStreamPlayTimeMs = m_pStreamingDecoder->GetStreamLengthMs();
    return filled;
}

// 0x4F1DE0
void CAEStreamingChannel::SetNextStream(CAEStreamingDecoder* decoder) {
    if (m_pNextStreamingDecoder) {
        delete m_pNextStreamingDecoder;
    }
    m_pNextStreamingDecoder = decoder;
}

// 0x4F1A60
int32 CAEStreamingChannel::GetPlayingTrackID() {
    switch (m_nState) {
    case StreamingChannelState::UNK_MINUS_3:
    case StreamingChannelState::UNK_MINUS_4:
    case StreamingChannelState::UNK_MINUS_1:
    case StreamingChannelState::UNK_MINUS_5:
        if (m_pStreamingDecoder)
            return m_pStreamingDecoder->GetStreamID();

        [[fallthrough]];
    default:
        return -1;
    }
}

// 0x4F1A40
int32 CAEStreamingChannel::GetActiveTrackID() {
    return m_pStreamingDecoder ? m_pStreamingDecoder->GetStreamID() : -1;
}

// 0x4F18A0
int32 CAEStreamingChannel::UpdatePlayTime() {
    return plugin::CallMethodAndReturn<int32, 0x4F18A0, CAEStreamingChannel*>(this);
}

// 0x4F2040
bool CAEStreamingChannel::IsSoundPlaying() {
    switch (m_nState) {
    case StreamingChannelState::UNK_MINUS_3:
    case StreamingChannelState::UNK_MINUS_4:
    case StreamingChannelState::UNK_MINUS_5:
    case StreamingChannelState::UNK_MINUS_1:
        return true;
    default:
        return false;
    }
}

// 0x4F19E0
int16 CAEStreamingChannel::GetPlayTime() {
    switch (m_nState) {
    case StreamingChannelState::UNK_MINUS_1:
    case StreamingChannelState::UNK_MINUS_3:
        if (CAEAudioUtility::GetCurrentTimeInMS() - m_nLastUpdateTime >= 1000u)
            return UpdatePlayTime();
        else
            return static_cast<int16>(m_nPlayTime - static_cast<uint32>(m_nLastUpdateTime) + CAEAudioUtility::GetCurrentTimeInMS());
        break;
    default:
        break;
    }

    return static_cast<int16>(m_nState); // possibly not intended.
}

// 0x4F1880
uint16 CAEStreamingChannel::GetLength() {
    return static_cast<uint16>(m_pStreamingDecoder ? m_pStreamingDecoder->GetStreamLengthMs() : -1);
}

// 0x4F1D40
void CAEStreamingChannel::Play(int16, int8 a3, float) {
    if (!m_pStreamingDecoder)
        return;

#if defined(USE_DSOUND)
    if (!m_pDirectSoundBuffer)
#elif defined(USE_OPENAL)
    if (!m_pSource)
#endif
        return;

    SetOriginalFrequency(m_pStreamingDecoder->GetSampleRate());

    if (m_nState != StreamingChannelState::UNK_MINUS_7)
        field_61 = a3;

    if (m_nState == StreamingChannelState::UNK_MINUS_5) {
#if defined(USE_DSOUND)
        AESmoothFadeThread.CancelFade(m_pDirectSoundBuffer);
        m_pDirectSoundBuffer->Stop();
#elif defined(USE_OPENAL)
        AESmoothFadeThread.CancelFade(m_pSource);
        m_pSource->Stop();
#endif

        m_nState = StreamingChannelState::UNK_MINUS_6;
    }

    m_nState = StreamingChannelState::UNK_MINUS_1;
#if defined(USE_DSOUND)
    m_pDirectSoundBuffer->SetVolume(static_cast<int32>(m_fVolume * 100.0f));
    m_pDirectSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
#elif defined(USE_OPENAL)
    m_pSource->SetVolume(m_fVolume);
    m_pSource->Play();
#endif
}

// 0x4F2170
void CAEStreamingChannel::Pause() {    
#if defined(USE_DSOUND)
    if (!m_pDirectSoundBuffer)
#elif defined(USE_OPENAL)
    if (!m_pSource)
#endif
        return;

    if (!IsBufferPlaying())
        return;

    FadeToSilence();
    m_nState = StreamingChannelState::UNK_MINUS_7;
}

// 0x4F1A90
void CAEStreamingChannel::Stop() {
    FadeToSilence();

    // SA: dead code
    if (false) {
        m_nState = StreamingChannelState::UNK_MINUS_5;
        field_60088 = 0;
    }
}

// 0x4F2550
void CAEStreamingChannel::Service() {
    plugin::CallMethod<0x4F2550, CAEStreamingChannel*>(this);
}

void CAEStreamingChannel::InjectHooks() {
    RH_ScopedClass(CAEStreamingChannel);
    RH_ScopedCategory("Audio/Hardware");

    RH_ScopedInstall(Constructor, 0x4F1800, { .reversed = false }); // makes game not load radio
    RH_ScopedInstall(Destructor, 0x4F2200);
    RH_ScopedInstall(SynchPlayback, 0x4F1870);
    RH_ScopedInstall(PrepareStream, 0x4F23D0, { .reversed = false });
    RH_ScopedInstall(Initialise, 0x4F22F0);
    RH_ScopedInstall(Pause, 0x4F2170);
    RH_ScopedInstall(SetReady, 0x4F1FF0);
    RH_ScopedInstall(SetBassEQ, 0x4F1F30);
    RH_ScopedInstall(FillBuffer, 0x4F1E20);
    RH_ScopedInstall(InitialiseSilence, 0x4F1C70);
    RH_ScopedInstall(SetNextStream, 0x4F1DE0);
    RH_ScopedInstall(Stop, 0x4F1A90);
    RH_ScopedInstall(GetPlayingTrackID, 0x4F1A60);
    RH_ScopedInstall(GetActiveTrackID, 0x4F1A40);
    RH_ScopedInstall(UpdatePlayTime, 0x4F18A0, { .reversed = false });
#ifdef USE_DSOUND
    RH_ScopedInstall(AddFX, 0x4F1AE0);
    RH_ScopedInstall(RemoveFX, 0x4F1C20);
#endif
    RH_ScopedVirtualInstall(Service, 0x4F2550, { .reversed = false });
    RH_ScopedVirtualInstall(IsSoundPlaying, 0x4F2040);
    RH_ScopedVirtualInstall(GetPlayTime, 0x4F19E0);
    RH_ScopedVirtualInstall(GetLength, 0x4F1880);
    RH_ScopedVirtualInstall(Play, 0x4F1D40);
    // RH_ScopedVirtualOverloadedInstall(Stop, "", 0x4F21C0, int8(CAEStreamingChannel::*)()); <-- unused, maybe inlined?
    RH_ScopedVirtualInstall(SetFrequencyScalingFactor, 0x4F2060);
}
