#include "StdInc.h"

#include <extensions/utility.hpp>

#include "AEStreamingChannel.h"
#include "AEAudioUtility.h"

void CAEStreamingChannel::Initialise() {
    SetOriginalFrequency(22050); // 22050?
    m_bLooped = false;
    m_pSource = new OALSource;
    m_pSource->SetStream();
}

void CAEStreamingChannel::InitialiseSilence() {
    NOTSA_LOG_WARN("InitialiseSilence is no-op in OpenAL!");
}

void CAEStreamingChannel::SetBassEQ(uint8 mode, float gain) {
    NOTSA_LOG_WARN("SetBassEQ is no-op in OpenAL! (mode={}, gain={})", mode, gain);
}
