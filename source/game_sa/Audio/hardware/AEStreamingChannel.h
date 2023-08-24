#pragma once

#include "AEAudioChannel.h"
#include "AEStreamingDecoder.h"
#include "AESmoothFadeThread.h"

enum class StreamingChannelState : int32 {
    UNK_MINUS_7 = -7,
    UNK_MINUS_6 = -6,
    UNK_MINUS_5 = -5,
    UNK_MINUS_4 = -4,
    UNK_MINUS_3 = -3,
    UNK_MINUS_2 = -2,
    UNK_MINUS_1 = -1,
};

class NOTSA_EXPORT_VTABLE CAEStreamingChannel : public CAEAudioChannel {
public:
    bool                  m_bInitialized{false};
    uint8                 field_61{0u};
    bool                  m_bPrepareNewStream{false};
    uint8                 m_bWrongSampleRate{false};
    uint8                 field_64{0u};
    uint8                 m_bFxEnabled{false};
    uint8                 m_nCurrentlyLoadedChunk{0u};
    uint8                 field_67{0u};
    void*                 m_pBuffer{nullptr};
    uint8                 m_aBuffer[0x60000];
    CAEStreamingDecoder*  m_pStreamingDecoder{nullptr};
    CAEStreamingDecoder*  m_pNextStreamingDecoder{nullptr};
    StreamingChannelState m_nState{StreamingChannelState::UNK_MINUS_6};
    uint32                m_nStreamPlayTimeMs{0u};
    uint32                m_nPlayTime;
    uint32                field_60084;
    int32                 field_60088{0u};
    uint64                m_nLastUpdateTime;
#if defined(USE_DSOUND)
    IDirectSoundBuffer* m_pSilenceBuffer;
#elif defined(USE_OPENAL)
#ifdef COMPATIBLE_STRUCT_SIZE
private:
    uint8 _pad1[sizeof(void*)];
public:
#endif
#endif
    float                 m_fSomething{1.0f};

public:
    CAEStreamingChannel(void* platform, uint16 channelId)
        : CAEAudioChannel(platform, channelId, 48000, 16)
    {} // 0x4F1800
    ~CAEStreamingChannel() override;

    void   Service() override;
    bool   IsSoundPlaying() override;
    int16  GetPlayTime() override;
    uint16 GetLength() override;
    void   Play(int16, int8, float) override;
    void   SynchPlayback() override;
    void   Stop() override;
    void   SetFrequencyScalingFactor(float factor) override;

    void  Initialise();
    void  InitialiseSilence();

#ifdef USE_DSOUND
    bool AddFX();
    void RemoveFX();
#endif

    int32 UpdatePlayTime();

    int32  GetActiveTrackID();
    int32  GetPlayingTrackID();
    void   SetNextStream(CAEStreamingDecoder* decoder);
    uint32 FillBuffer(void* buffer, uint32 size);
    void   SetBassEQ(uint8 mode, float gain);
    void   SetReady();
    void   PrepareStream(CAEStreamingDecoder* stream, int8 arg2, uint32 audioBytes);
    void   Pause();

private:
    friend void InjectHooksMain();
    static void InjectHooks();

    CAEStreamingChannel* Constructor(void* platform, uint16 channelId) {
        this->CAEStreamingChannel::CAEStreamingChannel(platform, channelId);
        return this;
    }

    void Destructor() {
        CAEStreamingChannel::~CAEStreamingChannel();
    }

    void Service_Reversed() {
        CAEStreamingChannel::Service();
    }

    bool IsSoundPlaying_Reversed() {
        return CAEStreamingChannel::IsSoundPlaying();
    }

    int32 GetPlayTime_Reversed() {
        return CAEStreamingChannel::GetPlayTime();
    }

    int32 GetLength_Reversed() {
        return CAEStreamingChannel::GetLength();
    }

    void Play_Reversed(int16 a2, char a3, float a4) {
        CAEStreamingChannel::Play(a2, a3, a4);
    }

    void Stop_Reversed() {
        CAEStreamingChannel::Stop();
    }

    void SetFrequencyScalingFactor_Reversed(float a2) {
        CAEStreamingChannel::SetFrequencyScalingFactor(a2);
    }

    // NOTSA
    void FadeToSilence() {
#if defined(USE_DSOUND)
        if (!AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, -100.0f, 35, true))
            m_pDirectSoundBuffer->Stop();
#elif defined(USE_OPENAL)
        if (!AESmoothFadeThread.RequestFade(m_pSource, -100.0f, 35, true))
            m_pSource->Stop();
#endif
    }
};

VALIDATE_SIZE(CAEStreamingChannel, 0x60098);
