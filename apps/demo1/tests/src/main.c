#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Compile the production validator into this standalone test application. */
#include "../../src/gfx_validate.c"
#include "../../src/gfx_bytebuf.c"
#include "../../src/gfx_stream.c"
#include "../../src/test_record_two_pass_demo.c"

int32_t gfx_execute_stream(void *backend_ctx, const void *stream, uint32_t size)
{
	unsigned *executions = backend_ctx;
	assert(gfx_validate_stream(stream, size).code == GFX_OK);
	(*executions)++;
	return 0;
}

static void expect(const void *stream, uint32_t size, gfx_result_t code,
uint32_t offset, uint16_t opcode)
{
	gfx_validation_result_t result = gfx_validate_stream(stream, size);
	if (result.code != code || result.offset != offset || result.opcode != opcode) {
		fprintf(stderr, "expected %d/%u/%u, got %d/%u/%u\n",
		code, offset, opcode, result.code, result.offset, result.opcode);
		assert(0);
	}
}

static void test_boundaries(void)
{
	uint8_t          bytes[64] = {0};
	gfx_cmd_header_t header    = {GFX_OP_NOP, 0, sizeof(header)};

	expect(NULL, 0, GFX_OK, 0, UINT16_MAX);
	expect(NULL, 1, GFX_ERROR_NULL_STREAM, 0, UINT16_MAX);
	expect(bytes, 7, GFX_ERROR_TRUNCATED_HEADER, 0, UINT16_MAX);

	header.byte_size = 0;
	memcpy(bytes, &header, sizeof(header));
	expect(bytes, 8, GFX_ERROR_INVALID_RECORD_SIZE, 0, GFX_OP_NOP);
	header.byte_size = 9;
	memcpy(bytes, &header, sizeof(header));
	expect(bytes, 12, GFX_ERROR_INVALID_RECORD_SIZE, 0, GFX_OP_NOP);
	header.byte_size = UINT32_MAX - 3u;
	memcpy(bytes, &header, sizeof(header));
	expect(bytes, 8, GFX_ERROR_TRUNCATED_RECORD, 0, GFX_OP_NOP);
	header.opcode    = 0xfffe;
	header.byte_size = 8;
	memcpy(bytes, &header, sizeof(header));
	expect(bytes, 8, GFX_ERROR_UNKNOWN_OPCODE, 0, 0xfffe);
	header.opcode = GFX_OP_DRAW;
	memcpy(bytes, &header, sizeof(header));
	expect(bytes, 8, GFX_ERROR_INVALID_RECORD_SIZE, 0, GFX_OP_DRAW);
	header.opcode    = GFX_OP_NOP;
	header.byte_size = 12;
	memcpy(bytes, &header, sizeof(header));
	expect(bytes, 12, GFX_ERROR_INVALID_PAYLOAD_SIZE, 0, GFX_OP_NOP);

	header.byte_size = 8;
	memcpy(bytes + 1, &header, sizeof(header));
	expect(bytes + 1, 8, GFX_OK, 8, UINT16_MAX);
	expect(bytes + 1, 9, GFX_ERROR_TRUNCATED_HEADER, 8, UINT16_MAX);
}

static void test_bytebuf_append(void)
{
	uint8_t            bytes[8] = {0};
	uint8_t            before[sizeof(bytes)];
	const uint8_t      a[]        = {1, 2, 3};
	const uint8_t      b[]        = {4, 5};
	gfx_bytebuf_t      buffer     = {bytes, 0, sizeof(bytes)};
	gfx_bytebuf_t      empty      = {0};
	gfx_bytebuf_span_t spans[]    = {{a, sizeof(a)}, {NULL, 0}, {b, sizeof(b)}};
	gfx_bytebuf_span_t overflow[] = {{a, UINT32_MAX}, {b, 1}};

	assert(gfx_bytebuf_append(NULL, a, sizeof(a)) == GFX_ERROR_INVALID_ARGUMENT);
	assert(gfx_bytebuf_appendv(&empty, NULL, 0) == GFX_OK);
	assert(gfx_bytebuf_append(&empty, NULL, 0) == GFX_OK);
	assert(gfx_bytebuf_append(&empty, a, 1) == GFX_ERROR_CAPACITY_EXCEEDED);
	empty.capacity = 1;
	assert(gfx_bytebuf_append(&empty, a, 1) == GFX_ERROR_INVALID_ARGUMENT);
	assert(gfx_bytebuf_append(&buffer, NULL, 0) == GFX_OK);
	assert(gfx_bytebuf_append(&buffer, a, sizeof(a)) == GFX_OK);
	assert(buffer.offset == sizeof(a) && memcmp(bytes, a, sizeof(a)) == 0);
	assert(gfx_bytebuf_appendv(&buffer, spans, 3) == GFX_OK);
	assert(buffer.offset == sizeof(bytes));
	assert(memcmp(bytes + 3, a, sizeof(a)) == 0);
	assert(memcmp(bytes + 6, b, sizeof(b)) == 0);

	memcpy(before, bytes, sizeof(bytes));
	assert(gfx_bytebuf_append(&buffer, b, sizeof(b)) == GFX_ERROR_CAPACITY_EXCEEDED);
	assert(gfx_bytebuf_append(&buffer, NULL, 1) == GFX_ERROR_INVALID_ARGUMENT);
	assert(gfx_bytebuf_appendv(&buffer, NULL, 1) == GFX_ERROR_INVALID_ARGUMENT);
	assert(gfx_bytebuf_appendv(&buffer, overflow, 2) == GFX_ERROR_SIZE_OVERFLOW);
	assert(buffer.offset == sizeof(bytes) && memcmp(bytes, before, sizeof(bytes)) == 0);

	buffer.offset = 0;
	spans[1].size = 1;
	assert(gfx_bytebuf_appendv(&buffer, spans, 3) == GFX_ERROR_INVALID_ARGUMENT);
	assert(buffer.offset == 0 && memcmp(bytes, before, sizeof(bytes)) == 0);
	spans[1].size   = 0;
	buffer.capacity = 4;
	assert(gfx_bytebuf_appendv(&buffer, spans, 3) == GFX_ERROR_CAPACITY_EXCEEDED);
	assert(buffer.offset == 0 && memcmp(bytes, before, sizeof(bytes)) == 0);
	buffer.offset = 5;
	assert(gfx_bytebuf_append(&buffer, a, sizeof(a)) == GFX_ERROR_INVALID_ARGUMENT);
	assert(buffer.offset == 5 && memcmp(bytes, before, sizeof(bytes)) == 0);
}

static void test_stream_push(void)
{
	uint8_t             bytes[32] = {0};
	const char          title[]   = "abc";
	cmd_window_create_t window    = {.title_len = 3};
	gfx_stream_t        stream    = {.bytes = {.buf = bytes, .capacity = sizeof(bytes)}};
	gfx_cmd_header_t    header;

	assert(gfx_stream_push(&stream, GFX_OP_WINDOW_CREATE, 7, &window,
	       sizeof(window), title, 3) == GFX_OK);
	assert(stream.bytes.offset == sizeof(window) + 4);
	memcpy(&header, bytes, sizeof(header));
	assert(header.opcode == GFX_OP_WINDOW_CREATE);
	assert(header.flags == 7);
	assert(header.byte_size == sizeof(window) + 4);
	assert(memcmp(bytes + sizeof(window), title, 3) == 0);
	assert(bytes[sizeof(window) + 3] == 0);

	stream.bytes.capacity = stream.bytes.offset;
	assert(gfx_stream_push(&stream, GFX_OP_NOP, 0, &header,
	       sizeof(header), NULL, 0) == GFX_ERROR_CAPACITY_EXCEEDED);
	assert(stream.bytes.offset == sizeof(window) + 4);
	assert(gfx_stream_push(&stream, GFX_OP_NOP, 0, &header,
	       sizeof(header), NULL, 0) == GFX_ERROR_CAPACITY_EXCEEDED);
	assert(stream.error == GFX_ERROR_CAPACITY_EXCEEDED);
}

