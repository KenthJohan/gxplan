#ifndef GFX_OPCODES_H
#define GFX_OPCODES_H

#include "gfx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opcode values and record layout are internal to the C command prototype,
 * not a stable binary protocol. */
typedef enum {
    GFX_OP_NOP              = 0x00,

    /* Window & Frame Lifecycle */
    GFX_OP_WINDOW_CREATE    = 0x01,
    GFX_OP_WINDOW_RESIZE    = 0x02,
    GFX_OP_WINDOW_DESTROY   = 0x03,
    GFX_OP_FRAME_BEGIN      = 0x04,
    GFX_OP_FRAME_END        = 0x05,

    /* Resource Management */
    GFX_OP_BUFFER_CREATE    = 0x10,
    GFX_OP_BUFFER_UPLOAD    = 0x11,
    GFX_OP_BUFFER_DESTROY   = 0x12,
    GFX_OP_TEXTURE_CREATE   = 0x13,
    GFX_OP_TEXTURE_UPLOAD   = 0x14,
    GFX_OP_TEXTURE_DESTROY  = 0x15,
    GFX_OP_SHADER_CREATE    = 0x16,
    GFX_OP_SHADER_DESTROY   = 0x17,
    GFX_OP_PIPELINE_CREATE  = 0x18,
    GFX_OP_PIPELINE_DESTROY = 0x19,

    /* Render Pass & Binding State */
    GFX_OP_PASS_BEGIN       = 0x20,
    GFX_OP_PASS_END         = 0x21,
    GFX_OP_SET_VIEWPORT     = 0x22,
    GFX_OP_SET_SCISSOR      = 0x23,
    GFX_OP_BIND_PIPELINE    = 0x24,
    GFX_OP_BIND_VBUFFER     = 0x25,
    GFX_OP_BIND_IBUFFER     = 0x26,
    GFX_OP_BIND_UNIFORM     = 0x27,
    GFX_OP_BIND_TEXTURE     = 0x28,

    /* Draw Execution */
    GFX_OP_DRAW             = 0x30,
    GFX_OP_DRAW_INDEXED     = 0x31
} gfx_opcode_t;

typedef struct {
    uint16_t opcode;
    uint16_t flags;
    uint32_t byte_size;
} gfx_cmd_header_t;

typedef struct {
    uint8_t  location;
    uint8_t  format;
    uint16_t offset;
} gfx_vertex_attr_t;

#ifdef __cplusplus
}
#endif

#endif
