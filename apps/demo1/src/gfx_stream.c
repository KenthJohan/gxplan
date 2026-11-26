#include "gfx_stream.h"

#include <string.h>

gfx_stream_result_t gfx_stream_push(gfx_stream_t *stream, uint16_t opcode,
    uint16_t flags, const void *cmd, uint32_t cmd_size,
    const void *extra, uint32_t extra_size)
{
    uint64_t padded_extra;
    uint64_t total_size;
    gfx_cmd_header_t header;

    if (!stream) {
        return GFX_STREAM_INVALID_ARGUMENT;
    }
    if (stream->error != GFX_STREAM_OK) {
        return stream->error;
    }
    if (!stream->buf || !cmd || cmd_size < sizeof(header)
        || (cmd_size & 3u) || (!extra && extra_size)
        || stream->offset > stream->capacity) {
        stream->error = GFX_STREAM_INVALID_ARGUMENT;
        return stream->error;
    }

    padded_extra = ((uint64_t)extra_size + 3u) & ~(uint64_t)3u;
    total_size = (uint64_t)cmd_size + padded_extra;
    if (total_size > UINT32_MAX) {
        stream->error = GFX_STREAM_SIZE_OVERFLOW;
        return stream->error;
    }
    if (total_size > (uint64_t)stream->capacity - stream->offset) {
        stream->error = GFX_STREAM_CAPACITY_EXCEEDED;
        return stream->error;
    }

    memcpy(&header, cmd, sizeof(header));
    header.opcode = opcode;
    header.flags = flags;
    header.byte_size = (uint32_t)total_size;

    memcpy(stream->buf + stream->offset, cmd, cmd_size);
    memcpy(stream->buf + stream->offset, &header, sizeof(header));
    if (extra_size) {
        memcpy(stream->buf + stream->offset + cmd_size, extra, extra_size);
        if (padded_extra > extra_size) {
            memset(stream->buf + stream->offset + cmd_size + extra_size, 0,
                (size_t)(padded_extra - extra_size));
        }
    }
    stream->offset += (uint32_t)total_size;
    return GFX_STREAM_OK;
}