static void test_stream_push_unsafe(void)
{
	uint8_t          bytes[16] = {0};
	gfx_cmd_header_t header    = {0};
	gfx_stream_t     stream    = {.bytes = {.buf = bytes, .capacity = sizeof(bytes)}};

	assert(gfx_stream_push_unsafe(&stream, 0xffff, 0, &header,
	       sizeof(header), NULL, 0) == GFX_OK);
	assert(stream.state == GFX_STREAM_OUTSIDE_FRAME);
	expect(bytes, stream.bytes.offset, GFX_ERROR_UNKNOWN_OPCODE, 0, 0xffff);
}

static void test_push_transitions(void)
{
	const struct {
		uint16_t opcode;
		uint32_t size;
		unsigned allowed;
	} cases[] = {
	{GFX_OP_NOP, sizeof(gfx_cmd_header_t), 7},
	{GFX_OP_WINDOW_CREATE, sizeof(cmd_window_create_t), 3},
	{GFX_OP_WINDOW_RESIZE, sizeof(cmd_window_resize_t), 3},
	{GFX_OP_WINDOW_DESTROY, sizeof(cmd_window_destroy_t), 3},
	{GFX_OP_FRAME_BEGIN, sizeof(cmd_frame_t), 1},
	{GFX_OP_FRAME_END, sizeof(cmd_frame_t), 2},
	{GFX_OP_BUFFER_CREATE, sizeof(cmd_buffer_create_t), 3},
	{GFX_OP_BUFFER_UPLOAD, sizeof(cmd_buffer_upload_t), 3},
	{GFX_OP_BUFFER_DESTROY, sizeof(cmd_buffer_destroy_t), 3},
	{GFX_OP_TEXTURE_CREATE, sizeof(cmd_texture_create_t), 3},
	{GFX_OP_TEXTURE_UPLOAD, sizeof(cmd_texture_upload_t), 3},
	{GFX_OP_TEXTURE_DESTROY, sizeof(cmd_texture_destroy_t), 3},
	{GFX_OP_SHADER_CREATE, sizeof(cmd_shader_create_t), 3},
	{GFX_OP_SHADER_DESTROY, sizeof(cmd_shader_destroy_t), 3},
	{GFX_OP_PIPELINE_CREATE, sizeof(cmd_pipeline_create_t), 3},
	{GFX_OP_PIPELINE_DESTROY, sizeof(cmd_pipeline_destroy_t), 3},
	{GFX_OP_PASS_BEGIN, sizeof(cmd_pass_begin_t), 2},
	{GFX_OP_PASS_END, sizeof(gfx_cmd_header_t), 4},
	{GFX_OP_SET_VIEWPORT, sizeof(cmd_set_viewport_t), 4},
	{GFX_OP_SET_SCISSOR, sizeof(cmd_set_scissor_t), 4},
	{GFX_OP_BIND_PIPELINE, sizeof(cmd_bind_pipeline_t), 4},
	{GFX_OP_BIND_VBUFFER, sizeof(cmd_bind_vbuffer_t), 4},
	{GFX_OP_BIND_IBUFFER, sizeof(cmd_bind_ibuffer_t), 4},
	{GFX_OP_BIND_UNIFORM, sizeof(cmd_bind_uniform_t), 4},
	{GFX_OP_BIND_TEXTURE, sizeof(cmd_bind_texture_t), 4},
	{GFX_OP_DRAW, sizeof(cmd_draw_t), 4},
	{GFX_OP_DRAW_INDEXED, sizeof(cmd_draw_indexed_t), 4}};
	uint8_t          cmd[128] = {0};
	cmd_frame_t      frame    = {0};
	cmd_pass_begin_t pass     = {0};
	gfx_cmd_header_t end      = {0};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		for (gfx_stream_state_t initial = GFX_STREAM_OUTSIDE_FRAME;
		     initial <= GFX_STREAM_IN_PASS; initial++) {
			uint8_t      bytes[512] = {0};
			uint8_t      before[sizeof(bytes)];
			gfx_stream_t stream = {.bytes = {.buf = bytes, .capacity = sizeof(bytes)}};
			if (initial >= GFX_STREAM_IN_FRAME) {
				assert(gfx_stream_push(&stream, GFX_OP_FRAME_BEGIN, 0,
				       &frame, sizeof(frame), NULL, 0) == GFX_OK);
			}
			if (initial == GFX_STREAM_IN_PASS) {
				assert(gfx_stream_push(&stream, GFX_OP_PASS_BEGIN, 0,
				       &pass, sizeof(pass), NULL, 0) == GFX_OK);
			}
			uint32_t offset = stream.bytes.offset;
			memcpy(before, bytes, sizeof(bytes));
			gfx_result_t result = gfx_stream_push(&stream, cases[i].opcode,
			0, cmd, cases[i].size, NULL, 0);
			if (!(cases[i].allowed & (1u << initial))) {
				assert(result == GFX_ERROR_INVALID_ORDER);
				assert(stream.bytes.offset == offset && stream.state == initial);
				assert(memcmp(bytes, before, sizeof(bytes)) == 0);
				assert(gfx_stream_push(&stream, GFX_OP_NOP, 0, &end,
				       sizeof(end), NULL, 0) == result);
				continue;
			}
			assert(result == GFX_OK);
			if (stream.state == GFX_STREAM_IN_PASS) {
				assert(gfx_stream_push(&stream, GFX_OP_PASS_END, 0,
				       &end, sizeof(end), NULL, 0) == GFX_OK);
			}
			if (stream.state == GFX_STREAM_IN_FRAME) {
				assert(gfx_stream_push(&stream, GFX_OP_FRAME_END, 0,
				       &frame, sizeof(frame), NULL, 0) == GFX_OK);
			}
			expect(bytes, stream.bytes.offset, GFX_OK,
			stream.bytes.offset, UINT16_MAX);
		}
	}
}

static void test_push_failures_and_reset(void)
{
	uint8_t             bytes[128] = {0};
	uint8_t             before[sizeof(bytes)];
	gfx_stream_t        stream  = {.bytes = {.buf = bytes, .capacity = sizeof(bytes)}};
	cmd_frame_t         frame   = {.window_id = 1};
	cmd_frame_t         other   = {.window_id = 2};
	cmd_window_create_t window  = {.title_len = 3};
	gfx_cmd_header_t    header  = {0};
	const char          title[] = "abc";

	assert(gfx_stream_push(&stream, 0xffff, 0, &header, sizeof(header),
	       NULL, 0) == GFX_ERROR_UNKNOWN_OPCODE);
	gfx_stream_reset(&stream);
	assert(gfx_stream_push(&stream, GFX_OP_DRAW, 0, &header, sizeof(header),
	       NULL, 0) == GFX_ERROR_INVALID_COMMAND_SIZE);
	gfx_stream_reset(&stream);
	assert(gfx_stream_push(&stream, GFX_OP_WINDOW_CREATE, 0, &window,
	       sizeof(window), title, 2) == GFX_ERROR_INVALID_PAYLOAD_SIZE);
	gfx_stream_reset(&stream);
	assert(gfx_stream_push(&stream, GFX_OP_FRAME_BEGIN, 0, &frame,
	       sizeof(frame), NULL, 0) == GFX_OK);
	assert(stream.state == GFX_STREAM_IN_FRAME && stream.frame_window == 1);
	uint32_t offset = stream.bytes.offset;
	memcpy(before, bytes, sizeof(bytes));
	assert(gfx_stream_push(&stream, GFX_OP_FRAME_END, 0, &other,
	       sizeof(other), NULL, 0) == GFX_ERROR_FRAME_WINDOW_MISMATCH);
	assert(stream.state == GFX_STREAM_IN_FRAME && stream.frame_window == 1);
	assert(stream.bytes.offset == offset && memcmp(bytes, before, sizeof(bytes)) == 0);

	gfx_stream_reset(&stream);
	assert(stream.bytes.buf == bytes && stream.bytes.capacity == sizeof(bytes));
	assert(stream.bytes.offset == 0 && stream.state == GFX_STREAM_OUTSIDE_FRAME);
	assert(stream.error == GFX_OK && stream.frame_window == GFX_NULL_HANDLE);
	stream.bytes.capacity = sizeof(frame) - 1;
	memcpy(before, bytes, sizeof(bytes));
	assert(gfx_stream_push(&stream, GFX_OP_FRAME_BEGIN, 0, &frame,
	       sizeof(frame), NULL, 0) == GFX_ERROR_CAPACITY_EXCEEDED);
	assert(stream.bytes.offset == 0 && stream.state == GFX_STREAM_OUTSIDE_FRAME);
	assert(stream.frame_window == GFX_NULL_HANDLE && memcmp(bytes, before, sizeof(bytes)) == 0);

	gfx_stream_reset(&stream);
	stream.bytes.capacity = sizeof(bytes);
	assert(gfx_stream_push(&stream, GFX_OP_FRAME_BEGIN, 0, &frame,
	       sizeof(frame), NULL, 0) == GFX_OK);
	expect(bytes, stream.bytes.offset, GFX_ERROR_INCOMPLETE_FRAME,
	stream.bytes.offset, UINT16_MAX);
	gfx_stream_reset(&stream);
	assert(gfx_stream_push(&stream, GFX_OP_NOP, 0, &header, sizeof(header),
	       title, UINT32_MAX) == GFX_ERROR_SIZE_OVERFLOW);
}

