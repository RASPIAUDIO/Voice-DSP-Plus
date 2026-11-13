#ifdef __cplusplus
extern "C" {
#endif
#ifndef VOICEDSP_V2_GUARD_H
#define VOICEDSP_V2_GUARD_H
#include "voicedsp_v2_audio.h"
typedef bool (*vdsp_authorize_fn)(void *, vdsp_v2_audio_route);
/* Callback must refresh clock/IO/power, may not access DAC or change its page.
 * -5 cancels activation and invalidates the driver; explicit reinit is required.
 */
int vdsp_v2_audio_guarded_select(vdsp_v2_audio *, vdsp_v2_audio_route,
    VDSP_FPTR("vdsp_authorize") vdsp_authorize_fn, void *);
#endif

#ifdef __cplusplus
}
#endif
