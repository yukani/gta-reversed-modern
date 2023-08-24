#include "StdInc.h"
#include "AESmoothFadeThread.h"

#include "AEAudioChannel.h"
#include "AEAudioUtility.h"

void CAESmoothFadeThread::InjectHooks() {
    RH_ScopedClass(CAESmoothFadeThread);
    RH_ScopedCategory("Audio");

    RH_ScopedInstall(Start, 0x4EEA10);
    RH_ScopedInstall(Stop, 0x4EEA30);
    RH_ScopedInstall(WaitForExit, 0x4EEA40);
    RH_ScopedInstall(Initialise, 0x4EEEC0);
    RH_ScopedInstall(InitialiseRequestSlots, 0x4EE9F0);
    RH_ScopedInstall(CancelFade, 0x4EEA50);
    RH_ScopedInstall(SetBufferVolume, 0x4EEC80);
    RH_ScopedInstall(SmoothFadeProc, 0x4EEE90);
    RH_ScopedInstall(RequestFade, 0x4EEAA0);
    RH_ScopedInstall(Service, 0x4EED10);
}

CAESmoothFadeThread::CAESmoothFadeThread() {
    m_threadHandle         = INVALID_HANDLE_VALUE;
    m_dwThreadId           = 0;
    m_bThreadCreated       = false;
    m_bActive              = false;
    m_wUnkn                = 0;
    m_wUnkn2               = 0;
    m_nLastServiceTime     = 0;
    m_nNumAvailableBuffers = 0;
}

void CAESmoothFadeThread::Initialise() {
    InitialiseRequestSlots();
    m_threadHandle = CreateThread(nullptr, 0, &CAESmoothFadeThread::SmoothFadeProc, this, CREATE_SUSPENDED, &m_dwThreadId);
    if (m_threadHandle == INVALID_HANDLE_VALUE || !m_threadHandle) { // NOTSA: nullptr check
        m_bThreadCreated = false;
    } else {
        SetThreadPriority(m_threadHandle, 0);
        m_bThreadCreated = true;
        m_bThreadInvalid = false;
    }
}

void CAESmoothFadeThread::InitialiseRequestSlots() {
    rng::for_each(m_aEntries, &tSmoothFadeEntry::SetInactive);
}

void CAESmoothFadeThread::Start() {
    m_bActive = true;
    ResumeThread(m_threadHandle);
}

void CAESmoothFadeThread::Stop() {
    m_bActive = false;
}

void CAESmoothFadeThread::WaitForExit() {
    WaitForSingleObject(m_threadHandle, -1);
}

void CAESmoothFadeThread::Service() {
    m_nLastServiceTime = (uint32)CAEAudioUtility::GetCurrentTimeInMS();
    for (auto& entry : m_aEntries) {
        if (entry.m_nStatus == eSmoothFadeEntryStatus::STATE_INACTIVE)
            continue;

        if (entry.m_nStatus == eSmoothFadeEntryStatus::STATE_CANCELLED) {
            --g_numSoundChannelsUsed;
            if (entry.m_pSource)
                entry.m_pSource->Release();

            entry.SetInactive();
            continue;
        }

        if (!entry.m_pSource)
            continue;

        if (m_nLastServiceTime < entry.m_nStartTime)
            continue;

        const auto elapsed = m_nLastServiceTime - entry.m_nStartTime;
        if (elapsed < entry.m_wFadeTime) {
            const auto fStep = pow(10.0F, entry.m_fVolumeDiff / 20.0F) - 1.0F;
            const auto fProgress = static_cast<float>(elapsed) / static_cast<float>(entry.m_wFadeTime);
            entry.m_fCurVolume = entry.m_fStartVolume + LOG10_2 * log2(fStep * fProgress) * 20.0F;

#if defined(USE_DSOUND)
            const auto dwVolume = static_cast<LONG>(entry.m_fCurVolume * 100.0F);
            entry.m_pSource->SetVolume(dwVolume);
#elif defined(USE_OPENAL)
            entry.m_pSource->SetVolume(entry.m_fCurVolume);
#endif
            continue;
        }

#if defined(USE_DSOUND)
        int32 curVolume;
        entry.m_pSource->GetVolume((LPLONG)&curVolume);
        const auto fCurVolume = static_cast<float>(curVolume) / 100.0F;
#elif defined(USE_OPENAL)
        const auto fCurVolume = entry.m_pSource->GetVolume();
#endif        
        if (entry.m_bStopBufferAfterFade || approxEqual2(fCurVolume, entry.m_fCurVolume, 0.01F)) {
#if defined(USE_DSOUND)
            const auto dwTargetVolume = static_cast<LONG>(entry.m_fTargetVolume * 100.0F);
            entry.m_pSource->SetVolume(dwTargetVolume);
#elif defined(USE_OPENAL)
            entry.m_pSource->SetVolume(entry.m_fTargetVolume);
#endif
            if (entry.m_bStopBufferAfterFade)
                entry.m_pSource->Stop();
        }

        --g_numSoundChannelsUsed;
        if (entry.m_pSource)
            entry.m_pSource->Release();

        entry.m_pSource = nullptr;
        entry.m_nStatus = eSmoothFadeEntryStatus::STATE_INACTIVE;
    }
}

void CAESmoothFadeThread::CancelFade(void* sourceBuffer) {
    if (m_bThreadInvalid)
        return;

    for (auto& entry : m_aEntries)
        if (entry.m_nStatus == eSmoothFadeEntryStatus::STATE_ACTIVE && entry.m_pSource == sourceBuffer)
            entry.m_nStatus = eSmoothFadeEntryStatus::STATE_CANCELLED;
}

