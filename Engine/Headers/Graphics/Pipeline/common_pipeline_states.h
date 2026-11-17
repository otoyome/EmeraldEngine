#pragma once

#include <string>
#include <string_view>
#include <d3d11.h>

namespace common_pipeline_states
{
	//	SRV数
	constexpr UINT shader_resource_view_max = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;

	//	UAV数
	constexpr UINT unordered_access_view_max = 8;//	FEATURELEVEL11.1 : D3D11_1_UAV_SLOT_COUNT

	//	共通CBV/SRV/Samplerインデックス
	constexpr UINT scene_cbv_index = 0;					//	シーン定数バッファ番号
	constexpr UINT primitive_cbv_index = 1;				//	プリミティブ定数バッファ番号
	constexpr UINT geometric_primitive_cbv_index = 1;	//	ジオメトリックプリミティブ定数バッファ番号
	constexpr UINT parameter_cbv_index = 2;				//	パラメーター定数バッファ番号
	constexpr UINT light_cbv_index = 3;					//	ライトの定数バッファ番号
	constexpr UINT shadowmap_cbv_index = 4;				//	シャドウマップの定数バッファ番号
	constexpr UINT meshlet_cbv_index = 5;				//	メッシュレット定数バッファ番号
	constexpr UINT light_probe_cbv_index = 6;			//	ライトプローブの定数バッファ番号
	constexpr UINT shadowmap_srv_index = 10;			//	シャドウマップのSRV番号
	constexpr UINT shadowmap_sampler_index = 10;		//	シャドウマップのSampler番号

	//	デカールステンシルID
	constexpr UINT decal_stencil_ref = 0x80;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	深度バッファ関係
	//	@remainder	シェーダー側の値はcommon.hlsliと合わせること
	constexpr float depth_near_value = math::is_reverse_depth() ? 1.0f : 0.0f;	//	near面の射影深度
	constexpr float depth_far_value = math::is_reverse_depth() ? 0.0f : 1.0f;	//	far面(無限遠)の射影深度
	constexpr float depth_clear_value = depth_far_value;						//	深度バッファのクリア値
	constexpr auto depth_comparison_func = math::is_reverse_depth()	//	深度テストの比較関数
										 ? D3D11_COMPARISON_GREATER_EQUAL
										 : D3D11_COMPARISON_LESS_EQUAL;

	constexpr float gbuffer_max_linear_depth = 1.0e10f;	//	G-Bufferに保存する線形深度(ビュー空間)の上限。空(無限遠)はこの値で表す

	//	シーン深度ステンシルバッファのフォーマット
	constexpr DXGI_FORMAT scene_depth_texture_format = DXGI_FORMAT_R32G8X24_TYPELESS;
	constexpr DXGI_FORMAT scene_depth_stencil_view_format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	constexpr DXGI_FORMAT scene_depth_shader_resource_view_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

	//	シャドウマップのフォーマット
	constexpr DXGI_FORMAT shadow_map_texture_format = DXGI_FORMAT_R32_TYPELESS;
	constexpr DXGI_FORMAT shadow_map_depth_stencil_view_format = DXGI_FORMAT_D32_FLOAT;
	constexpr DXGI_FORMAT shadow_map_shader_resource_view_format = DXGI_FORMAT_R32_FLOAT;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	シーン情報
	constexpr std::string_view scene_constant_buffer_name = "SCENE_CONSTANT_BUFFER";
	constexpr system_utility::hash_key scene_constant_buffer_hash_key = system_utility::generate_hash(scene_constant_buffer_name);
	struct scene_constants
	{
		math::float4 time_data;	//	x : 現在時間, y : 前時間, zw : 空き
		math::float4 z_buffer_parameteres;
		math::float4 camera_position; //  xyz : 座標, w : fovY
		math::float4 camera_right; //  xyz : 方向, w : tan(fovY)
		math::float4 camera_up; //  xyz : 方向, w : 空き
		math::float4 camera_direction; //  xyz : 方向, w : 空き
		math::float4 camera_clip_distance;
		math::float4 viewport_rect;
		math::float4x4 view_transform;
		math::float4x4 projection_transform;
		math::float4x4 view_projection_transform;
		math::float4x4 inverse_view_transform;
		math::float4x4 inverse_projection_transform;
		math::float4x4 inverse_view_projection_transform;
		math::float4x4 previous_view_projection_transform;

		//	ディスプレイマッピング情報
		UINT output_curve;			//	出力の伝達関数(display_mapping_renderer::output_curves)
		float peak_nits;			//	ディスプレイの最大輝度[nit]
		math::float2 display_mapping_dummy;
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	ライト定数バッファ構造体
	constexpr std::string_view deferred_light_constant_buffer_name = "DEFERREDLIGHT_CONSTANT_BUFFER";
	constexpr system_utility::hash_key deferred_light_constant_buffer_hash_key = system_utility::generate_hash(deferred_light_constant_buffer_name);
	struct deferred_light_constants
	{
		light_data light_data;
	};

	//	ライトプローブ定数バッファ構造体
	constexpr std::string_view deferred_light_probe_constant_buffer_name = "DEFERRED_LIGHT_PROBE_CONSTANT_BUFFER";
	constexpr system_utility::hash_key deferred_light_probe_constant_buffer_hash_key = system_utility::generate_hash(deferred_light_probe_constant_buffer_name);
	struct deferred_light_probe_constants
	{
		light_probes light_probe_data;
		UINT light_probe_attribute;
		math::float3 light_probe_dummy;
	};

	//	フォワード用ライト定数バッファ構造体
	constexpr UINT directional_light_count = 4;
	constexpr UINT point_light_count = 8;
	constexpr UINT spot_light_count = 8;
	constexpr std::string_view forward_light_constant_buffer_name = "FORWARD_LIGHTS_CONSTANT_BUFFER";
	constexpr system_utility::hash_key forward_light_constant_buffer_hash_key = system_utility::generate_hash(forward_light_constant_buffer_name);
	struct forward_light_constants
	{
		math::uint4 light_counts;
		light_data directional_light_data[directional_light_count];
		light_data point_light_data[point_light_count];
		light_data spot_light_data[spot_light_count];
		light_probes light_probe_data;
	};

	constexpr std::string_view forward_only_diffuse_iem_name = "forward_only_diffuse_iem";
	constexpr system_utility::hash_key forward_only_diffuse_iem_hash_key = system_utility::generate_hash(forward_only_diffuse_iem_name);
	constexpr std::string_view forward_only_specular_pmrem_name = "forward_only_specular_pmrem";
	constexpr system_utility::hash_key forward_only_specular_pmrem_hash_key = system_utility::generate_hash(forward_only_specular_pmrem_name);
	constexpr std::string_view forward_only_lut_ggx_name = "forward_only_lut_ggx";
	constexpr system_utility::hash_key forward_only_lut_ggx_hash_key = system_utility::generate_hash(forward_only_lut_ggx_name);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	影カテゴリー
	enum class shadow_category
	{
		default_id = 0,
		mask_1,
		mask_2,
		mask_3,
		mask_4,
		mask_5,
		mask_6,
		mask_7,
		max = 8,	// 8ビットまでなので
	};
	//	シャドウマップ情報
	constexpr UINT shadow_map_max = 6;
	constexpr std::string_view shadow_map_constant_buffer_name = "SHADOW_MAP_CONSTANT_BUFFER";
	constexpr system_utility::hash_key shadow_map_constant_buffer_hash_key = system_utility::generate_hash(shadow_map_constant_buffer_name);
	struct shadow_map_constants
	{
		UINT shadow_map_count = 0;
		math::float3 shadow_attenuation;
		math::float4 shadow_bias[shadow_map_max];
		math::float4 shadow_split_distance[shadow_map_max];
		math::float4x4 light_view_projection_transform[shadow_map_max];
		UINT shadow_mask;	//	8ビットまで
		UINT shadow_slice_offset;	//	シャドウマップアトラスを使うときの先頭スライス
		math::float2 shadow_dummy;
	};
	constexpr std::string_view shadow_maps_name = "shadow_maps";
	constexpr system_utility::hash_key shadow_maps_hash_key = system_utility::generate_hash(shadow_maps_name);
	constexpr std::string_view shadow_maps_sampler_state_name = "shadow_maps_sampler_state";
	constexpr system_utility::hash_key shadow_maps_sampler_state_hash_key = system_utility::generate_hash(shadow_maps_sampler_state_name);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	クラスタードライティング情報
	constexpr std::string_view clustered_lighting_constant_buffer_name = "CLUSTERED_LIGHTING_CONSTANT_BUFFER";
	constexpr system_utility::hash_key clustered_lighting_constant_buffer_hash_key = system_utility::generate_hash(clustered_lighting_constant_buffer_name);
	constexpr std::string_view clustered_lights_name = "clustered_lights";
	constexpr system_utility::hash_key clustered_lights_hash_key = system_utility::generate_hash(clustered_lights_name);
	constexpr std::string_view cluster_counts_name = "cluster_counts";
	constexpr system_utility::hash_key cluster_counts_hash_key = system_utility::generate_hash(cluster_counts_name);
	constexpr std::string_view cluster_offsets_name = "cluster_offsets";
	constexpr system_utility::hash_key cluster_offsets_hash_key = system_utility::generate_hash(cluster_offsets_name);
	constexpr std::string_view cluster_light_indices_name = "cluster_light_indices";
	constexpr system_utility::hash_key cluster_light_indices_hash_key = system_utility::generate_hash(cluster_light_indices_name);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	プリミティブ定数バッファ構造体
	constexpr UINT mesh_bone_max = 500;	//	1メッシュ500本あれば大丈夫かな？
	constexpr UINT morph_max = 8;
	constexpr std::string_view primitive_constant_buffer_name = "PRIMITIVE_CONSTANT_BUFFER";
	constexpr system_utility::hash_key primitive_constant_buffer_hash_key = system_utility::generate_hash(primitive_constant_buffer_name);
	//	頂点属性の番号
	enum class vertex_elements : UINT
	{
		position,
		normal,
		tangent,
		color0,
		texcoord0,
		texcoord1,
		joints0,
		joints1,
		joints2,
		joints3,
		weights0,
		weights1,
		weights2,
		weights3,

