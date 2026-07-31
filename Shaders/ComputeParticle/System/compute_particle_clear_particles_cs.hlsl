#include "compute_particle.hlsli"

RWStructuredBuffer<particle_header> particle_header_buffer : register(u0); //  パーティクルヘッダーバッファ

[numthreads(NumDispatchThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    int index = dispatch_thread_id.x;
    if (total_particle_count < index)
        return;

    particle_header_buffer[index].tag = invalid_tag;
    particle_header_buffer[index].depth = 0;
}
