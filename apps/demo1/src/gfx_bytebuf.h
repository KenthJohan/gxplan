#ifndef GFX_BYTEBUF_H
#define GFX_BYTEBUF_H

#include <stdint.h>

/* Caller-owned storage; offset is the number of bytes written. */
typedef struct {
    uint8_t *buf;
    uint32_t offset;
    uint32_t capacity;
} gfx_bytebuf_t;

#endif
