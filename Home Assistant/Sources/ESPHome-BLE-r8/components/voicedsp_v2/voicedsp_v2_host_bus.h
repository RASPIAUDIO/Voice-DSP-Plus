#ifdef __cplusplus
extern "C" {
#endif
#ifndef VOICEDSP_V2_HOST_BUS_H
#define VOICEDSP_V2_HOST_BUS_H
#include <stddef.h>
#include "voicedsp_v2_i2c.h"
/* Raw bootstrap reads/writes and SDK repeated-START transactions. No retries,
 * bus recovery or push-pull high. Single serialized owner as for the core bus.
 * A failed read may partially modify rx; callers must ignore it on error.
 */
int vdsp_v2_host_bus_transfer(vdsp_v2_i2c *, uint8_t address,
                             const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr);
/* Dedicated RUP2 writer transport. Runtime packet limits remain unchanged. */
int vdsp_v2_program_bus_transfer(vdsp_v2_i2c *, uint8_t address,
                                const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr);
#endif

#ifdef __cplusplus
}
#endif