		max,
	};
	constexpr UINT vertex_element_capacity = 16;	//	static_cast<UINT>(vertex_elements::max);

	//	属性の所持ビット
	constexpr UINT vertex_element_bit(vertex_elements element) { return 1u << static_cast<UINT>(element); }

	struct primitive_header
	{
		math::uint4 primitive_parameter = {};							//	x : 未使用（接線有無は has_element へ移行）, y : 位置モーフ数, z : 法線モーフ数, w : 未使用
		UINT bone_offset = 0;											//	今回ボーン先頭位置
		UINT previous_bone_offset = 0;									//	前回ボーン先頭位置
		UINT bone_count = 0;											//	ボーン数
		UINT has_element = 0;											//	頂点属性の所持ビット
		math::uint4 element_bases[vertex_element_capacity / 4] = {};	//	属性毎の頂点プール内での先頭位置
		math::float4 position_morph[morph_max / 4];						//	座標モーフオフセット
		math::float4 normal_morph[morph_max / 4];						//	法線オフセット
	};
	static_assert(sizeof(primitive_header) % 16 == 0, "primitive_header must be 16 byte aligned (StructuredBuffer stride must match HLSL PrimitiveHeader)");

	//	メッシュレットインスタンス情報
	struct meshlet_instance_record
	{
		UINT vertex_offset = 0;
		UINT triangle_offset = 0;
		UINT triangle_count = 0;
		UINT primitive_index = 0;
		math::float3 bounds_center = {};
		float bounds_radius = 0.0f;
		UINT transform_index = 0;
		UINT no_cull = 0;
		UINT pipeline_group = 0;
		UINT padding = 0;
	};
	constexpr UINT morph_srv_position_index = 80;	//	モーフィング用座標
	constexpr UINT morph_srv_normal_index = morph_srv_position_index + morph_max * 1;		//	モーフィング用法線
	constexpr std::string_view morph_positions_name = "morph_positions";
	constexpr system_utility::hash_key morph_positions_hash_key = system_utility::generate_hash(morph_positions_name);
	constexpr std::string_view morph_normals_name = "morph_normals";
	constexpr system_utility::hash_key morph_normals_hash_key = system_utility::generate_hash(morph_normals_name);
	constexpr std::string_view primitive_headers_name = "primitive_headers";
	constexpr system_utility::hash_key primitive_headers_hash_key = system_utility::generate_hash(primitive_headers_name);
	constexpr std::string_view bone_matrices_name = "bone_matrices";
	constexpr system_utility::hash_key bone_matrices_hash_key = system_utility::generate_hash(bone_matrices_name);
	constexpr std::string_view meshlet_positions_name = "meshlet_positions";
	constexpr system_utility::hash_key meshlet_positions_hash_key = system_utility::generate_hash(meshlet_positions_name);
	constexpr std::string_view meshlet_normals_name = "meshlet_normals";
	constexpr system_utility::hash_key meshlet_normals_hash_key = system_utility::generate_hash(meshlet_normals_name);
	constexpr std::string_view meshlet_tangents_name = "meshlet_tangents";
	constexpr system_utility::hash_key meshlet_tangents_hash_key = system_utility::generate_hash(meshlet_tangents_name);
	constexpr std::string_view meshlet_colors0_name = "meshlet_colors0";
	constexpr system_utility::hash_key meshlet_colors0_hash_key = system_utility::generate_hash(meshlet_colors0_name);
	constexpr std::string_view meshlet_texcoords0_name = "meshlet_texcoords0";
	constexpr system_utility::hash_key meshlet_texcoords0_hash_key = system_utility::generate_hash(meshlet_texcoords0_name);
	constexpr std::string_view meshlet_texcoords1_name = "meshlet_texcoords1";
	constexpr system_utility::hash_key meshlet_texcoords1_hash_key = system_utility::generate_hash(meshlet_texcoords1_name);
	constexpr std::string_view meshlet_joints0_name = "meshlet_joints0";
	constexpr system_utility::hash_key meshlet_joints0_hash_key = system_utility::generate_hash(meshlet_joints0_name);
	constexpr std::string_view meshlet_joints1_name = "meshlet_joints1";
	constexpr system_utility::hash_key meshlet_joints1_hash_key = system_utility::generate_hash(meshlet_joints1_name);
	constexpr std::string_view meshlet_joints2_name = "meshlet_joints2";
	constexpr system_utility::hash_key meshlet_joints2_hash_key = system_utility::generate_hash(meshlet_joints2_name);
	constexpr std::string_view meshlet_joints3_name = "meshlet_joints3";
	constexpr system_utility::hash_key meshlet_joints3_hash_key = system_utility::generate_hash(meshlet_joints3_name);
	constexpr std::string_view meshlet_weights0_name = "meshlet_weights0";
	constexpr system_utility::hash_key meshlet_weights0_hash_key = system_utility::generate_hash(meshlet_weights0_name);
	constexpr std::string_view meshlet_weights1_name = "meshlet_weights1";
	constexpr system_utility::hash_key meshlet_weights1_hash_key = system_utility::generate_hash(meshlet_weights1_name);
	constexpr std::string_view meshlet_weights2_name = "meshlet_weights2";
	constexpr system_utility::hash_key meshlet_weights2_hash_key = system_utility::generate_hash(meshlet_weights2_name);
	constexpr std::string_view meshlet_weights3_name = "meshlet_weights3";
	constexpr system_utility::hash_key meshlet_weights3_hash_key = system_utility::generate_hash(meshlet_weights3_name);
	constexpr std::string_view meshlet_vertex_indices_name = "meshlet_vertex_indices";
	constexpr system_utility::hash_key meshlet_vertex_indices_hash_key = system_utility::generate_hash(meshlet_vertex_indices_name);
	constexpr std::string_view meshlet_triangle_indices_name = "meshlet_triangle_indices";
	constexpr system_utility::hash_key meshlet_triangle_indices_hash_key = system_utility::generate_hash(meshlet_triangle_indices_name);
	constexpr std::string_view visible_meshlet_instances_name = "visible_meshlet_instances";
	constexpr system_utility::hash_key visible_meshlet_instances_hash_key = system_utility::generate_hash(visible_meshlet_instances_name);

