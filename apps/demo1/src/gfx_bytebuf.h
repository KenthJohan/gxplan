#ifndef GFX_BYTEBUF_H
#define GFX_BYTEBUF_H

#include <stdint.h>
#include "gfx_result.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Caller-owned storage; offset is the number of bytes written. */
typedef struct {
	uint8_t *buf;
	uint32_t offset;
	uint32_t capacity;
} gfx_bytebuf_t;

typedef struct {
	const void *data;
	uint32_t    size;
} gfx_bytebuf_span_t;

/*
 * Append bytes to caller-owned storage. A zero-length span may have NULL data.
 * Source ranges must not overlap the destination range or the buffer metadata;
 * the span array must not overlap the destination range.
 * On failure, neither the buffer contents nor its offset are changed.
 */
gfx_result_t gfx_bytebuf_append(gfx_bytebuf_t *buffer,
const void *data, uint32_t size);
gfx_result_t gfx_bytebuf_appendv(gfx_bytebuf_t *buffer,
const gfx_bytebuf_span_t *spans, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif
