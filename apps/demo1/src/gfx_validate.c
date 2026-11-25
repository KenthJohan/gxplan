#include "gfx_commands.h"

#include <string.h>

static gfx_validation_result_t validation_result(
    gfx_validation_code_t code, uint32_t offset, uint16_t opcode)
{
    gfx_validation_result_t result = {code, offset, opcode};
    return result;
}

gfx_validation_result_t gfx_validate_stream(const void *cmd_stream, uint32_t stream_bytes)
{
    const uint8_t *bytes = cmd_stream;
    uint32_t offset = 0;
    gfx_handle_t frame_window = GFX_NULL_HANDLE;
    int in_frame = 0;
    int in_pass = 0;

    if (!bytes && stream_bytes) {
        return validation_result(GFX_VALIDATION_NULL_STREAM, 0, UINT16_MAX);
    }

    while (offset < stream_bytes) {
        gfx_cmd_header_t header;
        uint32_t fixed_size = 0;
        uint64_t payload_size = 0;
        uint32_t remaining = stream_bytes - offset;
        if (remaining < sizeof(header)) {
            return validation_result(GFX_VALIDATION_TRUNCATED_HEADER, offset, UINT16_MAX);
        }
        memcpy(&header, bytes + offset, sizeof(header));
        if (header.byte_size < sizeof(header) || (header.byte_size & 3u)) {
            return validation_result(GFX_VALIDATION_INVALID_RECORD_SIZE, offset, header.opcode);
        }
        if (header.byte_size > remaining) {
            return validation_result(GFX_VALIDATION_TRUNCATED_RECORD, offset, header.opcode);
        }

        switch (header.opcode) {
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
            return validation_result(GFX_VALIDATION_UNKNOWN_OPCODE, offset, header.opcode);
        }
        if (header.byte_size < fixed_size) {
            return validation_result(GFX_VALIDATION_INVALID_RECORD_SIZE, offset, header.opcode);
        }

        switch (header.opcode) {
        case GFX_OP_WINDOW_CREATE: {
            cmd_window_create_t cmd;
            memcpy(&cmd, bytes + offset, sizeof(cmd));
            payload_size = cmd.title_len;
            break;
        }
        case GFX_OP_BUFFER_UPLOAD: {
            cmd_buffer_upload_t cmd;
            memcpy(&cmd, bytes + offset, sizeof(cmd));
            payload_size = cmd.data_size;
            break;
        }
        case GFX_OP_TEXTURE_UPLOAD: {
            cmd_texture_upload_t cmd;
            memcpy(&cmd, bytes + offset, sizeof(cmd));
            payload_size = cmd.data_size;
            break;
        }
        case GFX_OP_SHADER_CREATE: {
            cmd_shader_create_t cmd;
            memcpy(&cmd, bytes + offset, sizeof(cmd));
            payload_size = cmd.code_size;
            break;
        }
        case GFX_OP_PIPELINE_CREATE: {
            cmd_pipeline_create_t cmd;
            memcpy(&cmd, bytes + offset, sizeof(cmd));
            payload_size = (uint64_t)cmd.attr_count * sizeof(gfx_vertex_attr_t);
            break;
        }
        case GFX_OP_BIND_UNIFORM: {
            cmd_bind_uniform_t cmd;
            memcpy(&cmd, bytes + offset, sizeof(cmd));
            payload_size = cmd.data_size;
            break;
        }
        default: break;
        }

        if ((uint64_t)fixed_size + ((payload_size + 3u) & ~(uint64_t)3u)
            != header.byte_size) {
            return validation_result(GFX_VALIDATION_INVALID_PAYLOAD_SIZE, offset, header.opcode);
        }

        switch (header.opcode) {
        case GFX_OP_NOP: break;
        case GFX_OP_FRAME_BEGIN: {
            cmd_frame_t cmd;
            if (in_frame) {
                return validation_result(GFX_VALIDATION_INVALID_ORDER, offset, header.opcode);
            }
            memcpy(&cmd, bytes + offset, sizeof(cmd));
            frame_window = cmd.window_id;
            in_frame = 1;
            break;
        }
        case GFX_OP_FRAME_END: {
            cmd_frame_t cmd;
            if (!in_frame || in_pass) {
                return validation_result(GFX_VALIDATION_INVALID_ORDER, offset, header.opcode);
            }
            memcpy(&cmd, bytes + offset, sizeof(cmd));
            if (cmd.window_id != frame_window) {
                return validation_result(GFX_VALIDATION_FRAME_WINDOW_MISMATCH, offset, header.opcode);
            }
            in_frame = 0;
            break;
        }
        case GFX_OP_PASS_BEGIN:
            if (!in_frame || in_pass) {
                return validation_result(GFX_VALIDATION_INVALID_ORDER, offset, header.opcode);
            }
            in_pass = 1;
            break;
        case GFX_OP_PASS_END:
            if (!in_pass) {
                return validation_result(GFX_VALIDATION_INVALID_ORDER, offset, header.opcode);
            }
            in_pass = 0;
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
            if (!in_pass) {
                return validation_result(GFX_VALIDATION_INVALID_ORDER, offset, header.opcode);
            }
            break;
        default:
            if (in_pass) {
                return validation_result(GFX_VALIDATION_INVALID_ORDER, offset, header.opcode);
            }
            break;
        }
        offset += header.byte_size;
    }

    if (in_pass) {
        return validation_result(GFX_VALIDATION_INCOMPLETE_PASS, offset, UINT16_MAX);
    }
    if (in_frame) {
        return validation_result(GFX_VALIDATION_INCOMPLETE_FRAME, offset, UINT16_MAX);
    }
    return validation_result(GFX_VALIDATION_OK, offset, UINT16_MAX);
}
