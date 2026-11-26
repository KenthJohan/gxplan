#include "gfx_validate_internal.h"

#include <string.h>

static gfx_validation_result_t validation_result(
    gfx_result_t code, uint32_t offset, uint16_t opcode)
{
    gfx_validation_result_t result = {code, offset, opcode};
    return result;
}

gfx_result_t gfx_command_layout(uint16_t opcode, const void *cmd,
    uint32_t available, uint32_t *fixed_size_out, uint64_t *payload_size_out)
{
    uint32_t fixed_size = 0;
    uint64_t payload_size = 0;

    switch (opcode) {
    case GFX_OP_NOP: fixed_size = sizeof(gfx_cmd_header_t); break;
    case GFX_OP_WINDOW_CREATE: fixed_size = sizeof(cmd_window_create_t); break;
    case GFX_OP_WINDOW_RESIZE: fixed_size = sizeof(cmd_window_resize_t); break;
    case GFX_OP_WINDOW_DESTROY: fixed_size = sizeof(cmd_window_destroy_t); break;
    case GFX_OP_FRAME_BEGIN:
    case GFX_OP_FRAME_END: fixed_size = sizeof(cmd_frame_t); break;
    case GFX_OP_BUFFER_CREATE: fixed_size = sizeof(cmd_buffer_create_t); break;
    case GFX_OP_BUFFER_UPLOAD: fixed_size = sizeof(cmd_buffer_upload_t); break;
    case GFX_OP_BUFFER_DESTROY: fixed_size = sizeof(cmd_buffer_destroy_t); break;
    case GFX_OP_TEXTURE_CREATE: fixed_size = sizeof(cmd_texture_create_t); break;
    case GFX_OP_TEXTURE_UPLOAD: fixed_size = sizeof(cmd_texture_upload_t); break;
    case GFX_OP_TEXTURE_DESTROY: fixed_size = sizeof(cmd_texture_destroy_t); break;
    case GFX_OP_SHADER_CREATE: fixed_size = sizeof(cmd_shader_create_t); break;
    case GFX_OP_SHADER_DESTROY: fixed_size = sizeof(cmd_shader_destroy_t); break;
    case GFX_OP_PIPELINE_CREATE: fixed_size = sizeof(cmd_pipeline_create_t); break;
    case GFX_OP_PIPELINE_DESTROY: fixed_size = sizeof(cmd_pipeline_destroy_t); break;
    case GFX_OP_PASS_BEGIN: fixed_size = sizeof(cmd_pass_begin_t); break;
    case GFX_OP_PASS_END: fixed_size = sizeof(gfx_cmd_header_t); break;
    case GFX_OP_SET_VIEWPORT: fixed_size = sizeof(cmd_set_viewport_t); break;
    case GFX_OP_SET_SCISSOR: fixed_size = sizeof(cmd_set_scissor_t); break;
    case GFX_OP_BIND_PIPELINE: fixed_size = sizeof(cmd_bind_pipeline_t); break;
    case GFX_OP_BIND_VBUFFER: fixed_size = sizeof(cmd_bind_vbuffer_t); break;
    case GFX_OP_BIND_IBUFFER: fixed_size = sizeof(cmd_bind_ibuffer_t); break;
    case GFX_OP_BIND_UNIFORM: fixed_size = sizeof(cmd_bind_uniform_t); break;
    case GFX_OP_BIND_TEXTURE: fixed_size = sizeof(cmd_bind_texture_t); break;
    case GFX_OP_DRAW: fixed_size = sizeof(cmd_draw_t); break;
    case GFX_OP_DRAW_INDEXED: fixed_size = sizeof(cmd_draw_indexed_t); break;
    default:
        return GFX_ERROR_UNKNOWN_OPCODE;
    }
    if (available < fixed_size) {
        return GFX_ERROR_INVALID_RECORD_SIZE;
    }

    switch (opcode) {
    case GFX_OP_WINDOW_CREATE: {
        cmd_window_create_t value;
        memcpy(&value, cmd, sizeof(value));
        payload_size = value.title_len;
        break;
    }
    case GFX_OP_BUFFER_UPLOAD: {
        cmd_buffer_upload_t value;
        memcpy(&value, cmd, sizeof(value));
        payload_size = value.data_size;
        break;
    }
    case GFX_OP_TEXTURE_UPLOAD: {
        cmd_texture_upload_t value;
        memcpy(&value, cmd, sizeof(value));
        payload_size = value.data_size;
        break;
    }
    case GFX_OP_SHADER_CREATE: {
        cmd_shader_create_t value;
        memcpy(&value, cmd, sizeof(value));
        payload_size = value.code_size;
        break;
    }
    case GFX_OP_PIPELINE_CREATE: {
        cmd_pipeline_create_t value;
        memcpy(&value, cmd, sizeof(value));
        payload_size = (uint64_t)value.attr_count * sizeof(gfx_vertex_attr_t);
        break;
    }
    case GFX_OP_BIND_UNIFORM: {
        cmd_bind_uniform_t value;
        memcpy(&value, cmd, sizeof(value));
        payload_size = value.data_size;
        break;
    }
    default: break;
    }

    *fixed_size_out = fixed_size;
    *payload_size_out = payload_size;
    return GFX_OK;
}