static void test_push_payload_sizes(void)
{
	cmd_window_create_t   window   = {.title_len = 3};
	cmd_buffer_upload_t   buffer   = {.data_size = 3};
	cmd_texture_upload_t  texture  = {.data_size = 3};
	cmd_shader_create_t   shader   = {.code_size = 3};
	cmd_pipeline_create_t pipeline = {.attr_count = 1};
	cmd_bind_uniform_t    uniform  = {.data_size = 3};
	const struct {
		uint16_t    opcode;
		const void *cmd;
		uint32_t    size, payload;
	} cases[] = {
	{GFX_OP_WINDOW_CREATE, &window, sizeof(window), 3},
	{GFX_OP_BUFFER_UPLOAD, &buffer, sizeof(buffer), 3},
	{GFX_OP_TEXTURE_UPLOAD, &texture, sizeof(texture), 3},
	{GFX_OP_SHADER_CREATE, &shader, sizeof(shader), 3},
	{GFX_OP_PIPELINE_CREATE, &pipeline, sizeof(pipeline), sizeof(gfx_vertex_attr_t)},
	{GFX_OP_BIND_UNIFORM, &uniform, sizeof(uniform), 3}};
	uint8_t          payload[8] = {1, 2, 3, 4};
	cmd_frame_t      frame      = {.window_id = 1};
	cmd_pass_begin_t pass       = {0};
	gfx_cmd_header_t end        = {0};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		for (int valid = 0; valid < 2; valid++) {
			uint8_t      bytes[256] = {0};
			gfx_stream_t stream     = {.bytes = {.buf = bytes, .capacity = sizeof(bytes)}};
			if (cases[i].opcode == GFX_OP_BIND_UNIFORM) {
				assert(gfx_stream_push(&stream, GFX_OP_FRAME_BEGIN, 0,
				       &frame, sizeof(frame), NULL, 0) == GFX_OK);
				assert(gfx_stream_push(&stream, GFX_OP_PASS_BEGIN, 0,
				       &pass, sizeof(pass), NULL, 0) == GFX_OK);
			}
			uint32_t           offset = stream.bytes.offset;
			gfx_stream_state_t state  = stream.state;
			gfx_result_t       result = gfx_stream_push(&stream, cases[i].opcode, 0,
			      cases[i].cmd, cases[i].size, payload, cases[i].payload - !valid);
			if (!valid) {
				assert(result == GFX_ERROR_INVALID_PAYLOAD_SIZE);
				assert(stream.bytes.offset == offset && stream.state == state);
				continue;
			}
			assert(result == GFX_OK);
			assert(memcmp(bytes + offset + cases[i].size, payload, cases[i].payload) == 0);
			if (stream.state == GFX_STREAM_IN_PASS) {
				assert(gfx_stream_push(&stream, GFX_OP_PASS_END, 0,
				       &end, sizeof(end), NULL, 0) == GFX_OK);
				assert(gfx_stream_push(&stream, GFX_OP_FRAME_END, 0,
				       &frame, sizeof(frame), NULL, 0) == GFX_OK);
			}
			expect(bytes, stream.bytes.offset, GFX_OK,
			stream.bytes.offset, UINT16_MAX);
		}
	}
}

