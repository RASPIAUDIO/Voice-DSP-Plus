/* Raw bounded transfers for PAJ's STOP-separated register reads. Included in
 * the bit-bang implementation to reuse its arbitration/timeout handling. */
int vdsp_usb_i2c_transfer(vdsp_v2_i2c *b, uint8_t address,
                         const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr)
{
    if (!b || address > 127 || (nt && !tx) || (nr && !rx) || (nt && nr) || nr > 2 || nt > 2)
        return VDSP_I2C_ARGUMENT;
    int rc = start(b, false);
    bool started = !rc;
    if (!rc) rc = write_byte(b, (uint8_t)((address << 1) | (nr ? 1 : 0)));
    for (size_t i = 0; !rc && i < nt; ++i) rc = write_byte(b, tx[i]);
    for (size_t i = 0; !rc && i < nr; ++i) {
        uint8_t value = 0;
        release(b, VDSP_SDA);
        for (int bit = 7; !rc && bit >= 0; --bit) {
            delay(b);
            rc = clock_high(b);
            if (!rc && level(b, VDSP_SDA)) value |= (uint8_t)(1u << bit);
            low(b, VDSP_SCL);
            delay(b);
        }
        if (rc) break;
        if (i + 1 < nr) low(b, VDSP_SDA); else release(b, VDSP_SDA);
        delay(b);
        rc = clock_high(b);
        low(b, VDSP_SCL);
        delay(b);
        release(b, VDSP_SDA);
        if (!rc) rx[i] = value;
    }
    if (started && rc != VDSP_I2C_COLLISION && rc != VDSP_I2C_TIMEOUT) {
        int end = stop(b);
        if (!rc) rc = end;
    }
    release_bus(b);
    return rc;
}
