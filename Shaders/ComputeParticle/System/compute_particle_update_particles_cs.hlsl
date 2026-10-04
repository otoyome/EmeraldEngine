#include "compute_particle_update_items.hlsli"

[numthreads(NumDispatchThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint header_index = dispatch_thread_id.x;
    if (header_index >= total_particle_count)
        return;

    //  パーティクルヘッダー取得
    particle_header header = particle_header_buffer.Load(header_index);
    if (header.tag == invalid_tag)
        return;

    //  パーティクルヘッダーから情報取得
    bool alive;
    uint emitter_id, particle_index;
    get_particle_header_parameter(header, alive, emitter_id, particle_index);
    if (!alive)
        return;

    //  破棄Idリスト
    bool is_erased = false;
    [loop] for (uint index = 0; index < erase_id_count; ++index)
    {
        is_erased = is_erased || (erase_id_list[index / 4][index % 4] == emitter_id);
    }
    alive = alive && !is_erased;

    //  エミッターバイナリ座標を特定
    uint emitter_binary_offset = emitter_header_buffer[emitter_id].emitter_binary_head;

    //  パーティクルバイナリ座標を特定
    uint particle_binary_offset = emitter_header_buffer[emitter_id].particle_size * particle_index;
    particle_binary_offset += emitter_header_buffer[emitter_id].particle_binary_head;

    //  エミッターコンテキスト構築
    emitter_context em_context = build_emitter_context(emitter_binary_offset, emitter_id, dispatch_thread_id);

    //  パーティクル毎の共通アイテム取得
    pt_common_item pt_common;
    uint particle_common_write_binary_offset = particle_binary_offset; //  書き込み座標を事前に保存しておく
    stream_common_item(rw_read, particle_binary_offset, pt_common);

    //  アイテム更新処理
    {
        update_particle_trail_item(em_context, pt_common, emitter_binary_offset, particle_binary_offset);
        update_uv_item(em_context, pt_common, emitter_binary_offset, particle_binary_offset);
        update_color_item(em_context, pt_common, emitter_binary_offset, particle_binary_offset);
        update_velocity_item(em_context, pt_common, emitter_binary_offset, particle_binary_offset);
        update_attract_item(em_context, pt_common, emitter_binary_offset, particle_binary_offset);
        update_event_item(em_context, pt_common, emitter_binary_offset, particle_binary_offset);

        //  生存時間更新
        if (alive)
        {
            pt_common.life_time += em_context.elapsed_time;
            alive = pt_common.life_time < pt_common.life_time_max;
            clear_flag(pt_common.state, particle_binary_state_bit_spawn_frame);  //  生成フレームフラグを落とす
        }
    }

    //  パーティクル毎の共通アイテム保存
    stream_common_item(rw_write, particle_common_write_binary_offset, pt_common);

    //  ヘッダー更新
    {
        header.tag = set_particle_header_tag(alive, emitter_id, particle_index);
        header.depth = mul(float4(pt_common.translation, 1), view_transform).z;
        //  不透明/半透明用に逆深度ソート対応
        header.depth = em_context.reverse_depth ? -header.depth : header.depth;	//	遠クリップを使わず符号反転で逆順にする
        particle_header_buffer[header_index] = header;
    }
}