static void test_fixed_commands(void)
{
	const struct {
		uint16_t opcode;
		uint32_t size;
		int      pass;
	} cases[] = {
	{GFX_OP_NOP, sizeof(gfx_cmd_header_t), 0},
	{GFX_OP_WINDOW_RESIZE, sizeof(cmd_window_resize_t), 0},
	{GFX_OP_WINDOW_DESTROY, sizeof(cmd_window_destroy_t), 0},
	{GFX_OP_BUFFER_CREATE, sizeof(cmd_buffer_create_t), 0},
	{GFX_OP_BUFFER_DESTROY, sizeof(cmd_buffer_destroy_t), 0},
	{GFX_OP_TEXTURE_CREATE, sizeof(cmd_texture_create_t), 0},
	{GFX_OP_TEXTURE_DESTROY, sizeof(cmd_texture_destroy_t), 0},
	{GFX_OP_SHADER_DESTROY, sizeof(cmd_shader_destroy_t), 0},
	{GFX_OP_PIPELINE_DESTROY, sizeof(cmd_pipeline_destroy_t), 0},
	{GFX_OP_SET_VIEWPORT, sizeof(cmd_set_viewport_t), 1},
	{GFX_OP_SET_SCISSOR, sizeof(cmd_set_scissor_t), 1},
	{GFX_OP_BIND_PIPELINE, sizeof(cmd_bind_pipeline_t), 1},
	{GFX_OP_BIND_VBUFFER, sizeof(cmd_bind_vbuffer_t), 1},
	{GFX_OP_BIND_IBUFFER, sizeof(cmd_bind_ibuffer_t), 1},
	{GFX_OP_BIND_TEXTURE, sizeof(cmd_bind_texture_t), 1},
	{GFX_OP_DRAW, sizeof(cmd_draw_t), 1},
	{GFX_OP_DRAW_INDEXED, sizeof(cmd_draw_indexed_t), 1}};
	uint8_t          zero[128] = {0};
	cmd_frame_t      frame     = {.window_id = 1};
	cmd_pass_begin_t pass      = {0};
	gfx_cmd_header_t end       = {0};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		uint8_t      bytes[1024] = {0};
		gfx_stream_t stream      = {.bytes = {.buf = bytes, .capacity = sizeof(bytes)}};
		if (cases[i].pass) {
			assert(gfx_stream_push(&stream, GFX_OP_FRAME_BEGIN, 0,
			       &frame, sizeof(frame), NULL, 0) == GFX_OK);
			assert(gfx_stream_push(&stream, GFX_OP_PASS_BEGIN, 0,
			       &pass, sizeof(pass), NULL, 0) == GFX_OK);
		}
		assert(gfx_stream_push(&stream, cases[i].opcode, 0,
		       zero, cases[i].size, NULL, 0) == GFX_OK);
		if (cases[i].pass) {
			assert(gfx_stream_push(&stream, GFX_OP_PASS_END, 0,
			       &end, sizeof(end), NULL, 0) == GFX_OK);
			assert(gfx_stream_push(&stream, GFX_OP_FRAME_END, 0,
			       &frame, sizeof(frame), NULL, 0) == GFX_OK);
		}
		expect(stream.bytes.buf, stream.bytes.offset, GFX_OK, stream.bytes.offset, UINT16_MAX);
	}
}

