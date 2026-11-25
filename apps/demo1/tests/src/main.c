#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Compile the production validator into this standalone test application. */
#include "../../src/gfx_validate.c"
#include "../../src/test_record_two_pass_demo.c"

int32_t gfx_execute_stream(void *backend_ctx, const void *stream, uint32_t size)
{
    unsigned *executions = backend_ctx;
    assert(gfx_validate_stream(stream, size).code == GFX_VALIDATION_OK);
    (*executions)++;
    return 0;
}

typedef struct {
    uint8_t bytes[1024];
    uint32_t size;
} test_stream_t;

static void append(test_stream_t *stream, uint16_t opcode, const void *cmd,
    uint32_t fixed_size, uint32_t payload_size)
{
    uint32_t padded = (payload_size + 3u) & ~3u;
    gfx_cmd_header_t header = {opcode, 0, fixed_size + padded};
    assert(stream->size + header.byte_size <= sizeof(stream->bytes));
    memcpy(stream->bytes + stream->size, cmd, fixed_size);
    memcpy(stream->bytes + stream->size, &header, sizeof(header));
    memset(stream->bytes + stream->size + fixed_size, 0, padded);
    stream->size += header.byte_size;
}

static void expect(const void *stream, uint32_t size, gfx_validation_code_t code,
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
    uint8_t bytes[64] = {0};
    gfx_cmd_header_t header = {GFX_OP_NOP, 0, sizeof(header)};

    expect(NULL, 0, GFX_VALIDATION_OK, 0, UINT16_MAX);
    expect(NULL, 1, GFX_VALIDATION_NULL_STREAM, 0, UINT16_MAX);
    expect(bytes, 7, GFX_VALIDATION_TRUNCATED_HEADER, 0, UINT16_MAX);

    header.byte_size = 0;
    memcpy(bytes, &header, sizeof(header));
    expect(bytes, 8, GFX_VALIDATION_INVALID_RECORD_SIZE, 0, GFX_OP_NOP);
    header.byte_size = 9;
    memcpy(bytes, &header, sizeof(header));
    expect(bytes, 12, GFX_VALIDATION_INVALID_RECORD_SIZE, 0, GFX_OP_NOP);
    header.byte_size = UINT32_MAX - 3u;
    memcpy(bytes, &header, sizeof(header));
    expect(bytes, 8, GFX_VALIDATION_TRUNCATED_RECORD, 0, GFX_OP_NOP);
    header.opcode = 0xfffe;
    header.byte_size = 8;
    memcpy(bytes, &header, sizeof(header));
    expect(bytes, 8, GFX_VALIDATION_UNKNOWN_OPCODE, 0, 0xfffe);
    header.opcode = GFX_OP_DRAW;
    memcpy(bytes, &header, sizeof(header));
    expect(bytes, 8, GFX_VALIDATION_INVALID_RECORD_SIZE, 0, GFX_OP_DRAW);
    header.opcode = GFX_OP_NOP;
    header.byte_size = 12;
    memcpy(bytes, &header, sizeof(header));
    expect(bytes, 12, GFX_VALIDATION_INVALID_PAYLOAD_SIZE, 0, GFX_OP_NOP);

    header.byte_size = 8;
    memcpy(bytes + 1, &header, sizeof(header));
    expect(bytes + 1, 8, GFX_VALIDATION_OK, 8, UINT16_MAX);
    expect(bytes + 1, 9, GFX_VALIDATION_TRUNCATED_HEADER, 8, UINT16_MAX);
}

static void test_fixed_commands(void)
{
    const struct { uint16_t opcode; uint32_t size; int pass; } cases[] = {
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
        {GFX_OP_DRAW_INDEXED, sizeof(cmd_draw_indexed_t), 1}
    };
    uint8_t zero[128] = {0};
    cmd_frame_t frame = {.window_id = 1};
    cmd_pass_begin_t pass = {0};
    gfx_cmd_header_t end = {0};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        test_stream_t stream = {0};
        if (cases[i].pass) {
            append(&stream, GFX_OP_FRAME_BEGIN, &frame, sizeof(frame), 0);
            append(&stream, GFX_OP_PASS_BEGIN, &pass, sizeof(pass), 0);
        }
        append(&stream, cases[i].opcode, zero, cases[i].size, 0);
        if (cases[i].pass) {
            append(&stream, GFX_OP_PASS_END, &end, sizeof(end), 0);
            append(&stream, GFX_OP_FRAME_END, &frame, sizeof(frame), 0);
        }
        expect(stream.bytes, stream.size, GFX_VALIDATION_OK, stream.size, UINT16_MAX);
    }
}

