#ifndef GFX_STREAM_H
#define GFX_STREAM_H

#include "gfx_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GFX_STREAM_OK = 0,
    GFX_STREAM_INVALID_ARGUMENT,
    GFX_STREAM_SIZE_OVERFLOW,
    GFX_STREAM_CAPACITY_EXCEEDED
} gfx_stream_result_t;

typedef struct {
    uint8_t *buf;
    uint32_t offset;
    uint32_t capacity;
    gfx_stream_result_t error;
} gfx_stream_t;

/*
 * Appends a fixed command (including its header) and an optional payload.
 * The first error is retained in stream->error; after an error, the stream
 * remains unchanged and subsequent pushes return that error.
 */
gfx_stream_result_t gfx_stream_push(gfx_stream_t *stream, uint16_t opcode,
    uint16_t flags, const void *cmd, uint32_t cmd_size,
    const void *extra, uint32_t extra_size);

#ifdef __cplusplus
}
#endif

#endif