	//	モデル以外の頂点ストリーム(UI等)
	//	Engine/EngineShader/Common/vertex_stream.hlsli と合わせる
	constexpr UINT vertex_stream_srv_index = 96;	//	先頭の SRV 番号
	constexpr UINT vertex_stream_max = 3;			//	ストリーム数
	constexpr std::string_view vertex_stream0_name = "vertex_stream0";
	constexpr std::string_view vertex_stream1_name = "vertex_stream1";
	constexpr std::string_view vertex_stream2_name = "vertex_stream2";
	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	パラメーター定数バッファ構造体
	constexpr std::string_view parameter_constant_buffer_name = "PARAMETER_CONSTANT_BUFFER";
	constexpr system_utility::hash_key parameter_constant_buffer_hash_key = system_utility::generate_hash(parameter_constant_buffer_name);
	struct parameter_constants
	{
		math::int4 shader_resource_view_index[shader_resource_view_max / 4]; // SRV使用フラグ
		math::uint4 shader_resource_view_flags[shader_resource_view_max / 4]; // SRVフラグ類

		UINT material_flags; // マテリアルフラグ類
		float material_alpha_clip_threshold; // 透明/不透明閾値
		UINT material_shading_model; // シェーディングモデル
		UINT material_blend_mode; // ブレンドモード