static void test_payloads(void)
{
	uint8_t               bytes[1024] = {0};
	gfx_stream_t          stream      = {.bytes = {.buf = bytes, .capacity = sizeof(bytes)}};
	uint8_t               payload[5]  = {0};
	gfx_vertex_attr_t     attrs[2]    = {0};
	cmd_window_create_t   window      = {.title_len = 3};
	cmd_buffer_upload_t   buffer      = {.data_size = 5};
	cmd_texture_upload_t  texture     = {.data_size = 1};
	cmd_shader_create_t   shader      = {.code_size = 4};
	cmd_pipeline_create_t pipeline    = {.attr_count = 2};
	cmd_frame_t           frame       = {.window_id = 1};
	cmd_pass_begin_t      pass        = {0};
	cmd_bind_uniform_t    uniform     = {.data_size = 3};
	gfx_cmd_header_t      end         = {0};
	uint32_t              positions[6];

	positions[0] = stream.bytes.offset;
	assert(gfx_stream_push(&stream, GFX_OP_WINDOW_CREATE, 0, &window,
	       sizeof(window), payload, window.title_len) == GFX_OK);
	positions[1] = stream.bytes.offset;
	assert(gfx_stream_push(&stream, GFX_OP_BUFFER_UPLOAD, 0, &buffer,
	       sizeof(buffer), payload, buffer.data_size) == GFX_OK);
	positions[2] = stream.bytes.offset;
	assert(gfx_stream_push(&stream, GFX_OP_TEXTURE_UPLOAD, 0, &texture,
	       sizeof(texture), payload, texture.data_size) == GFX_OK);
	positions[3] = stream.bytes.offset;
	assert(gfx_stream_push(&stream, GFX_OP_SHADER_CREATE, 0, &shader,
	       sizeof(shader), payload, shader.code_size) == GFX_OK);
	positions[4] = stream.bytes.offset;
	assert(gfx_stream_push(&stream, GFX_OP_PIPELINE_CREATE, 0, &pipeline,
	       sizeof(pipeline), attrs, pipeline.attr_count * sizeof(gfx_vertex_attr_t)) == GFX_OK);
	assert(gfx_stream_push(&stream, GFX_OP_FRAME_BEGIN, 0,
	       &frame, sizeof(frame), NULL, 0) == GFX_OK);
	assert(gfx_stream_push(&stream, GFX_OP_PASS_BEGIN, 0,
	       &pass, sizeof(pass), NULL, 0) == GFX_OK);
	positions[5] = stream.bytes.offset;
	assert(gfx_stream_push(&stream, GFX_OP_BIND_UNIFORM, 0,
	       &uniform, sizeof(uniform), payload, uniform.data_size) == GFX_OK);
	assert(gfx_stream_push(&stream, GFX_OP_PASS_END, 0,
	       &end, sizeof(end), NULL, 0) == GFX_OK);
	assert(gfx_stream_push(&stream, GFX_OP_FRAME_END, 0,
	       &frame, sizeof(frame), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_OK, stream.bytes.offset, UINT16_MAX);

	for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
		gfx_cmd_header_t header;
		uint32_t         pos = positions[i];
		memcpy(&header, stream.bytes.buf + pos, sizeof(header));
		uint16_t opcode = header.opcode;
		header.byte_size -= 4;
		memcpy(stream.bytes.buf + pos, &header, sizeof(header));
		expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INVALID_PAYLOAD_SIZE, pos, opcode);
		header.byte_size += 4;
		memcpy(stream.bytes.buf + pos, &header, sizeof(header));
	}
	buffer.data_size = UINT32_MAX;
	memcpy(stream.bytes.buf + positions[1], &buffer, sizeof(buffer));
	gfx_cmd_header_t header = {GFX_OP_BUFFER_UPLOAD, 0, sizeof(buffer)};
	memcpy(stream.bytes.buf + positions[1], &header, sizeof(header));
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INVALID_PAYLOAD_SIZE,
	positions[1], GFX_OP_BUFFER_UPLOAD);
}

