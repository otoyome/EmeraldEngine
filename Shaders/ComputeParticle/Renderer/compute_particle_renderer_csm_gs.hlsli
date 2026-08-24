#ifndef __COMPUTE_PARTICLES_DEFAULT_CSM_HLSLI__
#define __COMPUTE_PARTICLES_DEFAULT_CSM_HLSLI__

#include "compute_particle_csm.hlsli"
#include "scene_constant_buffer.hlsli"

//  微小ポリゴンカリング
bool minimum_primitive_culling(float4 pos[3])
{
    float2 min_position = min(pos[0].xy, min(pos[1].xy, pos[2].xy));
    float2 max_position = max(pos[0].xy, max(pos[1].xy, pos[2].xy));
    min_position *= get_viewport_size();
    max_position *= get_viewport_size();
    return any(round(min_position) == round(max_position));
}

[maxvertexcount(3 * shadow_map_max)]
void main(triangle VS_IN input[3], inout TriangleStream<GS_CSM_OUT> output)
{
    uint i, j, k;

    [unroll] for (k = 0; k < shadow_map_max; k++)
    {
        if (k >= shadow_map_count)
            break;

        float4x4 view_projection = light_view_projection_transform[k];
        //	カリング
        float4 positions[3];
        [unroll] for (i = 0; i < 3; i++)
        {
            positions[i] = mul(float4(input[i].position.xyz, 1.0f), view_projection);
            positions[i] /= positions[i].w;
        }

        //  微小ポリゴンはカリングする
        if (minimum_primitive_culling(positions))
            break;	//	普通にカリングするならcontinueでも良いと思う

        //  シャドウマップ適応
        float4 offset_parameter = shadow_bias[k];
        [unroll] for (i = 0; i < 3; i++)
        {
            GS_CSM_OUT element = (GS_CSM_OUT) 0;
            element.render_target_array_index = k;
            element.vout.position = positions[i];
            element.vout.w_position = input[i].position;
            element.vout.color = input[i].color;
            element.vout.texcoord = input[i].texcoord;

            output.Append(element);
        }
        output.RestartStrip();
    }
}

#endif  //  __COMPUTE_PARTICLES_DEFAULT_CSM_HLSLI__
