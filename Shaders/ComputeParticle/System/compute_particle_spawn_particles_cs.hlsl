#include "compute_particle.hlsli"

//	汎用情報定義
RWStructuredBuffer<particle_header> particle_header_buffer : register(u0);
RWStructuredBuffer<emitter_range> emitter_range_buffer : register(u1);
RWStructuredBuffer<emitter_data> emitter_data_buffer : register(u2);
RWByteAddressBuffer indirect_data_buffer : register(u3);

StructuredBuffer<uint> emitter_table_buffer : register(t0);
StructuredBuffer<emitter_header> emitter_header_buffer : register(t1);
ByteAddressBuffer emitter_binary_buffer : register(t2);

//  エミッターバイナリーからアイテム情報を取得するので
#define SRV emitter_binary_buffer
#include "compute_particle_system_item_emitter.hlsli"

[numthreads(1, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint emitter_index = emitter_table_buffer.Load(dispatch_thread_id.x);
    if (emitter_index == invalid_tag)
        return;

    emitter_header em_header = emitter_header_buffer[emitter_index];

    //  レンジ初期化
    emitter_range_buffer[emitter_index] = (emitter_range) 0;

    //  エミッターバイナリ座標を特定
    uint emitter_binary_offset = em_header.emitter_binary_head;

    //  commonアイテム取得して発生数を取得
    em_common_item common;
    stream_common_item(rw_read_only, emitter_binary_offset, common);

    //  パーティクルヘッダーを割り当てる
    emitter_data em_data = emitter_data_buffer[emitter_index];
    em_data.spawn_num = em_data.spawn_head = 0;
    if (common.spawn_count > 0 && common.elapsed_time > 0.0f)
    {
        //  発生数をクランプ
        uint spawn_max = em_data.particle_max - em_data.particle_num;
        if (spawn_max > 0 && spawn_max <= em_data.particle_max)
        {
            spawn_max = min(common.spawn_count, spawn_max);

            //  パーティクルの末尾の番号を取得(ここの同期命令を無くしたい)
            uint spawn_particle_head = 0;
            indirect_data_buffer.InterlockedAdd(NumPreviousParticleOffset, spawn_max, spawn_particle_head);

            //  エミッターデータにパーティクルヘッダーの割り当てアドレスと発生数を記録
            uint current_particle_count = indirect_data_buffer.Load(NumCurrentParticleOffset);
            if (spawn_particle_head < current_particle_count)
            {
                em_data.spawn_num = spawn_max;
                em_data.spawn_head = spawn_particle_head;

                //  パーティクルヘッダーの使用場所を違反タグで埋めておく
                particle_header pt_header;
                pt_header.tag = invalid_tag;
                pt_header.depth = 0.0f;
                [loop] for (uint index = spawn_particle_head; index < spawn_particle_head + spawn_max; ++index)
                {
                    particle_header_buffer[index] = pt_header;
                }
            }
        }
    }
    
    //  データ書き込み
    emitter_data_buffer[emitter_index] = em_data;
}