static void test_order(void)
{
	uint8_t             bytes[1024] = {0};
	gfx_stream_t        stream      = {.bytes = {.buf = bytes, .capacity = sizeof(bytes)}};
	cmd_frame_t         frame       = {.window_id = 1};
	cmd_frame_t         other       = {.window_id = 2};
	cmd_pass_begin_t    pass        = {0};
	cmd_draw_t          draw        = {0};
	cmd_buffer_create_t buffer      = {0};
	gfx_cmd_header_t    end         = {0};
	uint32_t            pos;

	assert(gfx_stream_push_unsafe(&stream, GFX_OP_DRAW, 0, &draw,
	       sizeof(draw), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INVALID_ORDER, 0, GFX_OP_DRAW);
	stream.bytes.offset = 0;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_PASS_END, 0, &end,
	       sizeof(end), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INVALID_ORDER, 0, GFX_OP_PASS_END);
	stream.bytes.offset = 0;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_PASS_BEGIN, 0, &pass,
	       sizeof(pass), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INVALID_ORDER, 0, GFX_OP_PASS_BEGIN);
	stream.bytes.offset = 0;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_FRAME_END, 0, &frame,
	       sizeof(frame), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INVALID_ORDER, 0, GFX_OP_FRAME_END);
	stream.bytes.offset = 0;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_FRAME_BEGIN, 0, &frame,
	       sizeof(frame), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INCOMPLETE_FRAME, stream.bytes.offset, UINT16_MAX);
	pos = stream.bytes.offset;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_FRAME_BEGIN, 0, &frame,
	       sizeof(frame), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INVALID_ORDER, pos, GFX_OP_FRAME_BEGIN);
	stream.bytes.offset = pos;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_FRAME_END, 0, &other,
	       sizeof(other), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_FRAME_WINDOW_MISMATCH, pos, GFX_OP_FRAME_END);
	stream.bytes.offset = pos;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_PASS_BEGIN, 0, &pass,
	       sizeof(pass), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INCOMPLETE_PASS, stream.bytes.offset, UINT16_MAX);
	pos = stream.bytes.offset;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_PASS_BEGIN, 0, &pass,
	       sizeof(pass), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INVALID_ORDER, pos, GFX_OP_PASS_BEGIN);
	stream.bytes.offset = pos;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_FRAME_END, 0, &frame,
	       sizeof(frame), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INVALID_ORDER, pos, GFX_OP_FRAME_END);
	stream.bytes.offset = pos;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_BUFFER_CREATE, 0, &buffer,
	       sizeof(buffer), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_ERROR_INVALID_ORDER, pos, GFX_OP_BUFFER_CREATE);
	stream.bytes.offset = pos;
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_PASS_END, 0, &end,
	       sizeof(end), NULL, 0) == GFX_OK);
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_FRAME_END, 0, &frame,
	       sizeof(frame), NULL, 0) == GFX_OK);
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_FRAME_BEGIN, 0, &other,
	       sizeof(other), NULL, 0) == GFX_OK);
	assert(gfx_stream_push_unsafe(&stream, GFX_OP_FRAME_END, 0, &other,
	       sizeof(other), NULL, 0) == GFX_OK);
	expect(stream.bytes.buf, stream.bytes.offset, GFX_OK, stream.bytes.offset, UINT16_MAX);
}

int main(void)
{
	unsigned executions = 0;
	test_boundaries();
	test_bytebuf_append();
	test_stream_push();
	test_stream_push_unsafe();
	test_push_transitions();
	test_push_failures_and_reset();
	test_push_payload_sizes();
	test_fixed_commands();
	test_payloads();
	test_order();
	record_two_pass_demo(&executions);
	assert(executions == 1);
	puts("gfx validator tests passed");
	return 0;
}
