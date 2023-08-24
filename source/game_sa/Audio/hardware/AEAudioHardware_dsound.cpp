#include "AEAudioHardware.h"

// NOTSA
bool CAEAudioHardware::InitDirectSound() {
    VERIFY(SUCCEEDED(CoInitialize(nullptr)));

    // TODO: We need EAX headers here.
    //
    // Though, EAX through DirectSound IS NOT SUPPORTED after Windows Vista since
    // Windows doesn't emulate EAX extensions.
    //
    // Wine or 3rd party DSOUND libraries may support it so we should have them
    // here.

    // if (FAILED(EAXDirectSoundCreate(&DSDEVID_DefaultPlayback, &m_pDSDevice, 0)))
    //    return false;

    if (FAILED(DirectSoundCreate8(&DSDEVID_DefaultPlayback, &m_pPlatformDevice, 0)))
        return false;

    m_dsCaps.dwSize = 96;
    m_pPlatformDevice->GetCaps(&m_dsCaps);

    if (FAILED(m_pPlatformDevice->SetCooperativeLevel(PSGLOBAL(window), DSSCL_PRIORITY)) || !InitDirectSoundListener(2, 48'000, 16))
        return false;

    m_pPlatformDevice->GetSpeakerConfig((LPDWORD)&m_nSpeakerConfig);

    m_pStreamingChannel = new CAEStreamingChannel(m_pPlatformDevice, 0);
    m_pStreamingChannel->Initialise();

    const uint32 freeHw3DAllBuffers = m_dsCaps.dwFreeHw3DAllBuffers;
    if (freeHw3DAllBuffers < 24) {
        m_nNumChannels = 48;
        AESmoothFadeThread.m_nNumAvailableBuffers = 48;
        field_4 = 0;
    } else {
        m_nNumChannels = std::min(freeHw3DAllBuffers, 64u) - 7;
        field_4 = 1;
        AESmoothFadeThread.m_nNumAvailableBuffers = 7;
    }

    return true;
}

// 0x4D9640
bool CAEAudioHardware::InitDirectSoundListener(uint32 numChannels, uint32 samplesPerSec, uint32 bitsPerSample) {
    if (!m_pPlatformDevice)
        return false;

    DSBUFFERDESC dsBuffDsc{
        sizeof(DSBUFFERDESC), DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER, 0, 0, 0,
    };
    LPDIRECTSOUNDBUFFER soundbuf;
    if (FAILED(m_pPlatformDevice->CreateSoundBuffer(&dsBuffDsc, &soundbuf, NULL))) {
        return false;
    }

    const auto nBlockAlign = numChannels * bitsPerSample / 8;
    WAVEFORMATEX wavFmt{.wFormatTag = 1,
                        .nChannels = (WORD)numChannels,
                        .nSamplesPerSec = samplesPerSec,
                        .nAvgBytesPerSec = samplesPerSec * nBlockAlign,
                        .nBlockAlign = (WORD)nBlockAlign,
                        .wBitsPerSample = (WORD)bitsPerSample,
                        .cbSize = 0};
    if (FAILED(soundbuf->SetFormat(&wavFmt))) {
#ifdef FIX_BUGS
        soundbuf->Release();
#endif
        return false;
    }

    auto& listener = m_pDirectSound3dListener;
    if (FAILED(soundbuf->QueryInterface(IID_IDirectSound3DListener, (LPVOID*)&listener))) {
        soundbuf->Release();
        return false;
    }

    listener->SetPosition(0.f, 0.f, 0.f, TRUE);
    listener->SetOrientation(0.f, 1.f, 0.f,  // In Front
                             0.f, 0.f, -1.f, // Bottom
                             TRUE);
    listener->SetRolloffFactor(0.f, TRUE);
    listener->SetDopplerFactor(0.f, TRUE);
    listener->CommitDeferredSettings();

    soundbuf->Release();

    Query3DSoundEffects();

    return true;
}

// 0x4D8490
void CAEAudioHardware::Query3DSoundEffects() {
    DSBUFFERDESC bufferDesc;
    bufferDesc.guid3DAlgorithm = GUID_NULL;
    bufferDesc.lpwfxFormat = nullptr;
    bufferDesc.dwFlags = DSBCAPS_CTRL3D;
    bufferDesc.dwReserved = 0;
    bufferDesc.dwSize = sizeof(DSBUFFERDESC);
    bufferDesc.dwBufferBytes = 1024;

    IDirectSoundBuffer* ppDSBuffer{};
    if (FAILED(m_pPlatformDevice->CreateSoundBuffer(&bufferDesc, &ppDSBuffer, 0)) || !ppDSBuffer)
        return;

    IDirectSound3DBuffer* ppDS3DBuffer{};
    if (FAILED(ppDSBuffer->QueryInterface(IID_IDirectSound3DBuffer, (LPVOID*)&ppDS3DBuffer)) || !ppDS3DBuffer)
        ppDSBuffer->Release();

    IKsPropertySet* ppIKsPropertySet{};
    if (ppDS3DBuffer->QueryInterface(IID_IKsPropertySet, (LPVOID*)&ppIKsPropertySet) == S_OK) {
        // TODO: EAX (0x4D8593)
        m_n3dEffectsQueryResult = 1;
        SAFE_RELEASE(ppIKsPropertySet);
    }
    if (!m_n3dEffectsQueryResult)
        m_n3dEffectsQueryResult = 2;

    SAFE_RELEASE(ppDS3DBuffer);
    SAFE_RELEASE(ppDSBuffer);
}
