#pragma once

#include "audio_platform.h"

enum eSmoothFadeEntryStatus : uint32 {
    STATE_INACTIVE = 0,
    STATE_ACTIVE = 1,
    STATE_CANCELLED = 2,
};

struct tSmoothFadeEntry {
#if defined(USE_DSOUND)
    IDirectSoundBuffer*    m_pSource;
#elif defined(USE_OPENAL)
    OALSource*             m_pSource;
#endif
    float                  m_fStartVolume;
    float                  m_fTargetVolume;
    float                  m_fVolumeDiff;
    float                  m_fCurVolume;
    bool                   m_bStopBufferAfterFade;
    char                   _pad;
    uint16                 m_wFadeTime;
    eSmoothFadeEntryStatus m_nStatus;
    uint32                 m_nStartTime;

    void SetInactive() {
        m_nStatus = eSmoothFadeEntryStatus::STATE_INACTIVE;
#if defined(USE_DSOUND)
        m_pSource = nullptr;
#elif defined(USE_OPENAL)
        m_pSource = nullptr;
#endif
    }
};
VALIDATE_SIZE(tSmoothFadeEntry, 0x20);

class CAESmoothFadeThread {
public:
    static constexpr int32 NUM_SMOOTHFADE_ENTRIES = 64;

    HANDLE           m_threadHandle;
    DWORD            m_dwThreadId;
    // TODO: ^^^^ std::thread impl
    tSmoothFadeEntry m_aEntries[NUM_SMOOTHFADE_ENTRIES];
    bool             m_bThreadCreated;
    bool             m_bActive;
    uint16           m_wUnkn;
    uint16           m_wUnkn2;
    uint16           m_wUnkn3;
    uint32           m_nLastServiceTime;
    bool             m_bThreadInvalid;
    char             _pad2[3];
    uint32           m_nNumAvailableBuffers;

public:
    static void InjectHooks();

    CAESmoothFadeThread();

    void Initialise();
    void InitialiseRequestSlots();
    void Start();
    void Stop();
    void WaitForExit();
    void Service();

    void CancelFade(void* sourceBuffer);
    bool RequestFade(void* sourceBuffer, float fTargetVolume, int16 fadeTime, bool bStopBufferAfterFade);
    void SetBufferVolume(void* sourceBuffer, float volume);

    static DWORD WINAPI SmoothFadeProc(void* smoothFade);
};
VALIDATE_SIZE(CAESmoothFadeThread, 0x81C);

static inline CAESmoothFadeThread& AESmoothFadeThread = *(CAESmoothFadeThread*)0xB608D0;
