/* Keep exact accepted profile values; never invoke SHF from an RTOS worker. */
static void vdsp_usb_apply_aec(void)
{
    unsigned p = vdsp_usb_profile_requested;
    if (p == vdsp_usb_profile_applied || (p != 4 && p != 6)) return;
    float gain = vdsp_usb_far_gain(p), readback = 999.0f;
    SHF_SetPar_AEC(5, &gain);
    SHF_GetPar_AEC(5, &readback);
    if (readback == gain) vdsp_usb_profile_applied = p;
}
static void vdsp_usb_apply_pp(void)
{
    unsigned p = vdsp_usb_profile_requested;
    if (p == vdsp_usb_profile_applied || (p != 4 && p != 6)) return;
    const unsigned cmds[] = {21,31,10,13,23,27,22};
    union { float f; int32_t i; } values[7];
    values[0].f = p == 4 ? .10f : .15f;
    values[1].i = p == 4 ? 10 : 0;
    values[2].i = p == 4 ? 0 : 1;
    values[3].f = 5.0f;
    values[4].i = p == 4 ? 0 : 1;
    values[5].i = 1;
    values[6].f = .51f;
    for (unsigned i = 0; i < 7; ++i) {
        if (i == 3 && p == 6) continue; /* Internal AGC remains adaptive. */
        SHF_SetPar_PP(cmds[i], &values[i]);
    }
    bool good = true;
    for (unsigned i = 0; i < 7; ++i) {
        if (i == 3 && p == 6) continue; /* Only the internal AGC is adaptive. */
        uint32_t got = 0, expected = 0;
        memcpy(&expected, &values[i], 4);
        SHF_GetPar_PP(cmds[i], &got);
        if (got != expected) good = false;
    }
    if (good) vdsp_usb_profile_applied = p;
}