static void test_payloads(void)
{
    test_stream_t stream = {0};
    cmd_window_create_t window = {.title_len = 3};
    cmd_buffer_upload_t buffer = {.data_size = 5};
    cmd_texture_upload_t texture = {.data_size = 1};
    cmd_shader_create_t shader = {.code_size = 4};
    cmd_pipeline_create_t pipeline = {.attr_count = 2};
    cmd_frame_t frame = {.window_id = 1};
    cmd_pass_begin_t pass = {0};
    cmd_bind_uniform_t uniform = {.data_size = 3};
    gfx_cmd_header_t end = {0};
    uint32_t positions[6];

    positions[0] = stream.size;
    append(&stream, GFX_OP_WINDOW_CREATE, &window, sizeof(window), window.title_len);
    positions[1] = stream.size;
    append(&stream, GFX_OP_BUFFER_UPLOAD, &buffer, sizeof(buffer), buffer.data_size);
    positions[2] = stream.size;
    append(&stream, GFX_OP_TEXTURE_UPLOAD, &texture, sizeof(texture), texture.data_size);
    positions[3] = stream.size;
    append(&stream, GFX_OP_SHADER_CREATE, &shader, sizeof(shader), shader.code_size);
    positions[4] = stream.size;
    append(&stream, GFX_OP_PIPELINE_CREATE, &pipeline, sizeof(pipeline),
        pipeline.attr_count * sizeof(gfx_vertex_attr_t));
    append(&stream, GFX_OP_FRAME_BEGIN, &frame, sizeof(frame), 0);
    append(&stream, GFX_OP_PASS_BEGIN, &pass, sizeof(pass), 0);
    positions[5] = stream.size;
    append(&stream, GFX_OP_BIND_UNIFORM, &uniform, sizeof(uniform), uniform.data_size);
    append(&stream, GFX_OP_PASS_END, &end, sizeof(end), 0);
    append(&stream, GFX_OP_FRAME_END, &frame, sizeof(frame), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_OK, stream.size, UINT16_MAX);

    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
        gfx_cmd_header_t header;
        uint32_t pos = positions[i];
        memcpy(&header, stream.bytes + pos, sizeof(header));
        uint16_t opcode = header.opcode;
        header.byte_size -= 4;
        memcpy(stream.bytes + pos, &header, sizeof(header));
        expect(stream.bytes, stream.size, GFX_VALIDATION_INVALID_PAYLOAD_SIZE, pos, opcode);
        header.byte_size += 4;
        memcpy(stream.bytes + pos, &header, sizeof(header));
    }
    buffer.data_size = UINT32_MAX;
    memcpy(stream.bytes + positions[1], &buffer, sizeof(buffer));
    gfx_cmd_header_t header = {GFX_OP_BUFFER_UPLOAD, 0, sizeof(buffer)};
    memcpy(stream.bytes + positions[1], &header, sizeof(header));
    expect(stream.bytes, stream.size, GFX_VALIDATION_INVALID_PAYLOAD_SIZE,
        positions[1], GFX_OP_BUFFER_UPLOAD);
}

static void test_order(void)
{
    test_stream_t stream = {0};
    cmd_frame_t frame = {.window_id = 1};
    cmd_frame_t other = {.window_id = 2};
    cmd_pass_begin_t pass = {0};
    cmd_draw_t draw = {0};
    cmd_buffer_create_t buffer = {0};
    gfx_cmd_header_t end = {0};
    uint32_t pos;

    append(&stream, GFX_OP_DRAW, &draw, sizeof(draw), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_INVALID_ORDER, 0, GFX_OP_DRAW);
    stream.size = 0;
    append(&stream, GFX_OP_PASS_END, &end, sizeof(end), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_INVALID_ORDER, 0, GFX_OP_PASS_END);
    stream.size = 0;
    append(&stream, GFX_OP_PASS_BEGIN, &pass, sizeof(pass), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_INVALID_ORDER, 0, GFX_OP_PASS_BEGIN);
    stream.size = 0;
    append(&stream, GFX_OP_FRAME_END, &frame, sizeof(frame), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_INVALID_ORDER, 0, GFX_OP_FRAME_END);
    stream.size = 0;
    append(&stream, GFX_OP_FRAME_BEGIN, &frame, sizeof(frame), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_INCOMPLETE_FRAME, stream.size, UINT16_MAX);
    pos = stream.size;
    append(&stream, GFX_OP_FRAME_BEGIN, &frame, sizeof(frame), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_INVALID_ORDER, pos, GFX_OP_FRAME_BEGIN);
    stream.size = pos;
    append(&stream, GFX_OP_FRAME_END, &other, sizeof(other), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_FRAME_WINDOW_MISMATCH, pos, GFX_OP_FRAME_END);
    stream.size = pos;
    append(&stream, GFX_OP_PASS_BEGIN, &pass, sizeof(pass), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_INCOMPLETE_PASS, stream.size, UINT16_MAX);
    pos = stream.size;
    append(&stream, GFX_OP_PASS_BEGIN, &pass, sizeof(pass), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_INVALID_ORDER, pos, GFX_OP_PASS_BEGIN);
    stream.size = pos;
    append(&stream, GFX_OP_FRAME_END, &frame, sizeof(frame), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_INVALID_ORDER, pos, GFX_OP_FRAME_END);
    stream.size = pos;
    append(&stream, GFX_OP_BUFFER_CREATE, &buffer, sizeof(buffer), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_INVALID_ORDER, pos, GFX_OP_BUFFER_CREATE);
    stream.size = pos;
    append(&stream, GFX_OP_PASS_END, &end, sizeof(end), 0);
    append(&stream, GFX_OP_FRAME_END, &frame, sizeof(frame), 0);
    append(&stream, GFX_OP_FRAME_BEGIN, &other, sizeof(other), 0);
    append(&stream, GFX_OP_FRAME_END, &other, sizeof(other), 0);
    expect(stream.bytes, stream.size, GFX_VALIDATION_OK, stream.size, UINT16_MAX);
}

int main(void)
{
    unsigned executions = 0;
    test_boundaries();
    test_fixed_commands();
    test_payloads();
    test_order();
    record_two_pass_demo(&executions);
    assert(executions == 1);
    puts("gfx validator tests passed");
    return 0;
}
