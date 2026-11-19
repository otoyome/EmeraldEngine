#include "sprite.hlsli"
#include "sprite_vertex.hlsli"

VS_OUT main(uint vertex_id : SV_VertexID)
{
    VS_OUT vout;

    //	頂点読込
    SPRITE_VERTEX vin = load_sprite_vertex(vertex_id);
    float3 position = vin.position;
    float4 color = vin.color;
    float2 texcoord = vin.texcoord;
    
    float2 screen_size = viewport_rect.zw - viewport_rect.xy;
    
    //  頂点情報をスクリーン座標に変換
    {
        //  位置情報を算出
        float2 p = screen_size * transform_matrix._11_12;
        float2 s = screen_size * transform_matrix._13_14;
        float4 screen_rect = float4(p, s);
        vout.position.xy = screen_rect.xy + screen_rect.zw * position.xy;
        vout.position.z = 0.0f;
        vout.position.w = 1.0f;

	    //	中心点算出
        float2 center = screen_rect.xy + screen_rect.zw * transform_matrix._23_24;

	    //	回転
        vout.position.xy -= center.xy;
	    {
            float2 r = vout.position.xy;
            vout.position.x = transform_matrix._21 * r.x - transform_matrix._22 * r.y;
            vout.position.y = transform_matrix._22 * r.x + transform_matrix._21 * r.y;
        }
        vout.position.xy += center.xy;
    }

	//	NDC座標に変換
    vout.position.x = 2.0f * vout.position.x / screen_size.x - 1.0f;
    vout.position.y = 1.0f - 2.0f * vout.position.y / screen_size.y;

	//	UV座標設定
    vout.color = color * transform_matrix._41_42_43_44;
    vout.texcoord.x = transform_matrix._31 + transform_matrix._33 * texcoord.x;
    vout.texcoord.y = transform_matrix._32 + transform_matrix._34 * texcoord.y;
	return vout;
}
