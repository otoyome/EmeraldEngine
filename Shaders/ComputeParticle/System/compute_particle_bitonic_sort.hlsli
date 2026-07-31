
#include "compute_particle.hlsli"

//	バイトニックソート情報
cbuffer COMPUTE_PARTICLE_BITONIC_SORT_CONSTANT_BUFFER : register(b12)
{
    uint increment;
    uint direction;
    uint sort_dummy[2];
};

static const uint BitonicSortB2Thread = 256;
static const uint BitonicSortC2Thread = 512;

RWStructuredBuffer<particle_header> particle_header_buffer : register(u0);

bool comparer(in particle_header x0, in particle_header x1)
{
    uint first_key = x0.tag >> tag_particle_index_bit_count;
    uint second_key = x1.tag >> tag_particle_index_bit_count;
    bool flag = (first_key > second_key) || ((first_key == second_key) && (x0.depth < x1.depth));
    return !flag;
}
