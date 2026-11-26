#include "gfx_validate_internal.h"

#include <string.h>

void gfx_stream_reset(gfx_stream_t *stream)
{
    stream->bytes.offset = 0;
    stream->state = GFX_STREAM_OUTSIDE_FRAME;
    stream->frame_window = GFX_NULL_HANDLE;
    stream->error = GFX_OK;
}

gfx_result_t gfx_stream_push(gfx_stream_t *stream, uint16_t opcode,
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
    gfx_result_t validation;
    gfx_result_t append_result;

    if (!stream) {
        return GFX_ERROR_INVALID_ARGUMENT;
    }
    if (stream->error != GFX_OK) {
        return stream->error;
    }
    if (!stream->bytes.buf || !cmd || cmd_size < sizeof(header)
        || (cmd_size & 3u) || (!extra && extra_size)
        || stream->bytes.offset > stream->bytes.capacity
        || (stream->bytes.offset & 3u)
        || stream->state < GFX_STREAM_OUTSIDE_FRAME
        || stream->state > GFX_STREAM_IN_PASS) {
        stream->error = GFX_ERROR_INVALID_ARGUMENT;
        return stream->error;
    }

    padded_extra = ((uint64_t)extra_size + 3u) & ~(uint64_t)3u;
    total_size = (uint64_t)cmd_size + padded_extra;
    if (total_size > UINT32_MAX) {
        stream->error = GFX_ERROR_SIZE_OVERFLOW;
        return stream->error;
    }
    validation = gfx_command_layout(opcode, cmd, cmd_size, &fixed_size, &expected_payload);
    if (validation == GFX_ERROR_INVALID_RECORD_SIZE) {
        stream->error = GFX_ERROR_INVALID_COMMAND_SIZE;
        return stream->error;
    }
    if (validation != GFX_OK) {
        stream->error = validation;
        return stream->error;
    }
    if (cmd_size != fixed_size) {
        stream->error = GFX_ERROR_INVALID_COMMAND_SIZE;
        return stream->error;
    }
    if (expected_payload != extra_size) {
        stream->error = GFX_ERROR_INVALID_PAYLOAD_SIZE;
        return stream->error;
    }

    next_state = stream->state;
    next_window = stream->frame_window;
    validation = gfx_command_transition(opcode, cmd, &next_state, &next_window);
    if (validation != GFX_OK) {
        stream->error = validation;
        return stream->error;
    }
    memcpy(&header, cmd, sizeof(header));
    header.opcode = opcode;
    header.flags = flags;
    header.byte_size = (uint32_t)total_size;

    const uint8_t padding[3] = {0};
    const gfx_bytebuf_span_t spans[] = {
        {&header, sizeof(header)},
        {(const uint8_t *)cmd + sizeof(header), cmd_size - sizeof(header)},
        {extra, extra_size},
        {padding, (uint32_t)(padded_extra - extra_size)}
    };
    append_result = gfx_bytebuf_appendv(&stream->bytes, spans,
        sizeof(spans) / sizeof(spans[0]));
    if (append_result != GFX_OK) {
        stream->error = append_result;
        return stream->error;
    }
    stream->state = next_state;
    stream->frame_window = next_window;
    return GFX_OK;
}
