#include "AEAudioChannel.h"
#include "AESmoothFadeThread.h"

// 0x4D7D00
void CAEAudioChannel::SetFrequencyScalingFactor(float factor) {
    if (factor == 0.0F) {
        if (m_pDirectSoundBuffer &&
            !m_bNoScalingFactor &&
            IsBufferPlaying() &&
            !AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, -100.0F, -1, true)
        ) {
            m_pDirectSoundBuffer->Stop();
        }

        m_bNoScalingFactor = true;
        return;
    }

    const auto newFreq = static_cast<uint32>(float(m_nOriginalFrequency) * factor);
    SetFrequency(newFreq);

    if (m_bNoScalingFactor) {
        if (m_pDirectSoundBuffer) {
            const auto curPos = GetCurrentPlaybackPosition();
            if (curPos != 0) {
                m_pDirectSoundBuffer->SetVolume(-10000);
            }

            m_pDirectSoundBuffer->Play(0, 0, m_bLooped);

            if (curPos != 0 && !AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, m_fVolume, -2, false)) {
                const auto volume = static_cast<LONG>(m_fVolume * 100.0F);
                m_pDirectSoundBuffer->SetVolume(volume);
            }
        }
        m_bNoScalingFactor = false;
    }
}
