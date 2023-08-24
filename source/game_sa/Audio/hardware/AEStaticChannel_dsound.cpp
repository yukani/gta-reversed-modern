#include "AEStaticChannel.h"
#include "Timer.h"

// 0x4F10D0
void CAEStaticChannel::Service() {
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

    UpdateStatus();

    if (!m_bNoScalingFactor && !bufferStatus.Bit0x1) {
        if (const auto buf = std::exchange(m_pDirectSoundBuffer, nullptr)) {
            --g_numSoundChannelsUsed;
            buf->Release();
        }
    }
}

// 0x4F1040
void CAEStaticChannel::SynchPlayback() {
    if (!m_pDirectSoundBuffer || !m_bNeedsSynch || m_bNoScalingFactor)
        return;

    if (m_bUnkn2) {
        m_pDirectSoundBuffer->SetVolume(-10000);
        if (!AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, m_fVolume, -2, false)) {
            const auto dwVolume = static_cast<LONG>(m_fVolume * 100.0F);
            m_pDirectSoundBuffer->SetVolume(dwVolume);
        }
    }

    m_pDirectSoundBuffer->Play(0, 0, m_bLooped);
    m_nSyncTime = CTimer::GetTimeInMS();
    m_bNeedsSynch = false;
}

// 0x4F0FB0
void CAEStaticChannel::Stop() {
    if (m_pDirectSoundBuffer && CAEAudioChannel::IsBufferPlaying() && !AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, -100.0F, -1, true)) {
        m_pDirectSoundBuffer->Stop();
    }

    { // todo: Same as CAEAudioChannel::~CAEAudioChannel
        if (m_pDirectSoundBuffer) {
            --g_numSoundChannelsUsed;
            m_pDirectSoundBuffer->Release();
            m_pDirectSoundBuffer = nullptr;
        }

        if (m_pDirectSound3DBuffer) {
            m_pDirectSound3DBuffer->Release();
            m_pDirectSound3DBuffer = nullptr;
        }
    }
}

// 0x4F0C40
bool CAEStaticChannel::SetAudioBuffer(IDirectSound3DBuffer* buffer, uint16 size, int16 f88, int16 f8c, int16 loopOffset, uint16 frequency) {
    return false;
}
