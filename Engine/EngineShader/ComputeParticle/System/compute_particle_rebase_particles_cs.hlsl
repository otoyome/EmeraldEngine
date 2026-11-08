#include "compute_particle.hlsli"

RWByteAddressBuffer particle_binary_buffer : register(u0);

StructuredBuffer<emitter_header> emitter_header_buffer : register(t0);
ByteAddressBuffer emitter_binary_buffer : register(t1);
StructuredBuffer<particle_header> particle_header_buffer : register(t2);

#define SRV emitter_binary_buffer
#include "compute_particle_system_item_emitter.hlsli"

#define UAV particle_binary_buffer
#include "compute_particle_system_item_particle.hlsli"

[numthreads(NumDispatchThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint header_index = dispatch_thread_id.x;
    if (header_index >= total_particle_count)
        return;

    //  パーティクルヘッダー取得
    particle_header header = particle_header_buffer[header_index];
    if (header.tag == invalid_tag)
        return;

    bool alive;
    uint emitter_id, particle_index;
    get_particle_header_parameter(header, alive, emitter_id, particle_index);
    if (!alive)
        return;

    //  バイナリ座標を特定
    uint emitter_binary_offset = emitter_header_buffer[emitter_id].emitter_binary_head;
    uint particle_binary_offset = emitter_header_buffer[emitter_id].particle_size * particle_index;
    particle_binary_offset += emitter_header_buffer[emitter_id].particle_binary_head;

    //  エミッターの有効アイテム
    em_common_item em_common;
    stream_common_item(rw_read_only, emitter_binary_offset, em_common);

    //  共通アイテム
    {
        pt_common_item pt_common;
        uint write_offset = particle_binary_offset;
        stream_common_item(rw_read, particle_binary_offset, pt_common);
        pt_common.translation -= simulation_rebase_offset;
        stream_common_item(rw_write, write_offset, pt_common);
    }

    //  軌跡アイテム
    if (has_flag(em_common.active_items, emitter_active_trail_item_bit_flag))
    {
        em_trail_item em_trail;
        stream_trail_item(rw_read_only, emitter_binary_offset, em_trail);

        pt_trail_header_item trail_header;
        stream_trail_header(rw_read, particle_binary_offset, trail_header);
        [loop] for (uint index = 0; index < em_trail.max; ++index)
        {
            pt_trail_body_item trail_body;
            uint write_offset = particle_binary_offset;
            stream_trail_body(rw_read, particle_binary_offset, trail_body);
            trail_body.translation -= simulation_rebase_offset;
            stream_trail_body(rw_write, write_offset, trail_body);
        }
    }

    //  座標を持たないアイテムは読み飛ばす
    if (has_flag(em_common.active_items, emitter_active_uv_item_bit_flag))
    {
        pt_uv_item pt_uv;
        stream_uv_item(rw_read, particle_binary_offset, pt_uv);
    }
    if (has_flag(em_common.active_items, emitter_active_veloctiy_item_bit_flag))
    {
        pt_velocity_item pt_velocity;
        stream_velocity_item(rw_read, particle_binary_offset, pt_velocity);
    }
    if (has_flag(em_common.active_items, emitter_active_attract_item_bit_flag))
    {
        pt_attract_item pt_attract;
        stream_attract_item(rw_read, particle_binary_offset, pt_attract);
    }

    //  イベントアイテム
    if (has_flag(em_common.active_items, emitter_active_event_item_bit_flag))
    {
        pt_event_item pt_event;
        uint write_offset = particle_binary_offset;
        stream_event_item(rw_read, particle_binary_offset, pt_event);
        pt_event.previous_position -= simulation_rebase_offset;
        stream_event_item(rw_write, write_offset, pt_event);
    }
}
