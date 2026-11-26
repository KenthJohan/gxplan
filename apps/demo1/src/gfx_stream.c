#include "gfx_validate_internal.h"

#include <string.h>

void gfx_stream_reset(gfx_stream_t *stream)
{
    stream->bytes.offset = 0;
    stream->state = GFX_STREAM_OUTSIDE_FRAME;
    stream->frame_window = GFX_NULL_HANDLE;
    stream->error = GFX_STREAM_OK;
}

gfx_stream_result_t gfx_stream_push(gfx_stream_t *stream, uint16_t opcode,
    uint16_t flags, const void *cmd, uint32_t cmd_size,
    const void *extra, uint32_t extra_size)
{
    uint64_t padded_extra;
    uint64_t total_size;
    uint64_t expected_payload;
    uint32_t fixed_size;
    gfx_cmd_header_t header;
    gfx_stream_state_t next_state;
    gfx_handle_t next_window;
    gfx_validation_code_t validation;

    if (!stream) {
        return GFX_STREAM_INVALID_ARGUMENT;
    }
    if (stream->error != GFX_STREAM_OK) {
        return stream->error;
    }
    if (!stream->bytes.buf || !cmd || cmd_size < sizeof(header)
        || (cmd_size & 3u) || (!extra && extra_size)
        || stream->bytes.offset > stream->bytes.capacity
        || (stream->bytes.offset & 3u)
        || stream->state < GFX_STREAM_OUTSIDE_FRAME
        || stream->state > GFX_STREAM_IN_PASS) {
        stream->error = GFX_STREAM_INVALID_ARGUMENT;
        return stream->error;
    }

    padded_extra = ((uint64_t)extra_size + 3u) & ~(uint64_t)3u;
    total_size = (uint64_t)cmd_size + padded_extra;
    if (total_size > UINT32_MAX) {
        stream->error = GFX_STREAM_SIZE_OVERFLOW;
        return stream->error;
    }
    validation = gfx_command_layout(opcode, cmd, cmd_size, &fixed_size, &expected_payload);
    if (validation == GFX_VALIDATION_UNKNOWN_OPCODE) {
        stream->error = GFX_STREAM_UNKNOWN_OPCODE;
        return stream->error;
    }
    if (validation != GFX_VALIDATION_OK || cmd_size != fixed_size) {
        stream->error = GFX_STREAM_INVALID_COMMAND_SIZE;
        return stream->error;
    }
    if (expected_payload != extra_size) {
        stream->error = GFX_STREAM_INVALID_PAYLOAD_SIZE;
        return stream->error;
    }

    next_state = stream->state;
    next_window = stream->frame_window;
    validation = gfx_command_transition(opcode, cmd, &next_state, &next_window);
    if (validation != GFX_VALIDATION_OK) {
        stream->error = validation == GFX_VALIDATION_FRAME_WINDOW_MISMATCH
            ? GFX_STREAM_FRAME_WINDOW_MISMATCH : GFX_STREAM_INVALID_ORDER;
        return stream->error;
    }
    if (total_size > (uint64_t)stream->bytes.capacity - stream->bytes.offset) {
        stream->error = GFX_STREAM_CAPACITY_EXCEEDED;
        return stream->error;
    }

    memcpy(&header, cmd, sizeof(header));
    header.opcode = opcode;
    header.flags = flags;
    header.byte_size = (uint32_t)total_size;

    memcpy(stream->bytes.buf + stream->bytes.offset, cmd, cmd_size);
    memcpy(stream->bytes.buf + stream->bytes.offset, &header, sizeof(header));
    if (extra_size) {
        memcpy(stream->bytes.buf + stream->bytes.offset + cmd_size, extra, extra_size);
        if (padded_extra > extra_size) {
            memset(stream->bytes.buf + stream->bytes.offset + cmd_size + extra_size, 0,
                (size_t)(padded_extra - extra_size));
        }
    }
    stream->bytes.offset += (uint32_t)total_size;
    stream->state = next_state;
    stream->frame_window = next_window;
    return GFX_STREAM_OK;
}
