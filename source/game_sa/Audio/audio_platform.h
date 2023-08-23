#pragma once

#if defined(USE_DSOUND)
#include "dsound.h"
#elif defined(USE_OPENAL)
#include "OpenAL/OALBase.h"
#include "OpenAL/OALBuffer.h"
#include "OpenAL/OALSource.h"
#else
#error "Invalid audio platform option"
#endif

#define COMPATIBLE_STRUCT_SIZE // probably not necessary but hey
