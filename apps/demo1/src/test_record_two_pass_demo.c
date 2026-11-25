#include <stdio.h>
#include <string.h>
#include "gfx_commands.h"

typedef struct {
	uint8_t *buf;
	uint32_t offset;
	uint32_t capacity;
} gfx_stream_t;

static void push_cmd(gfx_stream_t *s, uint16_t op, uint16_t flags,
const void *cmd, uint32_t cmd_size,
const void *extra, uint32_t extra_size)
{
	uint32_t padded_extra = GFX_ALIGN4(extra_size);
	uint32_t total        = cmd_size + padded_extra;
	if (s->offset + total > s->capacity)
		return;

	gfx_cmd_header_t hdr = {op, flags, total};
	memcpy(s->buf + s->offset, cmd, cmd_size);
	memcpy(s->buf + s->offset, &hdr, sizeof(hdr)); /* Write filled header */
	s->offset += cmd_size;

	if (extra && extra_size > 0) {
		memcpy(s->buf + s->offset, extra, extra_size);
		if (padded_extra > extra_size) {
			memset(s->buf + s->offset + extra_size, 0, padded_extra - extra_size);
		}
		s->offset += padded_extra;
	}
}

void record_two_pass_demo(void *backend_ctx)
{
	uint8_t      memory[16384];
	gfx_stream_t s = {memory, 0, sizeof(memory)};

	/* Handle IDs chosen by the host */
	const gfx_handle_t WIN_ID        = 1;
	const gfx_handle_t SWAPCHAIN_TEX = 2;
	const gfx_handle_t OFFSCREEN_TEX = 10;
	const gfx_handle_t VBUF_TRI      = 20;
	const gfx_handle_t VBUF_QUAD     = 21;
	const gfx_handle_t PIPE_PASS1    = 30;
	const gfx_handle_t PIPE_PASS2    = 31;

	/* --- A. ONE-TIME SETUP --- */
	const char         *title = "Two-Pass Demo";
	cmd_window_create_t win   = {
	.window_id = WIN_ID, .swapchain_tex_id = SWAPCHAIN_TEX, .width = 800, .height = 600, .present_mode = GFX_PRESENT_VSYNC, .title_len = (uint16_t)strlen(title)};
	push_cmd(&s, GFX_OP_WINDOW_CREATE, 0, &win, sizeof(win), title, win.title_len);

	/* Create offscreen color texture (sampled in Pass 2, render target in Pass 1) */
	cmd_texture_create_t tex = {
	.tex_id = OFFSCREEN_TEX, .width = 800, .height = 600, .format = GFX_FORMAT_RGBA8_UNORM, .usage_flags = GFX_TEXTURE_RENDER_TARGET | GFX_TEXTURE_SAMPLED};
	push_cmd(&s, GFX_OP_TEXTURE_CREATE, 0, &tex, sizeof(tex), NULL, 0);

	/* Upload raw vertex bytes (opaque to the protocol) */
	float               tri_verts[] = {0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f};
	cmd_buffer_create_t bc          = {.buffer_id = VBUF_TRI, .size_bytes = sizeof(tri_verts), .usage_flags = GFX_BUFFER_VERTEX};
	cmd_buffer_upload_t bu          = {.buffer_id = VBUF_TRI, .dst_offset = 0, .data_size = sizeof(tri_verts)};
	push_cmd(&s, GFX_OP_BUFFER_CREATE, 0, &bc, sizeof(bc), NULL, 0);
	push_cmd(&s, GFX_OP_BUFFER_UPLOAD, 0, &bu, sizeof(bu), tri_verts, sizeof(tri_verts));

	/* (Assume shaders & PIPE_PASS1 / PIPE_PASS2 + VBUF_QUAD created similarly) */

	/* --- B. PER-FRAME TWO-PASS RENDERING --- */
	cmd_frame_t frame = {.window_id = WIN_ID};
	push_cmd(&s, GFX_OP_FRAME_BEGIN, 0, &frame, sizeof(frame), NULL, 0);

	/* PASS 1: Draw triangle into OFFSCREEN_TEX (ID 10) */
	cmd_pass_begin_t pass1 = {
	.color_tex_id = OFFSCREEN_TEX,
	.depth_tex_id = GFX_NULL_HANDLE,
	.clear_flags  = GFX_CLEAR_COLOR,
	.clear_rgba   = {0.1f, 0.2f, 0.4f, 1.0f}};
	cmd_set_viewport_t  vp  = {.x = 0, .y = 0, .width = 800, .height = 600, .min_depth = 0.0f, .max_depth = 1.0f};
	cmd_bind_pipeline_t bp1 = {.pipeline_id = PIPE_PASS1};
	cmd_bind_vbuffer_t  bv1 = {.slot = 0, .buffer_id = VBUF_TRI, .offset_bytes = 0};
	cmd_draw_t          dr1 = {.vertex_count = 3, .first_vertex = 0};

	push_cmd(&s, GFX_OP_PASS_BEGIN, 0, &pass1, sizeof(pass1), NULL, 0);
	push_cmd(&s, GFX_OP_SET_VIEWPORT, 0, &vp, sizeof(vp), NULL, 0);
	push_cmd(&s, GFX_OP_BIND_PIPELINE, 0, &bp1, sizeof(bp1), NULL, 0);
	push_cmd(&s, GFX_OP_BIND_VBUFFER, 0, &bv1, sizeof(bv1), NULL, 0);
	push_cmd(&s, GFX_OP_DRAW, 0, &dr1, sizeof(dr1), NULL, 0);
	push_cmd(&s, GFX_OP_PASS_END, 0, &(gfx_cmd_header_t){0}, sizeof(gfx_cmd_header_t), NULL, 0);

	/* PASS 2: Sample OFFSCREEN_TEX (ID 10) and draw onto SWAPCHAIN_TEX (ID 2) */
	cmd_pass_begin_t pass2 = {
	.color_tex_id = SWAPCHAIN_TEX,
	.depth_tex_id = GFX_NULL_HANDLE,
	.clear_flags  = GFX_CLEAR_NONE};
	cmd_bind_pipeline_t bp2 = {.pipeline_id = PIPE_PASS2};
	cmd_bind_texture_t  bt2 = {.slot = 0, .sampler_flags = GFX_SAMPLER_LINEAR | GFX_SAMPLER_CLAMP, .tex_id = OFFSCREEN_TEX};
	cmd_bind_vbuffer_t  bv2 = {.slot = 0, .buffer_id = VBUF_QUAD, .offset_bytes = 0};
	cmd_draw_t          dr2 = {.vertex_count = 6, .first_vertex = 0};

	push_cmd(&s, GFX_OP_PASS_BEGIN, 0, &pass2, sizeof(pass2), NULL, 0);
	push_cmd(&s, GFX_OP_SET_VIEWPORT, 0, &vp, sizeof(vp), NULL, 0);
	push_cmd(&s, GFX_OP_BIND_PIPELINE, 0, &bp2, sizeof(bp2), NULL, 0);
	push_cmd(&s, GFX_OP_BIND_TEXTURE, 0, &bt2, sizeof(bt2), NULL, 0);
	push_cmd(&s, GFX_OP_BIND_VBUFFER, 0, &bv2, sizeof(bv2), NULL, 0);
	push_cmd(&s, GFX_OP_DRAW, 0, &dr2, sizeof(dr2), NULL, 0);
	push_cmd(&s, GFX_OP_PASS_END, 0, &(gfx_cmd_header_t){0}, sizeof(gfx_cmd_header_t), NULL, 0);

	/* Present Window 1 */
	push_cmd(&s, GFX_OP_FRAME_END, 0, &frame, sizeof(frame), NULL, 0);

	/* Execute the entire recorded byte stream on the chosen backend */
	gfx_execute_stream(backend_ctx, s.buf, s.offset);
}
