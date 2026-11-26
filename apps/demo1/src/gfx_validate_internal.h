#ifndef GFX_VALIDATE_INTERNAL_H
#define GFX_VALIDATE_INTERNAL_H

#include "gfx_stream.h"

gfx_result_t gfx_command_layout(uint16_t opcode, const void *cmd,
    uint32_t available, uint32_t *fixed_size, uint64_t *payload_size);
gfx_result_t gfx_command_transition(uint16_t opcode, const void *cmd,
    gfx_stream_state_t *state, gfx_handle_t *frame_window);

#endif
