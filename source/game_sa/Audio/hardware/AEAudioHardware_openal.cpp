#include "StdInc.h"
#include <AL/alc.h>
#include "AEAudioHardware.h"
#include "oswrapper.h"

extern inline void* s_oalTrashMutex{nullptr};

void CAEAudioHardware::InitOpenALListener() {
    static constexpr ALenum ALC_CONTEXT_ATTRS[] = {ALC_FREQUENCY, /* 0x5622, */ 0}; // what the fuck is 0x5622?

    VERIFY(m_pPlatformDevice = alcOpenDevice(nullptr));
    VERIFY(m_alContext = alcCreateContext(m_pPlatformDevice, ALC_CONTEXT_ATTRS));
    alcMakeContextCurrent(m_alContext);
    alListenerf(AL_GAIN, 0.5f);
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);

    CVector orient{0.0f, 1.0f, 0.0f};
    alListenerfv(AL_ORIENTATION, reinterpret_cast<float*>(&orient));
    alDistanceModel(AL_NONE);

    s_oalTrashMutex = OS_MutexCreate("OALTrash");
}
