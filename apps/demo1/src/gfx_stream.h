#ifndef GFX_STREAM_H
#define GFX_STREAM_H

#include "gfx_bytebuf.h"
#include "gfx_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	GFX_STREAM_OUTSIDE_FRAME = 0,
	GFX_STREAM_IN_FRAME,
	GFX_STREAM_IN_PASS
} gfx_stream_state_t;

typedef struct {
	gfx_bytebuf_t      bytes;
	gfx_stream_state_t state;
	gfx_handle_t       frame_window;
	gfx_result_t       error;
} gfx_stream_t;

/*
 * Initialize with caller-owned bytes and zero offset/state/error. Reset before
 * reusing a stream; do not mutate its bytes or state while recording.
 * A push validates command layout and ordering before appending the fixed
 * command and optional payload. The first error is retained; a failed push
 * changes neither the bytes nor the state. Full-stream validation is still
 * required before execution to detect unfinished frames/passes.
 * Command and payload storage must not overlap the appended destination.
 */
void gfx_stream_reset(gfx_stream_t *stream);

gfx_result_t gfx_stream_push(gfx_stream_t *stream, uint16_t opcode,
uint16_t flags, const void *cmd, uint32_t cmd_size,
const void *extra, uint32_t extra_size);

/*
 * Appends a record without validating its opcode, command layout, payload
 * size, or ordering. Basic buffer arguments, record alignment, and capacity
 * are still checked. Command and payload storage must not overlap the
 * appended destination. Validate the complete stream before execution.
 */
gfx_result_t gfx_stream_push_unsafe(gfx_stream_t *stream, uint16_t opcode,
uint16_t flags, const void *cmd, uint32_t cmd_size,
const void *extra, uint32_t extra_size);

#ifdef __cplusplus
}
#endif

#endif