		parameter_constants()
		{
			memset(shader_resource_view_index, -1, sizeof(shader_resource_view_index));
			memset(shader_resource_view_flags, 0, sizeof(shader_resource_view_flags));
			material_flags = 0;
			material_shading_model = 0;
			material_alpha_clip_threshold = 0.1f;
			material_blend_mode = 0;
		}
	};
	//	シェーダーリソースフラグ
	constexpr UINT shader_resource_view_flags_srgb = 1 << 0;  //  sRGB色空間の情報フラグ
	constexpr UINT shader_resource_view_flags_gamut_rec709 = 1 << 2;	//	テクスチャの色域 Rec.709
	constexpr UINT shader_resource_view_flags_gamut_rec2020 = 1 << 3;	//	テクスチャの色域 Rec.2020
	constexpr UINT shader_resource_view_flags_cube_map = 1 << 1;  //  キューブマップフラグ

	//  マテリアルアトリビュート
	constexpr UINT material_attribute_indirect_lighting_off = 1 << 0;  //  間接光の処理を適応させない
	constexpr UINT material_attribute_shadow_receiver = 1 << 1;	//	影適応
	constexpr UINT material_attribute_decal_off = 1 << 2;	//	デカールを適応させない

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	デカール用定数バッファ構造体
	constexpr std::string_view decal_primitive_constant_buffer_name = "DECAL_PRIMITIVE_CONSTANT_BUFFER";
	constexpr system_utility::hash_key decal_primitive_constant_buffer_hash_key = system_utility::generate_hash(decal_primitive_constant_buffer_name);
	struct decal_primitive_constants
	{
		math::float4x4 decal_transform;
		math::float4x4 decal_inverse_transform;
		math::float4 decal_direction;
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	シーン関係
	constexpr std::string_view scene_color_map_name = "scene_color_map";
	constexpr std::string_view scene_depth_map_name = "scene_depth_map";
	constexpr system_utility::hash_key scene_color_map_hash_key = system_utility::generate_hash(scene_color_map_name);
	constexpr system_utility::hash_key scene_depth_map_hash_key = system_utility::generate_hash(scene_depth_map_name);
	//	G-Buffer関連
	constexpr std::string_view gbuffer_base_color = "gbuffer_base_color";
	constexpr std::string_view gbuffer_normal_tangent = "gbuffer_normal_tangent";
	constexpr std::string_view gbuffer_parameter = "gbuffer_parameter";
	constexpr std::string_view gbuffer_velocity = "gbuffer_velocity";
	constexpr std::string_view gbuffer_custom_data0 = "gbuffer_custom_data0";
	constexpr std::string_view gbuffer_custom_data1 = "gbuffer_custom_data1";
	constexpr std::string_view gbuffer_custom_data2 = "gbuffer_custom_data2";
	//	G-Buffer として読むシーンの深度バッファ
	constexpr std::string_view gbuffer_depth = "gbuffer_depth";
	constexpr system_utility::hash_key gbuffer_depth_hash_key = system_utility::generate_hash(gbuffer_depth);
	constexpr std::string_view gbuffer_names[] =
	{
		gbuffer_base_color,
		gbuffer_normal_tangent,
		gbuffer_parameter,
		gbuffer_velocity,
		gbuffer_custom_data0,

		gbuffer_custom_data1,
		gbuffer_custom_data2,
	};
	constexpr system_utility::hash_key gbuffer_hash_keys[] =
	{
		system_utility::generate_hash(gbuffer_base_color),
		system_utility::generate_hash(gbuffer_normal_tangent),
		system_utility::generate_hash(gbuffer_parameter),
		system_utility::generate_hash(gbuffer_velocity),
		system_utility::generate_hash(gbuffer_custom_data0),

		system_utility::generate_hash(gbuffer_custom_data1),
		system_utility::generate_hash(gbuffer_custom_data2),
	};
	//	G-Buffer識別子
	enum class gbuffer_id
	{
		base_color = 0,
		normal_tangent,
		parameters,
		velocity,
		custom_data0,
		custom_data1,
		custom_data2,

