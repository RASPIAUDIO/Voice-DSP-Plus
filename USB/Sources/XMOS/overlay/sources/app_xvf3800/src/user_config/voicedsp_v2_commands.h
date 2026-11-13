#ifndef VOICEDSP_V2_COMMANDS_H
#define VOICEDSP_V2_COMMANDS_H
#include <stdint.h>
#include <stddef.h>
#define VDSP_V2_COMMAND_PORT 8
#define VDSP_V2_STATUS_SIZE 56
/* Application servicer on tile 1 calls exchange serially; no I2C here. */
int vdsp_v2_command_exchange(const uint8_t *command, size_t length, uint8_t *status);
void vdsp_v2_command_start(void);
/* Tile 0 mailbox. Newest accepted request wins; a new epoch cancels old enable. */
int vdsp_v2_command_accept(const uint8_t command[4]);
uint32_t vdsp_v2_command_get(uint8_t command[4]);
void vdsp_v2_status_publish(const uint8_t status[VDSP_V2_STATUS_SIZE]);
void vdsp_v2_status_get(uint8_t status[VDSP_V2_STATUS_SIZE]);
void vdsp_v2_command_ready(void);
#endif
