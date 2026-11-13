#include "voicedsp_v2_geometry.h"
#include <stddef.h>

/* Acoustic openings from acoustics-20261007.json. Do not use body origins.
 * QF60B pins 46/50/48/51 = PORT_8D bits 4/5/6/7 = DSP lanes 0/1/2/3.
 */
const vdsp_v2_positions *vdsp_v2_geometry(vdsp_v2_mic_array array)
{
    static const vdsp_v2_positions internal_square = {
        {-0.022f, 0.022f, 0}, {0.022f, 0.022f, 0},
        {0.022f, -0.022f, 0}, {-0.022f, -0.022f, 0}
    };
    static const vdsp_v2_positions external_square = {
        {0.022f, -0.022f, 0}, {-0.022f, -0.022f, 0},
        {-0.022f, 0.022f, 0}, {0.022f, 0.022f, 0}
    };
    static const vdsp_v2_positions external_linear = {
        {0.0495f, 0, 0}, {0.0165f, 0, 0}, {-0.0165f, 0, 0}, {-0.0495f, 0, 0}
    };
    switch (array) {
    case VDSP_MIC_INTERNAL_SQUARE: return &internal_square;
    case VDSP_MIC_EXTERNAL_SQUARE: return &external_square;
    case VDSP_MIC_EXTERNAL_LINEAR: return &external_linear;
    default: return NULL;
    }
}
