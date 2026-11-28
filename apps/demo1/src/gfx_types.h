#ifndef GFX_TYPES_H
#define GFX_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_PROTOCOL_VERSION_MAJOR 1
#define GFX_PROTOCOL_VERSION_MINOR 0

typedef uint32_t gfx_handle_t;
#define GFX_NULL_HANDLE ((gfx_handle_t)0)

#define GFX_ALIGN4(x) (((uint32_t)(x) + 3u) & ~3u)

typedef enum {
	GFX_WINDOW_NONE       = 0,
	GFX_WINDOW_RESIZABLE  = 1 << 0,
	GFX_WINDOW_BORDERLESS = 1 << 1,
	GFX_WINDOW_FULLSCREEN = 1 << 2,
	GFX_WINDOW_HIGH_DPI   = 1 << 3
} gfx_window_flags_t;

typedef enum {
	GFX_PRESENT_IMMEDIATE = 0,
	GFX_PRESENT_VSYNC     = 1,
	GFX_PRESENT_MAILBOX   = 2
} gfx_present_mode_t;

typedef enum {
	GFX_FRAME_DEFAULT  = 0,
	GFX_FRAME_NO_VSYNC = 1 << 0
} gfx_frame_flags_t;

typedef enum {
	GFX_BUFFER_VERTEX  = 1 << 0,
	GFX_BUFFER_INDEX   = 1 << 1,
	GFX_BUFFER_UNIFORM = 1 << 2,
	GFX_BUFFER_DYNAMIC = 1 << 3
} gfx_buffer_usage_t;

typedef enum {
	GFX_FORMAT_RGBA8_UNORM      = 1,
	GFX_FORMAT_RGBA8_SRGB       = 2,
	GFX_FORMAT_R8_UNORM         = 3,
	GFX_FORMAT_RGBA16_FLOAT     = 4,
	GFX_FORMAT_DEPTH24_STENCIL8 = 5,
	GFX_FORMAT_DEPTH32_FLOAT    = 6
} gfx_texture_format_t;

typedef enum {
	GFX_TEXTURE_SAMPLED       = 1 << 0,
	GFX_TEXTURE_RENDER_TARGET = 1 << 1
} gfx_texture_usage_t;

typedef enum {
	GFX_STAGE_VERTEX   = 1,
	GFX_STAGE_FRAGMENT = 2
} gfx_shader_stage_t;

typedef enum {
	GFX_ATTR_FLOAT       = 1,
	GFX_ATTR_FLOAT2      = 2,
	GFX_ATTR_FLOAT3      = 3,
	GFX_ATTR_FLOAT4      = 4,
	GFX_ATTR_UBYTE4_NORM = 5,
	GFX_ATTR_SHORT2_NORM = 6
} gfx_attr_format_t;

typedef enum {
	GFX_TOPOLOGY_TRIANGLES      = 0,
	GFX_TOPOLOGY_TRIANGLE_STRIP = 1,
	GFX_TOPOLOGY_LINES          = 2,
	GFX_TOPOLOGY_LINE_STRIP     = 3,
	GFX_TOPOLOGY_POINTS         = 4
} gfx_topology_t;

typedef enum {
	GFX_CULL_NONE  = 0,
	GFX_CULL_FRONT = 1,
	GFX_CULL_BACK  = 2
} gfx_cull_mode_t;

typedef enum {
	GFX_DEPTH_OFF       = 0,
	GFX_DEPTH_LESS      = 1,
	GFX_DEPTH_LEQUAL    = 2,
	GFX_DEPTH_READ_ONLY = 3
} gfx_depth_mode_t;

typedef enum {
	GFX_BLEND_OPAQUE       = 0,
	GFX_BLEND_ALPHA        = 1,
	GFX_BLEND_PREMUL_ALPHA = 2,
	GFX_BLEND_ADDITIVE     = 3
} gfx_blend_mode_t;

typedef enum {
	GFX_CLEAR_NONE  = 0,
	GFX_CLEAR_COLOR = 1 << 0,
	GFX_CLEAR_DEPTH = 1 << 1
} gfx_clear_flags_t;

typedef enum {
	GFX_INDEX_U16 = 1,
	GFX_INDEX_U32 = 2
} gfx_index_type_t;

typedef enum {
	GFX_SAMPLER_NEAREST = 0,
	GFX_SAMPLER_LINEAR  = 1 << 0,
	GFX_SAMPLER_CLAMP   = 0,
	GFX_SAMPLER_REPEAT  = 1 << 1
} gfx_sampler_flags_t;

#ifdef __cplusplus
}
#endif

#endif