		start = base_color,
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	スクリーン空間反射用情報
	//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
	struct screen_space_reflection_constants
	{
		float ray_max_distance;				//  反射最大距離
		float ray_resolution;				//  レイのステップ解像度
		math::float2 ray_tickness;		//  深度判定を行う際の厚みの範囲

		int ray_substep_iteration_max;		//	ステップ深度チェック細分化回数
		int ray_flags;						//  処理の設定フラグ
		math::float2 dummy;
		math::float4 viewport_size;	//  シーンのビューポートサイズ
	};
	constexpr UINT screen_space_reflection_flags_reflection_dir_adjust = 0;	//	角度による厚み調整
	constexpr UINT screen_space_reflection_flags_consideration_uv_hole = 1;	//	UV穴埋め
	constexpr UINT screen_space_reflection_flags_substep_check = 2;	//	水平反射時の深度チェック
	constexpr UINT screen_space_reflection_flags_step_random = 3;	//	ランダムノイズを入れる

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	システム側で利用する定数バッファ名前リスト
	//	マテリアルやパイプラインステートで自由に設定されると困る系列
	constexpr std::string_view system_constant_buffer_names[] =
	{
		scene_constant_buffer_name,
		primitive_constant_buffer_name,
		shadow_map_constant_buffer_name,
		deferred_light_constant_buffer_name,
		deferred_light_probe_constant_buffer_name,
		clustered_lighting_constant_buffer_name,
		forward_light_constant_buffer_name,
		parameter_constant_buffer_name,

		decal_primitive_constant_buffer_name,
	};

	//	システム側で利用するシェーダーリソース名前リスト
	//	マテリアルやパイプラインステートで自由に設定されると困る系列
	constexpr std::string_view system_shader_resource_names[] =
	{
		morph_positions_name,
		morph_normals_name,
		primitive_headers_name,
		bone_matrices_name,
		meshlet_positions_name,
		meshlet_normals_name,
		meshlet_tangents_name,
		meshlet_colors0_name,
		meshlet_texcoords0_name,
		meshlet_texcoords1_name,
		meshlet_joints0_name,
		meshlet_joints1_name,
		meshlet_joints2_name,
		meshlet_joints3_name,
		meshlet_weights0_name,
		meshlet_weights1_name,
		meshlet_weights2_name,
		meshlet_weights3_name,
		meshlet_vertex_indices_name,
		meshlet_triangle_indices_name,
		visible_meshlet_instances_name,
		vertex_stream0_name,
		vertex_stream1_name,
		vertex_stream2_name,

		forward_only_diffuse_iem_name,
		forward_only_specular_pmrem_name,
		forward_only_lut_ggx_name,

		clustered_lights_name,
		cluster_counts_name,
		cluster_offsets_name,
		cluster_light_indices_name,

		shadow_maps_name,

		scene_color_map_name,
		scene_depth_map_name,
		gbuffer_depth,

		gbuffer_base_color,
		gbuffer_normal_tangent,
		gbuffer_parameter,
		gbuffer_velocity,
		gbuffer_custom_data0,
		gbuffer_custom_data1,
		gbuffer_custom_data2,
	};
}	//	namespace common_pipeline_states

