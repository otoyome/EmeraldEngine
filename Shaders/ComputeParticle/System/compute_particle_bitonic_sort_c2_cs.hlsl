#include "compute_particle_bitonic_sort.hlsli"

groupshared particle_header shared_data[BitonicSortC2Thread * 2];

[numthreads(BitonicSortC2Thread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    int t = dispatch_thread_id.x;
    int wgBits = 2 * BitonicSortC2Thread - 1;

    for (int inc = increment; inc > 0; inc >>= 1)
    {
        int low = t & (inc - 1);
        int i = (t << 1) - low;
        bool reverse = ((direction & i) == 0);
        particle_header x0, x1;

        if (inc == (int) increment)
        {
            x0 = particle_header_buffer[i];
            x1 = particle_header_buffer[i + inc];
        }
        else
        {
            GroupMemoryBarrierWithGroupSync();
            x0 = shared_data[i & wgBits];
            x1 = shared_data[(i + inc) & wgBits];
        }

        particle_header auxa = x0;
        particle_header auxb = x1;
        if (reverse ^ comparer(x0, x1))
        {
            x0 = auxb;
            x1 = auxa;
        }

        if (inc == 1)
        {
            particle_header_buffer[i] = x0;
            particle_header_buffer[i + inc] = x1;
        }
        else
        {
            GroupMemoryBarrierWithGroupSync();
            shared_data[i & wgBits] = x0;
            shared_data[(i + inc) & wgBits] = x1;
        }
    }
}

