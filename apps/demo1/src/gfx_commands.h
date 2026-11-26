#ifndef GFX_COMMANDS_H
#define GFX_COMMANDS_H

#include "gfx_opcodes.h"
#include "gfx_result.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Commands are authored and pushed from C. The current in-process byte stream
 * is an implementation detail, not a stable binary or wire command format.
 *
 * A record consists of its fixed command struct followed by its payload,
 * padded to four bytes. header.byte_size includes the header and padding.
 * WINDOW_CREATE carries title_len bytes; BUFFER_UPLOAD, TEXTURE_UPLOAD,
 * SHADER_CREATE and BIND_UNIFORM carry data_size, data_size, code_size and
 * data_size bytes respectively. PIPELINE_CREATE carries attr_count
 * gfx_vertex_attr_t values. All other commands have no payload.
 *
 * Each stream must contain complete frames and passes. Passes are nested in
 * frames, rendering state and draw commands are confined to passes, and
 * resource and window commands may only appear outside passes. Frame end
 * must refer to the same window as frame begin. Streams can contain setup
 * commands and multiple complete frames.
 */

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     window_id;
    gfx_handle_t     swapchain_tex_id;
    uint16_t         width;
    uint16_t         height;
    uint8_t          present_mode;
    uint8_t          window_flags;
    uint16_t         title_len;
} cmd_window_create_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     window_id;
    uint16_t         pixel_width;
    uint16_t         pixel_height;
} cmd_window_resize_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     window_id;
} cmd_window_destroy_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     window_id;
} cmd_frame_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     buffer_id;
    uint32_t         size_bytes;
    uint32_t         usage_flags;
} cmd_buffer_create_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     buffer_id;
    uint32_t         dst_offset;
    uint32_t         data_size;
} cmd_buffer_upload_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     buffer_id;
} cmd_buffer_destroy_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     tex_id;
    uint16_t         width;
    uint16_t         height;
    uint8_t          format;
    uint8_t          usage_flags;
    uint16_t         _pad;
} cmd_texture_create_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     tex_id;
    uint16_t         x;
    uint16_t         y;
    uint16_t         width;
    uint16_t         height;
    uint32_t         data_size;
} cmd_texture_upload_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     tex_id;
} cmd_texture_destroy_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     shader_id;
    uint8_t          stage;
    uint8_t          _pad[3];
    uint32_t         code_size;
} cmd_shader_create_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     shader_id;
} cmd_shader_destroy_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     pipeline_id;
    gfx_handle_t     vs_id;
    gfx_handle_t     fs_id;
    uint16_t         vertex_stride;
    uint8_t          attr_count;
    uint8_t          topology;
    uint8_t          cull_mode;
    uint8_t          depth_test;
    uint8_t          blend_mode;
    uint8_t          _pad;
} cmd_pipeline_create_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     pipeline_id;
} cmd_pipeline_destroy_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     color_tex_id;
    gfx_handle_t     depth_tex_id;
    uint8_t          clear_flags;
    uint8_t          _pad[3];
    float            clear_rgba[4];
    float            clear_depth;
} cmd_pass_begin_t;

typedef struct {
    gfx_cmd_header_t header;
    float            x;
    float            y;
    float            width;
    float            height;
    float            min_depth;
    float            max_depth;
} cmd_set_viewport_t;

typedef struct {
    gfx_cmd_header_t header;
    int32_t          x;
    int32_t          y;
    uint32_t         width;
    uint32_t         height;
} cmd_set_scissor_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     pipeline_id;
} cmd_bind_pipeline_t;

typedef struct {
    gfx_cmd_header_t header;
    uint32_t         slot;
    gfx_handle_t     buffer_id;
    uint32_t         offset_bytes;
} cmd_bind_vbuffer_t;

typedef struct {
    gfx_cmd_header_t header;
    gfx_handle_t     buffer_id;
    uint32_t         offset_bytes;
    uint8_t          index_type;
    uint8_t          _pad[3];
} cmd_bind_ibuffer_t;

typedef struct {
    gfx_cmd_header_t header;
    uint8_t          stage;
    uint8_t          slot;
    uint16_t         data_size;
} cmd_bind_uniform_t;

typedef struct {
    gfx_cmd_header_t header;
    uint8_t          slot;
    uint8_t          sampler_flags;
    uint16_t         _pad;
    gfx_handle_t     tex_id;
} cmd_bind_texture_t;

typedef struct {
    gfx_cmd_header_t header;
    uint32_t         vertex_count;
    uint32_t         first_vertex;
} cmd_draw_t;

typedef struct {
    gfx_cmd_header_t header;
    uint32_t         index_count;
    uint32_t         first_index;
    int32_t          vertex_offset;
} cmd_draw_indexed_t;

typedef struct {
    gfx_result_t code;
    uint32_t offset;
    /* UINT16_MAX means no complete header was available. */
    uint16_t opcode;
} gfx_validation_result_t;

/*
 * Validates the complete stream without executing it. An empty stream is valid.
 * On an incomplete frame or pass, offset points one byte past the stream and
 * opcode is UINT16_MAX. Resource existence and backend constraints are not
 * checked. A producer must separately report recording failures.
 */
gfx_validation_result_t gfx_validate_stream(const void *cmd_stream, uint32_t stream_bytes);

/* A backend must validate the entire stream before executing any command. */
int32_t gfx_execute_stream(void *backend_ctx, const void *cmd_stream, uint32_t stream_bytes);

#ifdef __cplusplus
}
#endif

#endif
