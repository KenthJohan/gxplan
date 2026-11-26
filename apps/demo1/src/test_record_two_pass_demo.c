#include <stdio.h>
#include <string.h>
#include "gfx_commands.h"
#include "gfx_stream.h"

void record_two_pass_demo(void *backend_ctx)
{
	uint8_t      memory[16384];
	gfx_stream_t s = {.bytes = {.buf = memory, .capacity = sizeof(memory)}};

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
	gfx_stream_push(&s, GFX_OP_WINDOW_CREATE, 0, &win, sizeof(win), title, win.title_len);

	/* Create offscreen color texture (sampled in Pass 2, render target in Pass 1) */
	cmd_texture_create_t tex = {
	.tex_id = OFFSCREEN_TEX, .width = 800, .height = 600, .format = GFX_FORMAT_RGBA8_UNORM, .usage_flags = GFX_TEXTURE_RENDER_TARGET | GFX_TEXTURE_SAMPLED};
	gfx_stream_push(&s, GFX_OP_TEXTURE_CREATE, 0, &tex, sizeof(tex), NULL, 0);

	/* Upload raw vertex bytes (opaque to the protocol) */
	float               tri_verts[] = {0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f};
	cmd_buffer_create_t bc          = {.buffer_id = VBUF_TRI, .size_bytes = sizeof(tri_verts), .usage_flags = GFX_BUFFER_VERTEX};
	cmd_buffer_upload_t bu          = {.buffer_id = VBUF_TRI, .dst_offset = 0, .data_size = sizeof(tri_verts)};
	gfx_stream_push(&s, GFX_OP_BUFFER_CREATE, 0, &bc, sizeof(bc), NULL, 0);
	gfx_stream_push(&s, GFX_OP_BUFFER_UPLOAD, 0, &bu, sizeof(bu), tri_verts, sizeof(tri_verts));

	/* (Assume shaders & PIPE_PASS1 / PIPE_PASS2 + VBUF_QUAD created similarly) */

	/* --- B. PER-FRAME TWO-PASS RENDERING --- */
	cmd_frame_t frame = {.window_id = WIN_ID};
	gfx_stream_push(&s, GFX_OP_FRAME_BEGIN, 0, &frame, sizeof(frame), NULL, 0);

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

	gfx_stream_push(&s, GFX_OP_PASS_BEGIN, 0, &pass1, sizeof(pass1), NULL, 0);
	gfx_stream_push(&s, GFX_OP_SET_VIEWPORT, 0, &vp, sizeof(vp), NULL, 0);
	gfx_stream_push(&s, GFX_OP_BIND_PIPELINE, 0, &bp1, sizeof(bp1), NULL, 0);
	gfx_stream_push(&s, GFX_OP_BIND_VBUFFER, 0, &bv1, sizeof(bv1), NULL, 0);
	gfx_stream_push(&s, GFX_OP_DRAW, 0, &dr1, sizeof(dr1), NULL, 0);
	gfx_stream_push(&s, GFX_OP_PASS_END, 0, &(gfx_cmd_header_t){0}, sizeof(gfx_cmd_header_t), NULL, 0);

	/* PASS 2: Sample OFFSCREEN_TEX (ID 10) and draw onto SWAPCHAIN_TEX (ID 2) */
	cmd_pass_begin_t pass2 = {
	.color_tex_id = SWAPCHAIN_TEX,
	.depth_tex_id = GFX_NULL_HANDLE,
	.clear_flags  = GFX_CLEAR_NONE};
	cmd_bind_pipeline_t bp2 = {.pipeline_id = PIPE_PASS2};
	cmd_bind_texture_t  bt2 = {.slot = 0, .sampler_flags = GFX_SAMPLER_LINEAR | GFX_SAMPLER_CLAMP, .tex_id = OFFSCREEN_TEX};
	cmd_bind_vbuffer_t  bv2 = {.slot = 0, .buffer_id = VBUF_QUAD, .offset_bytes = 0};
	cmd_draw_t          dr2 = {.vertex_count = 6, .first_vertex = 0};

	gfx_stream_push(&s, GFX_OP_PASS_BEGIN, 0, &pass2, sizeof(pass2), NULL, 0);
	gfx_stream_push(&s, GFX_OP_SET_VIEWPORT, 0, &vp, sizeof(vp), NULL, 0);
	gfx_stream_push(&s, GFX_OP_BIND_PIPELINE, 0, &bp2, sizeof(bp2), NULL, 0);
	gfx_stream_push(&s, GFX_OP_BIND_TEXTURE, 0, &bt2, sizeof(bt2), NULL, 0);
	gfx_stream_push(&s, GFX_OP_BIND_VBUFFER, 0, &bv2, sizeof(bv2), NULL, 0);
	gfx_stream_push(&s, GFX_OP_DRAW, 0, &dr2, sizeof(dr2), NULL, 0);
	gfx_stream_push(&s, GFX_OP_PASS_END, 0, &(gfx_cmd_header_t){0}, sizeof(gfx_cmd_header_t), NULL, 0);

	/* Present Window 1 */
	gfx_stream_push(&s, GFX_OP_FRAME_END, 0, &frame, sizeof(frame), NULL, 0);

	/* Execute the entire recorded byte stream on the chosen backend */
	if (s.error != GFX_OK) {
		fprintf(stderr, "Unable to record command stream: error %d\n", s.error);
		return;
	}
	gfx_validation_result_t result = gfx_validate_stream(s.bytes.buf, s.bytes.offset);
	if (result.code != GFX_OK) {
		fprintf(stderr, "Invalid command stream: error %d at byte %u (opcode %u)\n",
			result.code, result.offset, result.opcode);
		return;
	}
	gfx_execute_stream(backend_ctx, s.bytes.buf, s.bytes.offset);
}
