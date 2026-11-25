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

void run_dynamic_loop(void *backend_ctx)
{
	static uint8_t stream_mem[65536];
	gfx_stream_t   s = {stream_mem, 0, sizeof(stream_mem)};

	const gfx_handle_t WIN_ID        = 1;
	const gfx_handle_t SWAPCHAIN_TEX = 2;
	const gfx_handle_t DYN_VBUF      = 10;
	const gfx_handle_t PIPE_ID       = 20;
	const uint32_t     MAX_VERTS     = 1024;

	/* =====================================================================
	 * 1. STARTUP STREAM (Executed ONCE before the loop)
	 * ===================================================================== */
	cmd_window_create_t win = {
	.window_id = WIN_ID, .swapchain_tex_id = SWAPCHAIN_TEX, .width = 800, .height = 600, .present_mode = GFX_PRESENT_VSYNC};
	push_cmd(&s, GFX_OP_WINDOW_CREATE, 0, &win, sizeof(win), NULL, 0);

	/* Pre-allocate a dynamic buffer large enough for worst-case vertex count */
	cmd_buffer_create_t bc = {
	.buffer_id   = DYN_VBUF,
	.size_bytes  = MAX_VERTS * sizeof(float) * 2,
	.usage_flags = GFX_BUFFER_VERTEX | GFX_BUFFER_DYNAMIC};
	push_cmd(&s, GFX_OP_BUFFER_CREATE, 0, &bc, sizeof(bc), NULL, 0);

	/* (Assume shaders & PIPE_ID are created here too) */

	gfx_execute_stream(backend_ctx, s.buf, s.offset);

	/* =====================================================================
	 * 2. MAIN LOOP (Executed every frame)
	 * ===================================================================== */
	float    verts[MAX_VERTS * 2];
	uint32_t vert_count = 0;
	int      running    = 1;

	while (running) {
		/* A. Host polls input (SDL_PollEvent, etc.) and updates `verts` */
		float mouse_x = 0.0f, mouse_y = 0.0f;
		int   clicked = poll_host_input(&running, &mouse_x, &mouse_y);
		if (clicked && vert_count < MAX_VERTS) {
			verts[vert_count * 2 + 0] = mouse_x;
			verts[vert_count * 2 + 1] = mouse_y;
			vert_count++;
		}

		/* B. Reset stream cursor to 0 for the new frame */
		s.offset = 0;

		cmd_frame_t frame = {.window_id = WIN_ID};
		push_cmd(&s, GFX_OP_FRAME_BEGIN, 0, &frame, sizeof(frame), NULL, 0);

		/* C. Upload dynamic vertices BEFORE starting the render pass */
		if (vert_count > 0) {
			uint32_t            upload_bytes = vert_count * sizeof(float) * 2;
			cmd_buffer_upload_t bu           = {
			.buffer_id  = DYN_VBUF,
			.dst_offset = 0,
			.data_size  = upload_bytes};
			push_cmd(&s, GFX_OP_BUFFER_UPLOAD, 0, &bu, sizeof(bu), verts, upload_bytes);
		}

		/* D. Begin Render Pass & Draw `vert_count` vertices */
		cmd_pass_begin_t pass = {
		.color_tex_id = SWAPCHAIN_TEX,
		.clear_flags  = GFX_CLEAR_COLOR,
		.clear_rgba   = {0.08f, 0.08f, 0.10f, 1.0f}};
		push_cmd(&s, GFX_OP_PASS_BEGIN, 0, &pass, sizeof(pass), NULL, 0);

		if (vert_count > 0) {
			cmd_bind_pipeline_t bp = {.pipeline_id = PIPE_ID};
			cmd_bind_vbuffer_t  bv = {.slot = 0, .buffer_id = DYN_VBUF, .offset_bytes = 0};
			cmd_draw_t          dr = {.vertex_count = vert_count, .first_vertex = 0};

			push_cmd(&s, GFX_OP_BIND_PIPELINE, 0, &bp, sizeof(bp), NULL, 0);
			push_cmd(&s, GFX_OP_BIND_VBUFFER, 0, &bv, sizeof(bv), NULL, 0);
			push_cmd(&s, GFX_OP_DRAW, 0, &dr, sizeof(dr), NULL, 0);
		}

		push_cmd(&s, GFX_OP_PASS_END, 0, &(gfx_cmd_header_t){0}, sizeof(gfx_cmd_header_t), NULL, 0);
		push_cmd(&s, GFX_OP_FRAME_END, 0, &frame, sizeof(frame), NULL, 0);

		/* E. Execute the frame's command stream */
		gfx_execute_stream(backend_ctx, s.buf, s.offset);
	}
}

int main()
{
	printf("hello\n");
}