bool CAESmoothFadeThread::RequestFade(void* sourceBuffer, float fTargetVolume, int16 fadeTime, bool bStopBufferAfterFade) {
    if (!m_bThreadCreated || m_bThreadInvalid)
        return false;

#if defined(USE_DSOUND)
    auto* buffer = reinterpret_cast<IDirectSoundBuffer*>(sourceBuffer);
#elif defined(USE_OPENAL)
    auto* buffer = reinterpret_cast<OALSource*>(sourceBuffer);
#endif

#if defined(USE_DSOUND)
    uint32 status;
    buffer->GetStatus((LPDWORD)&status);
    if (!(status & DSBSTATUS_PLAYING))
        return false;
#elif defined(USE_OPENAL)
    if (buffer->m_currentState != AL_PLAYING)
        return false;
#endif

#if defined(USE_DSOUND)
    int32 curVolume;
    buffer->GetVolume((LPLONG)&curVolume);
    const auto fCurVolume = static_cast<float>(curVolume) / 100.0F;
#elif defined(USE_OPENAL)
    const auto fCurVolume = buffer->GetVolume();
#endif
    if (approxEqual2(fCurVolume, fTargetVolume, 0.01F)) {
        if (bStopBufferAfterFade)
            buffer->Stop();

        return true;
    }

    bool bFound = false;
    int32 iFreeInd = -1;
    for (uint32 i = 0; i < m_nNumAvailableBuffers; ++i) {
        auto& sound = m_aEntries[i];
        if (sound.m_nStatus == eSmoothFadeEntryStatus::STATE_INACTIVE && !bFound) {
            bFound = true;
            iFreeInd = i;
            continue;
        }

        if (sound.m_nStatus != eSmoothFadeEntryStatus::STATE_ACTIVE)
            continue;

        if (sound.m_pSource != buffer)
            continue;

        if (fadeTime + sound.m_nStartTime - m_nLastServiceTime <= 1)
            continue;

        bFound = true;
        iFreeInd = i;
        break;
    }

    if (!bFound)
        return false;

    auto& sound = m_aEntries[iFreeInd];
    if (sound.m_nStatus == eSmoothFadeEntryStatus::STATE_ACTIVE) {
        if (sound.m_bStopBufferAfterFade)
            return true;
    } else {
        ++g_numSoundChannelsUsed;
        buffer->AddRef();
        sound.m_pSource = buffer;
    }

    sound.m_fTargetVolume        = fTargetVolume;
    sound.m_fCurVolume           = fCurVolume;
    sound.m_fStartVolume         = fCurVolume;
    sound.m_fVolumeDiff          = fTargetVolume - fCurVolume;
    sound.m_bStopBufferAfterFade = bStopBufferAfterFade;

#if defined(USE_DSOUND)
    if (fadeTime == -2)
        sound.m_wFadeTime = (status & DSBSTATUS_LOCHARDWARE) ? 20 : 28;
    else if (fadeTime == -1)
        sound.m_wFadeTime = (status & DSBSTATUS_LOCHARDWARE) ? 20 : 30;
    else
        sound.m_wFadeTime = fadeTime;
#elif defined(USE_OPENAL)
    if (fadeTime == -2 || fadeTime == -1)
        sound.m_wFadeTime = (fadeTime == -2) ? 28 : 30;
    else
        sound.m_wFadeTime = fadeTime;
#endif

    sound.m_nStartTime = m_nLastServiceTime;
    sound.m_nStatus = eSmoothFadeEntryStatus::STATE_ACTIVE;

    return true;
}

void CAESmoothFadeThread::SetBufferVolume(void* sourceBuffer, float volume) {
#if defined(USE_DSOUND)
    auto* buffer = reinterpret_cast<IDirectSoundBuffer*>(sourceBuffer);
    const auto newVolume = static_cast<LONG>(volume * 100.0F);
#elif defined(USE_OPENAL)
    auto* buffer = reinterpret_cast<OALSource*>(sourceBuffer);
    const auto newVolume = volume;
#endif
    if (m_bThreadInvalid) {
        buffer->SetVolume(newVolume);
        return;
    }

    int32 entryInd;
    for (entryInd = 0; entryInd < NUM_SMOOTHFADE_ENTRIES; ++entryInd) {
        auto& entry = m_aEntries[entryInd];
        if (entry.m_nStatus == eSmoothFadeEntryStatus::STATE_ACTIVE && entry.m_pSource == buffer)
            break;
    }

    if (entryInd >= NUM_SMOOTHFADE_ENTRIES)
        buffer->SetVolume(newVolume);
    else {
        auto& entry = m_aEntries[entryInd];
        if (entry.m_bStopBufferAfterFade || approxEqual2(volume, entry.m_fTargetVolume, 0.01F))
            return;

        entry.m_nStatus = eSmoothFadeEntryStatus::STATE_CANCELLED;
        buffer->SetVolume(newVolume);
    }
}

DWORD WINAPI CAESmoothFadeThread::SmoothFadeProc(void* smoothFade) {
#ifdef TRACY_ENABLE
    tracy::SetThreadName("AESmoothFadeThread");
#endif

    auto* fade = static_cast<CAESmoothFadeThread*>(smoothFade);
    while (fade->m_bActive) {
        ZoneScoped;

        fade->Service();
        Sleep(1);
    }

    return 0;
}