gfx_result_t gfx_command_transition(uint16_t opcode, const void *cmd,
    gfx_stream_state_t *state, gfx_handle_t *frame_window)
{
    switch (opcode) {
    case GFX_OP_NOP: break;
    case GFX_OP_FRAME_BEGIN: {
        cmd_frame_t value;
        if (*state != GFX_STREAM_OUTSIDE_FRAME) {
            return GFX_ERROR_INVALID_ORDER;
        }
        memcpy(&value, cmd, sizeof(value));
        *frame_window = value.window_id;
        *state = GFX_STREAM_IN_FRAME;
        break;
    }
    case GFX_OP_FRAME_END: {
        cmd_frame_t value;
        if (*state != GFX_STREAM_IN_FRAME) {
            return GFX_ERROR_INVALID_ORDER;
        }
        memcpy(&value, cmd, sizeof(value));
        if (value.window_id != *frame_window) {
            return GFX_ERROR_FRAME_WINDOW_MISMATCH;
        }
        *frame_window = GFX_NULL_HANDLE;
        *state = GFX_STREAM_OUTSIDE_FRAME;
        break;
    }
    case GFX_OP_PASS_BEGIN:
        if (*state != GFX_STREAM_IN_FRAME) {
            return GFX_ERROR_INVALID_ORDER;
        }
        *state = GFX_STREAM_IN_PASS;
        break;
    case GFX_OP_PASS_END:
        if (*state != GFX_STREAM_IN_PASS) {
            return GFX_ERROR_INVALID_ORDER;
        }
        *state = GFX_STREAM_IN_FRAME;
        break;
    case GFX_OP_SET_VIEWPORT:
    case GFX_OP_SET_SCISSOR:
    case GFX_OP_BIND_PIPELINE:
    case GFX_OP_BIND_VBUFFER:
    case GFX_OP_BIND_IBUFFER:
    case GFX_OP_BIND_UNIFORM:
    case GFX_OP_BIND_TEXTURE:
    case GFX_OP_DRAW:
    case GFX_OP_DRAW_INDEXED:
        if (*state != GFX_STREAM_IN_PASS) {
            return GFX_ERROR_INVALID_ORDER;
        }
        break;
    default:
        if (*state == GFX_STREAM_IN_PASS) {
            return GFX_ERROR_INVALID_ORDER;
        }
        break;
    }
    return GFX_OK;
}

gfx_validation_result_t gfx_validate_stream(const void *cmd_stream, uint32_t stream_bytes)
{
    const uint8_t *bytes = cmd_stream;
    uint32_t offset = 0;
    gfx_handle_t frame_window = GFX_NULL_HANDLE;
    gfx_stream_state_t state = GFX_STREAM_OUTSIDE_FRAME;

    if (!bytes && stream_bytes) {
        return validation_result(GFX_ERROR_NULL_STREAM, 0, UINT16_MAX);
    }

    while (offset < stream_bytes) {
        gfx_cmd_header_t header;
        uint32_t fixed_size;
        uint64_t payload_size;
        gfx_result_t code;
        uint32_t remaining = stream_bytes - offset;
        if (remaining < sizeof(header)) {
            return validation_result(GFX_ERROR_TRUNCATED_HEADER, offset, UINT16_MAX);
        }
        memcpy(&header, bytes + offset, sizeof(header));
        if (header.byte_size < sizeof(header) || (header.byte_size & 3u)) {
            return validation_result(GFX_ERROR_INVALID_RECORD_SIZE, offset, header.opcode);
        }
        if (header.byte_size > remaining) {
            return validation_result(GFX_ERROR_TRUNCATED_RECORD, offset, header.opcode);
        }
        code = gfx_command_layout(header.opcode, bytes + offset, header.byte_size,
            &fixed_size, &payload_size);
        if (code != GFX_OK) {
            return validation_result(code, offset, header.opcode);
        }
        if ((uint64_t)fixed_size + ((payload_size + 3u) & ~(uint64_t)3u)
            != header.byte_size) {
            return validation_result(GFX_ERROR_INVALID_PAYLOAD_SIZE, offset, header.opcode);
        }
        code = gfx_command_transition(header.opcode, bytes + offset, &state, &frame_window);
        if (code != GFX_OK) {
            return validation_result(code, offset, header.opcode);
        }
        offset += header.byte_size;
    }

    if (state == GFX_STREAM_IN_PASS) {
        return validation_result(GFX_ERROR_INCOMPLETE_PASS, offset, UINT16_MAX);
    }
    if (state == GFX_STREAM_IN_FRAME) {
        return validation_result(GFX_ERROR_INCOMPLETE_FRAME, offset, UINT16_MAX);
    }
    return validation_result(GFX_OK, offset, UINT16_MAX);
}
