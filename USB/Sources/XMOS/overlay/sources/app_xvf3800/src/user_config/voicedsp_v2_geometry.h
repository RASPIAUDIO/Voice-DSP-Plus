#ifndef VOICEDSP_V2_GEOMETRY_H
#define VOICEDSP_V2_GEOMETRY_H
#include "voicedsp_v2_control.h"

typedef float vdsp_v2_positions[4][3];
/* Metres, native CAD XY, DSP lanes D0/D1/D2/D3. NULL means unknown. */
const vdsp_v2_positions *vdsp_v2_geometry(vdsp_v2_mic_array array);
#endif
