#include "StdInc.h"
#include "AEStaticChannel.h"

bool CAEStaticChannel::SetAudioBuffer(void* sourceBuffer, uint16 size, int16 f88, int16 f8c, int16 loopOffset, uint16 frequency) {
    NOTSA_UNREACHABLE("CAEStaticChannel::SetAudioBuffer: OpenAL stub (sourceBuffer={}, size={}, f88={}, f8c={}, loopOffset={}, freq={})", LOG_PTR(sourceBuffer), size, f88, f8c, loopOffset, frequency);
}
