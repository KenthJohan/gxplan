#include "gfx_bytebuf.h"

#include <string.h>

gfx_result_t gfx_bytebuf_appendv(gfx_bytebuf_t *buffer, const gfx_bytebuf_span_t *spans, uint32_t count)
{
	uint32_t total = 0;
	uint32_t offset;

	if (!buffer || (!buffer->buf && buffer->capacity) || buffer->offset > buffer->capacity || (!spans && count)) {
		return GFX_ERROR_INVALID_ARGUMENT;
	}

	for (uint32_t i = 0; i < count; i++) {
		if (!spans[i].data && spans[i].size) {
			return GFX_ERROR_INVALID_ARGUMENT;
		}
		if (spans[i].size > UINT32_MAX - total) {
			return GFX_ERROR_SIZE_OVERFLOW;
		}
		total += spans[i].size;
	}
	if (total > buffer->capacity - buffer->offset) {
		return GFX_ERROR_CAPACITY_EXCEEDED;
	}

	offset = buffer->offset;
	for (uint32_t i = 0; i < count; i++) {
		if (spans[i].size) {
			memcpy(buffer->buf + offset, spans[i].data, spans[i].size);
			offset += spans[i].size;
		}
	}
	buffer->offset = offset;
	return GFX_OK;
}

gfx_result_t gfx_bytebuf_append(gfx_bytebuf_t *buffer,
const void *data, uint32_t size)
{
	gfx_bytebuf_span_t span = {data, size};
	return gfx_bytebuf_appendv(buffer, &span, 1);
}
