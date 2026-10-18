#include "Renderer/scene_render_features.h"

#include "model_instance.h"

#include "custom_pipeline_state.h"
#include "PipelineState/skybox_pipeline_state.h"
#include "PipelineState/fullscreen_quad_pipeline_state.h"
#include "PipelineState/debug_draw_line_pipeline_state.h"
#include "PipelineState/deferred_decal_pipeline_state.h"

#include "graphics/clustered_lighting_system.h"
#include "graphics/occlusion_culling_system.h"
#include "graphics/hiz_buffer.h"
#include "graphics/meshlet_culling_system.h"
#include "graphics/shadow_atlas.h"

#include <cmath>
#include <cstdint>
#include <cstring>

using namespace scene_rendering;

namespace
{
	void compute_bounding_sphere(const math::vector3f* corners, UINT size, math::vector3f& out_center, float& out_radius)
	{
		math::vector3f sum = math::vector3f::zero();
		for (UINT index = 0; index < size; ++index)
		{
			sum = sum + corners[index];
		}

		math::vector3f center = sum * (1.0f / 8.0f);

		float radius_sq = 0.0f;
		for (UINT index = 0; index < size; ++index)
		{
			float d_sq = (corners[index] - center).square_length();
			radius_sq = std::max(radius_sq, d_sq);
		}

		out_center = center;
		out_radius = sqrtf(radius_sq);
	}

	void compute_frustum_bounding_sphere(float ndc_near, float ndc_far,
										  const math::matrixf& inverse_view_projection_transform,
										  math::vector3f& out_center, float& out_radius)
	{
		const math::vector3f corners[8] =
		{
			math::vector3f::transform_coord(math::vector3f(-1.0f, -1.0f, ndc_near), inverse_view_projection_transform),
			math::vector3f::transform_coord(math::vector3f(+1.0f, -1.0f, ndc_near), inverse_view_projection_transform),
			math::vector3f::transform_coord(math::vector3f(+1.0f, +1.0f, ndc_near), inverse_view_projection_transform),
			math::vector3f::transform_coord(math::vector3f(-1.0f, +1.0f, ndc_near), inverse_view_projection_transform),
			math::vector3f::transform_coord(math::vector3f(-1.0f, -1.0f, ndc_far), inverse_view_projection_transform),
			math::vector3f::transform_coord(math::vector3f(+1.0f, -1.0f, ndc_far), inverse_view_projection_transform),
			math::vector3f::transform_coord(math::vector3f(+1.0f, +1.0f, ndc_far), inverse_view_projection_transform),
			math::vector3f::transform_coord(math::vector3f(-1.0f, +1.0f, ndc_far), inverse_view_projection_transform),
		};
		compute_bounding_sphere(corners, 8, out_center, out_radius);
	}

	//	シャドウキャッシュキー生成
	std::uint64_t compute_shadow_signature(const light_render_proxy& light, const view_render_data& render_data, bool& out_dynamic)
	{
		std::uint64_t hash = 1469598103934665603ull;
		auto mix = [&hash](std::uint64_t value) { hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2); };
		auto mix_quantized = [&mix](double value, double scale) { mix(static_cast<std::uint64_t>(std::llround(value * scale))); };
		auto mix_float = [&mix](float value)
		{
			std::uint32_t bits;
			std::memcpy(&bits, &value, sizeof(bits));
			mix(bits);
		};

		//	ライト
		const math::vector3d& origin = render_data.render_origin;
		mix(static_cast<std::uint64_t>(light.type));
		mix_quantized(origin.x + light.position.x, 100.0);
		mix_quantized(origin.y + light.position.y, 100.0);
		mix_quantized(origin.z + light.position.z, 100.0);
		mix_quantized(light.direction.x, 10000.0);
		mix_quantized(light.direction.y, 10000.0);
		mix_quantized(light.direction.z, 10000.0);
		mix_float(light.range);
		mix_float(light.outer_cone);
		for (const auto& bias : light.bias_data)
		{
			mix_float(bias.depth_bias);
			mix_float(bias.slope_depth_bias_scale);
			mix_float(bias.max_depth_bias);
			mix_float(bias.slope_depth_bias);
			mix_float(bias.max_slope_depth_bias);
			mix_float(bias.sigma_factor);
		}

		//	影響範囲内のメッシュ
		const auto& mesh_container = render_data.mesh_container;
		const float range_square = light.range * light.range;
		for (auto draw_index : mesh_container.shadow_items)
		{
			const auto& draw_item = mesh_container.draw_items[draw_index];

			const float dx = (std::max)((std::max)(draw_item.bounds_min.x - light.position.x, light.position.x - draw_item.bounds_max.x), 0.0f);
			const float dy = (std::max)((std::max)(draw_item.bounds_min.y - light.position.y, light.position.y - draw_item.bounds_max.y), 0.0f);
			const float dz = (std::max)((std::max)(draw_item.bounds_min.z - light.position.z, light.position.z - draw_item.bounds_max.z), 0.0f);
			if (dx * dx + dy * dy + dz * dz > range_square)
				continue;

			if (!system_utility::is_bitflag(draw_item.system_flags, mesh_draw_item::flags::static_mesh))
				out_dynamic = true;

			mix(draw_item.unique_id);
			const math::matrixf world(draw_item.world_transform);
			for (UINT row = 0; row < 3; ++row)
			{
				const math::vector3f axis = world.to_vector3(row);
				mix_quantized(axis.x, 10000.0);
				mix_quantized(axis.y, 10000.0);
				mix_quantized(axis.z, 10000.0);
			}
			const math::vector3f translation = world.to_vector3(3);
			mix_quantized(origin.x + translation.x, 100.0);
			mix_quantized(origin.y + translation.y, 100.0);
			mix_quantized(origin.z + translation.z, 100.0);
		}
		return hash;
	}

	//	@tool シーン深度の描画先
	frame_graph::depth_attachment scene_depth_attachment(const scene_frame& frame, bool clear_depth, bool clear_stencil)
	{
		frame_graph::depth_attachment depth;
		depth.texture = frame.textures.scene_depth;
		depth.format = common_pipeline_states::scene_depth_stencil_view_format;
		depth.clear_depth = clear_depth;
		depth.clear_stencil = clear_stencil;
		return depth;
	}

	//	@tool 視点のシャドウマップの描画先（配列全体を、深度・ステンシルともクリアしてから描く）
	frame_graph::depth_attachment shadow_map_attachment(const scene_frame& frame)
	{
		frame_graph::depth_attachment depth;
		depth.texture = frame.textures.shadow_map;
		depth.format = common_pipeline_states::shadow_map_depth_stencil_view_format;
		if (auto texture = std::dynamic_pointer_cast<texture2d>(frame.shadow_map))
			depth.array_size = texture->get_desc().ArraySize;
		depth.clear_depth = true;
		depth.clear_stencil = true;
		return depth;
	}

	//	@tool 視点のシャドウマップ全体のビューポート
	D3D11_VIEWPORT shadow_map_viewport(const scene_frame& frame)
	{
		D3D11_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = frame.shadow_map ? static_cast<float>(frame.shadow_map->get_width()) : 0.0f;
		viewport.Height = frame.shadow_map ? static_cast<float>(frame.shadow_map->get_height()) : 0.0f;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		return viewport;
	}

	//	@tool 点光源・スポットライトのシャドウマップアトラス（視点の状態に持つ）
	constexpr view_history_key shadow_atlas_key = system_utility::generate_hash("shadow_atlas");
	shadow_atlas& get_shadow_atlas(const scene_frame& frame)
	{
		return frame.context.current_view_state->get_feature_state<shadow_atlas>(shadow_atlas_key);
	}

	//	@tool G-Buffer をパス層へ設定する
	void bind_g_buffers(render_job_context& context, const frame_graph::resources& resources, const scene_renderer_commons::scene_texture_handles& textures)
	{
		for (UINT index = 0; index < static_cast<UINT>(common_pipeline_states::gbuffer_id::max); ++index)
		{
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[index], resources.get_texture(textures.g_buffers[index]));
		}
	}

	//	@tool ライティングのパス（G-Buffer を読み、シーンに加算する）の共通の準備
	struct lighting_pass_data
	{
		scene_renderer_commons::scene_texture_handles textures;
		std::shared_ptr<const material_render_proxy> lighting_material;
		D3D11_VIEWPORT viewport{};
	};
	void setup_lighting_pass(frame_graph::builder& builder, lighting_pass_data& data, const scene_frame& frame, frame_graph::texture_handle shadow_texture)
	{
		for (const auto& handle : frame.textures.g_buffers)
			builder.read(handle);
		if (shadow_texture.is_valid())
			builder.read(shadow_texture);

		frame_graph::color_attachment color;
		color.texture = frame.textures.scene_color;
		color.clear = false;
		color.clear_value = { 0.0f, 0.0f, 0.0f, 1.0f };
		builder.set_render_targets({ color });
		data.textures = frame.textures;
		data.viewport = frame.config.get_viewport();
	}

	//	@tool 影を描くパスのデータ
	struct shadow_pass_data
	{
		common_pipeline_states::shadow_map_constants shadow_map_constant{};
		scene_renderer_commons::shadow_draw_batch_list_ptr shadow_batches;
		D3D11_VIEWPORT viewport{};
	};

	//	@tool 影の描画先（アトラスのスライス範囲、または視点のシャドウマップ）
	struct shadow_target
	{
		texture_ptr atlas_texture;	//	@tool null は視点のシャドウマップ
		UINT first_slice = 0;
		UINT slice_count = 0;
		D3D11_VIEWPORT atlas_viewport{};
	};

	//	@tool アトラスのスライス範囲の描画先（スライスが割り当てられていない時と、アトラスが無い時は視点のシャドウマップ）
	shadow_target make_shadow_target(const shadow_atlas& atlas, int atlas_slice, UINT slice_count)
	{
		shadow_target target;
		if (atlas_slice >= 0 && atlas.get_texture())
		{
			target.atlas_texture = atlas.get_texture();
			target.first_slice = static_cast<UINT>(atlas_slice);
			target.slice_count = slice_count;
			target.atlas_viewport = atlas.get_viewport();
		}
		return target;
	}

	//	@tool 影を描くパス（点光源・スポットライト共通の実行処理）
	void add_shadow_pass(frame_graph& graph, const scene_frame& frame, std::string_view name, const shadow_pass_data& source, const shadow_target& target)
	{
		graph.add_pass<shadow_pass_data>(name, frame.context, [&](frame_graph::builder& builder, shadow_pass_data& data)
		{
			data = source;
			if (target.atlas_texture)
			{
				//	@tool アトラスは割り当てたスライスだけを深度クリアしてから描く（他のスライスはキャッシュとして残す）
				frame_graph::depth_attachment depth;
				depth.texture = graph.import_texture("shadow_map", target.atlas_texture);
				depth.format = common_pipeline_states::shadow_map_depth_stencil_view_format;
				depth.first_slice = target.first_slice;
				depth.array_size = target.slice_count;
				depth.clear_depth = true;
				depth.clear_stencil = false;
				builder.set_render_targets({}, depth);
				data.viewport = target.atlas_viewport;
			}
			else
			{
				builder.set_render_targets({}, shadow_map_attachment(frame));
				data.viewport = shadow_map_viewport(frame);
			}
			builder.use_immediate_context();
		},
		[](const shadow_pass_data& data, const frame_graph::resources&, render_job_context& context)
		{
			context.recorder.get_bind_table().set_constants(resource_view_bind_layer::pass, common_pipeline_states::shadow_map_constant_buffer_hash_key, data.shadow_map_constant);

			//	ビューポートの設定
			context.recorder.set_viewport(data.viewport);

			context.recorder.get_render_state_overrides().is_shadow_rendering = true;
			scene_renderer_commons::execute_shadow_draw_batches(context, *data.shadow_batches);
			context.recorder.get_render_state_overrides().is_shadow_rendering = false;
		});
	}

	//	@tool 点光源・スポットライトのライティングパス
	void add_light_pass(frame_graph& graph, const scene_frame& frame, const deferred_lighting_resources& resources, std::string_view name, const light_render_proxy& light, const common_pipeline_states::shadow_map_constants& shadow_map_constant, const texture_ptr& shadow_texture, material::system_paths path)
	{
		struct pass_data : lighting_pass_data
		{
			light_render_proxy light;
			common_pipeline_states::shadow_map_constants shadow_map_constant{};
			frame_graph::texture_handle shadow_map;
			material::system_paths path = material::system_paths::deferred_point;
		};
		graph.add_pass<pass_data>(name, frame.context, [&](frame_graph::builder& builder, pass_data& data)
		{
			data.shadow_map = graph.import_texture("shadow_map", shadow_texture);
			setup_lighting_pass(builder, data, frame, data.shadow_map);
			//	@tool 描画側はマテリアルの複製を参照する（ジョブ内で共有マテリアルに触れない）
			data.lighting_material = resources.deferred_lighting_material->get_render_proxy();
			data.light = light;
			data.shadow_map_constant = shadow_map_constant;
			data.path = path;
		},
		[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			context.recorder.get_bind_table().set_constants(resource_view_bind_layer::pass, common_pipeline_states::deferred_light_constant_buffer_hash_key, common_pipeline_states::deferred_light_constants{ data.light.data });
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::shadow_maps_hash_key, resources.get_texture(data.shadow_map));
			context.recorder.get_bind_table().set_constants(resource_view_bind_layer::pass, common_pipeline_states::shadow_map_constant_buffer_hash_key, data.shadow_map_constant);

			//	G-Bufferテクスチャ設定
			bind_g_buffers(context, resources, data.textures);

			//	ビューポートの設定
			context.recorder.set_viewport(data.viewport);
			{
				auto pipeline_state =
					data.lighting_material->get_pipeline_state(data.path);

				context.recorder.set_pipeline(*pipeline_state);
				context.recorder.bind_draw(*pipeline_state, *data.lighting_material, nullptr);
				scene_renderer_commons::draw(context, pipeline_state, 4);
				context.recorder.end_pipeline();
			}
		});
	}

	//	@tool 間接光
	void add_indirect_pass(frame_graph& graph, const scene_frame& frame, const deferred_lighting_resources& resources)
	{
		graph.add_pass<lighting_pass_data>("render_indirect_pass", frame.context, [&](frame_graph::builder& builder, lighting_pass_data& data)
		{
			setup_lighting_pass(builder, data, frame, {});
			//	@tool 描画側はマテリアルの複製を参照する（ジョブ内で共有マテリアルに触れない）
			data.lighting_material = resources.deferred_lighting_material->get_render_proxy();
		},
		[](const lighting_pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			//	G-Bufferテクスチャ設定
			bind_g_buffers(context, resources, data.textures);

			//	AOバッファ生成
			{
			}

			//	ビューポートの設定
			context.recorder.set_viewport(data.viewport);

			//	間接光
			for (const auto& light_probe : context.render_data->light_container.light_probes)
			{
				if (!light_probe.is_flag(light_probe::flags::active))
					continue;

				//	間接光の情報を設定
				common_pipeline_states::deferred_light_probe_constants deferred_light_probe_constant{};
				deferred_light_probe_constant.light_probe_data = light_probe.data;
				deferred_light_probe_constant.light_probe_attribute = light_probe.attribute;
				context.recorder.get_bind_table().set_constants(resource_view_bind_layer::pass, common_pipeline_states::deferred_light_probe_constant_buffer_hash_key, deferred_light_probe_constant);

				if (light_probe.is_flag(light_probe::flags::unbind))
				{
					//	全画面描画
					//	IBLテクスチャ設定
					//	@tool 共有マテリアルは書き換えず、context 経由でパイプラインに渡す
					context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::deferred_diffuse_iem_hash_key, light_probe.diffuse_ibl_texture);
					context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::deferred_specular_pmrem_hash_key, light_probe.specular_ibl_texture);
					context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::deferred_lut_ggx_hash_key, light_probe.lut_ggx_texture);

					auto pipeline_state =
						data.lighting_material->get_pipeline_state(material::system_paths::deferred_indirect);

					context.recorder.set_pipeline(*pipeline_state);
					context.recorder.bind_draw(*pipeline_state, *data.lighting_material, nullptr);
					scene_renderer_commons::draw(context, pipeline_state, 4);
					context.recorder.end_pipeline();
				}
				else
				{
					//	範囲制限をかける
				}
			}
		});
	}

	//	@tool 影無しの点光源/スポットライト（クラスタードライティング）
	void add_clustered_pass(frame_graph& graph, const scene_frame& frame)
	{
		if (!frame.context.current_view_state)
			return;

		const std::shared_ptr<clustered_lighting_system> clustered_lighting = frame.context.current_view_state->get_clustered_lighting();
		if (!clustered_lighting)
			return;

		//	対象ライトの収集とバッファ準備はメインスレッドで行う
		clustered_lighting->prepare(*frame.context.render_data, frame.context.scene_constant);

		struct pass_data : lighting_pass_data
		{
			std::shared_ptr<clustered_lighting_system> clustered_lighting;
		};

		//	クラスター構築
		//	@tool 描画先を持たないパスに分ける（結果はクラスタードライティングのバッファ（グラフの外）に残す）
		graph.add_pass<pass_data>("build_clustered_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
		{
			for (const auto& handle : frame.textures.g_buffers)
				builder.read(handle);
			builder.side_effect();
			data.textures = frame.textures;
			data.clustered_lighting = clustered_lighting;
		},
		[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			//	G-Bufferテクスチャ設定
			bind_g_buffers(context, resources, data.textures);

			data.clustered_lighting->build(context);
		});

		graph.add_pass<pass_data>("render_clustered_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
		{
			setup_lighting_pass(builder, data, frame, {});
			data.clustered_lighting = clustered_lighting;
		},
		[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			if (data.clustered_lighting->get_light_count() == 0 && !clustered_lighting_system::is_visualize(context.render_data->settings))
				return;

			//	G-Bufferテクスチャ設定
			bind_g_buffers(context, resources, data.textures);

			//	ビューポートの設定
			context.recorder.set_viewport(data.viewport);

			//	クラスター内のライトでライティング
			data.clustered_lighting->render(context);
		});
	}

	//	@tool 方向光源のシャドウマップ
	void add_directional_shadow_pass(frame_graph& graph, scene_frame& frame, const light_render_proxy& light, common_pipeline_states::shadow_map_constants& shadow_map_constant)
	{
		constexpr UINT directional_shadow_map_count = deferred_lighting_resources::directional_shadow_map_count;
		const renderer_context& context = frame.context;
		const D3D11_VIEWPORT shadow_map_pass_viewport = shadow_map_viewport(frame);

		math::float4 rotation = light.rotation.to_quaternion().to_float4();
		math::matrixf CameraTransformMatrix = math::matrixf(context.scene_constant.view_transform).inverted();
		math::matrixf LightTransformMatrix = math::matrixf::rotation_quaternion(math::quaternionf(rotation));
		math::matrixf InverseProjectionTransform;
		InverseProjectionTransform = math::matrixf(context.scene_constant.inverse_projection_transform);

		math::vector3f LightRight, LightUp, LightFront, LightPosition;
		LightRight = LightTransformMatrix.to_vector3(0);
		LightUp = LightTransformMatrix.to_vector3(1);
		LightFront = LightTransformMatrix.to_vector3(2);
		LightPosition = LightTransformMatrix.to_vector3(3);

		const math::vector3d& render_origin = context.render_data->render_origin;
		const double origin_x = math::vector3d::dot(render_origin, math::vector3d(LightRight));
		const double origin_y = math::vector3d::dot(render_origin, math::vector3d(LightUp));

		//	クリップ距離を算出
		float near_clip = context.scene_constant.camera_clip_distance.x;
		float camera_far_clip = context.scene_constant.camera_clip_distance.y;

		//	シーンのAABBを算出
		math::float3 scene_min = {FLT_MAX, FLT_MAX, FLT_MAX}, scene_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
		{
			auto draw_item_ptr = context.render_data->mesh_container.draw_items.data();
			for (auto& draw_index : context.render_data->mesh_container.shadow_items)
			{
				auto draw_item = draw_item_ptr + draw_index;
				scene_min = math::min(math::vector3f(draw_item->bounds_min), math::vector3f(scene_min)).to_float3();
				scene_max = math::max(math::vector3f(draw_item->bounds_max), math::vector3f(scene_max)).to_float3();
			}
		}
		const bool has_scene_aabb = (scene_min.x <= scene_max.x) && (scene_min.y <= scene_max.y) && (scene_min.z <= scene_max.z);

		//	ライト空間上でのシーンAABBのZ範囲を取得
		float scene_min_z = 0.0f, scene_max_z = 0.0f;
		if (has_scene_aabb)
		{
			math::vector3f Min(scene_min);
			math::vector3f Max(scene_max);
			math::vector3f Center = (Min + Max) * 0.5f;
			math::vector3f Extent = (Max - Min) * 0.5f;
			float center_proj = math::vector3f::dot(Center, LightFront);
			float extent_proj = math::vector3f::dot(Extent, math::abs(LightFront));
			scene_min_z = center_proj - extent_proj;
			scene_max_z = center_proj + extent_proj;
		}

		//	CSM用に遠クリップ面に対して距離制限をかける
		float far_clip = std::max(near_clip, std::min(light.csm_distance, camera_far_clip));
		float projection_near_clip = scene_renderer_commons::convert_view_depth_to_projection_depth(near_clip, near_clip, camera_far_clip);
		float projection_far_clip = scene_renderer_commons::convert_view_depth_to_projection_depth(far_clip, near_clip, camera_far_clip);

		//	シャドウマップ情報算出
		shadow_map_constant.shadow_map_count = directional_shadow_map_count;
		shadow_map_constant.shadow_attenuation = light.shadow_attenuation;
		shadow_map_constant.shadow_mask = 0;
		collision::frustum_culling frustum_culling;
		{
			math::matrixf InverseViewProjectionTransform;
			InverseViewProjectionTransform = math::matrixf(context.scene_constant.inverse_view_projection_transform);
			//	視錐台を囲むライト空間上の行列を生成
			math::matrixf LightViewMatrix, LightProjectionMatrix;
			float top_min_distance, top_max_distance;	//	最大深度の範囲
			{
				//	AABBを囲む球体を算出
				math::vector3f Center;
				float radius;
				if (has_scene_aabb)
				{
					math::vector3f scene_corners[8];
					scene_corners[0] = math::vector3f(scene_min.x, scene_min.y, scene_min.z);
					scene_corners[1] = math::vector3f(scene_max.x, scene_min.y, scene_min.z);
					scene_corners[2] = math::vector3f(scene_min.x, scene_max.y, scene_min.z);
					scene_corners[3] = math::vector3f(scene_max.x, scene_max.y, scene_min.z);
					scene_corners[4] = math::vector3f(scene_min.x, scene_min.y, scene_max.z);
					scene_corners[5] = math::vector3f(scene_max.x, scene_min.y, scene_max.z);
					scene_corners[6] = math::vector3f(scene_min.x, scene_max.y, scene_max.z);
					scene_corners[7] = math::vector3f(scene_max.x, scene_max.y, scene_max.z);
					compute_bounding_sphere(scene_corners, 8, Center, radius);
				}
				else
				{
					//	フラスタムから生成
					compute_frustum_bounding_sphere(projection_near_clip, projection_far_clip, InverseViewProjectionTransform, Center, radius);
				}

				//	視線行列算出
				{
					LightPosition = Center - (LightFront * radius);
					LightTransformMatrix = math::matrixf(math::vector4f(LightRight, 0.0f), math::vector4f(LightUp, 0.0f), math::vector4f(LightFront, 0.0f), math::vector4f(LightPosition, 1.0f));
					LightViewMatrix = LightTransformMatrix.inverted();
				}

				//	投影行列生成
				{
					top_min_distance = 0.0f;
					top_max_distance = top_min_distance + radius * 2.0f;
					LightProjectionMatrix = math::matrixf::orthographic_offset_center(-radius, +radius, -radius, +radius, top_min_distance, top_max_distance);
				}

				//	フラスタムカリング生成
				{
					math::float4x4 frustum_culling_light_view_projection_transform;
					frustum_culling_light_view_projection_transform = (LightViewMatrix * LightProjectionMatrix).to_float4x4();
					frustum_culling.create_planes(frustum_culling_light_view_projection_transform);
					frustum_culling.create_frustum_corners(math::matrixf(frustum_culling_light_view_projection_transform).inverted());
				}
			}

			//	PSSM
			std::vector<float> split_distance;
			split_distance.resize(directional_shadow_map_count + 1);
			{
				float split_weight = light.pssm_split_weight;
				float weight = math::lerp(0.1f, 0.9f, split_weight);
				float divN = far_clip / near_clip, subN = far_clip - near_clip;

				UINT index = 0;
				split_distance[index]	= near_clip;
				for (++index; index < directional_shadow_map_count; ++index)
				{
					float spliit = static_cast<float>(index) / static_cast<float>(directional_shadow_map_count);
					float fLog = near_clip * pow(divN, spliit);
					float fUni = near_clip + subN * spliit;
					split_distance[index]	= fLog * weight + fUni * ( 1.0f - weight);
				}
				split_distance[index]	= far_clip;
			}

			//	視錐台を距離で分割してライトビュープロジェクション行列を生成
			for (UINT index = 0; index < directional_shadow_map_count; ++index)
			{
				//	AABBを囲む球体を算出
				math::vector3f Center;
				float radius;
				compute_frustum_bounding_sphere(scene_renderer_commons::convert_view_depth_to_projection_depth(split_distance[index + 0], near_clip, camera_far_clip),
												scene_renderer_commons::convert_view_depth_to_projection_depth(split_distance[index + 1], near_clip, camera_far_clip),
												InverseViewProjectionTransform, Center, radius);
				radius *= 1.0001f;

				//	各行列調整
				{
					//	Stable CSM
					float texel_size_x = (radius * 2.0f) / shadow_map_pass_viewport.Width;
					float texel_size_y = (radius * 2.0f) / shadow_map_pass_viewport.Height;
					float center_x = math::vector3f::dot(Center, LightRight);
					float center_y = math::vector3f::dot(Center, LightUp);
					float center_z = math::vector3f::dot(Center, LightFront);

					//	絶対座標でスナップしてから描画原点からの相対値に戻す
					const double snapped_x = std::floor((origin_x + center_x) / texel_size_x) * texel_size_x;
					const double snapped_y = std::floor((origin_y + center_y) / texel_size_y) * texel_size_y;
					float X = static_cast<float>(snapped_x - origin_x);
					float Y = static_cast<float>(snapped_y - origin_y);

					//	near側はcasterを含めるためAABB最小値まで延長、far側はレシーバー範囲
					float receiver_near_z = center_z - radius;
					float receiver_far_z  = center_z + radius;
					float near_z = receiver_near_z;
					float far_z  = receiver_far_z;
					if (has_scene_aabb)
					{
						near_z = std::min(receiver_near_z, scene_min_z);
					}
					else
					{
						near_z -= (top_max_distance - top_min_distance);
					}

					math::vector3f Origin = ((LightRight * X) + (LightUp * Y)) + (LightFront * near_z);

					LightTransformMatrix = math::matrixf(math::vector4f(LightRight, 0.0f), math::vector4f(LightUp, 0.0f), math::vector4f(LightFront, 0.0f), math::vector4f(Origin, 1.0f));
					math::matrixf LightViewMatrix = LightTransformMatrix.inverted();

					//	投影行列生成
					float ortho_near = 0.0f;
					float ortho_far  = std::max(far_z - near_z, 0.0001f);
					const math::matrixf LightProjectionMatrix = math::matrixf::orthographic_offset_center(-radius, +radius, -radius, +radius, ortho_near, ortho_far);
					shadow_map_constant.light_view_projection_transform[index] = (LightViewMatrix * LightProjectionMatrix).to_float4x4();
				}

				{
					float split_near_clip = split_distance[index + 0];
					float split_far_clip = split_distance[index + 1];
					shadow_map_constant.shadow_split_distance[index].x = split_near_clip;
					shadow_map_constant.shadow_split_distance[index].y = split_near_clip;
					shadow_map_constant.shadow_split_distance[index].z = split_far_clip;
					shadow_map_constant.shadow_split_distance[index].w = split_far_clip;

					//	バイアス納入
					auto& bias_data = light.get_shadow_bias(index);
					float depth_bias, slope_depth_bias;
					depth_bias = bias_data.max_depth_bias / directional_shadow_map_count;
					depth_bias *= 0.5f;
					depth_bias *= bias_data.depth_bias;
					slope_depth_bias	= bias_data.slope_depth_bias_scale * bias_data.slope_depth_bias;
					shadow_map_constant.shadow_bias[index].x = std::max(0.0f, depth_bias);
					shadow_map_constant.shadow_bias[index].y = std::max(0.0f, depth_bias * slope_depth_bias);
					shadow_map_constant.shadow_bias[index].z = bias_data.max_slope_depth_bias;
					shadow_map_constant.shadow_bias[index].w = 1.0f / bias_data.sigma_factor; // 分散値
				}
			}
		}

		//	@tool 影の描画コマンドはパスを追加する時に作っておく（カリングとマテリアル毎のまとめもここで行う）
		struct pass_data
		{
			scene_renderer_commons::scene_texture_handles textures;
			scene_renderer_commons::shadow_draw_batch_list_ptr shadow_batches;
			common_pipeline_states::shadow_map_constants shadow_map_constant{};
			D3D11_VIEWPORT viewport{};
		};
		graph.add_pass<pass_data>("render_directional_shadow_map_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
		{
			builder.set_render_targets({}, shadow_map_attachment(frame));
			//	@tool G-Buffer も割り当てるので、読みとして宣言しておく
			for (const auto& handle : frame.textures.g_buffers)
				builder.read(handle);
			builder.use_immediate_context();
			data.textures = frame.textures;
			data.viewport = shadow_map_pass_viewport;
			data.shadow_batches = scene_renderer_commons::build_shadow_draw_batches(*context.render_data, context.render_data->mesh_container.shadow_items, frustum_culling, material::surface_paths::shadow_write_pass);
			data.shadow_map_constant = shadow_map_constant;
		},
		[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			//	定数バッファ情報設定
			context.recorder.get_bind_table().set_constants(resource_view_bind_layer::pass, common_pipeline_states::shadow_map_constant_buffer_hash_key, data.shadow_map_constant);

			//	G-Bufferテクスチャ設定
			bind_g_buffers(context, resources, data.textures);

			//	ビューポートの設定
			context.recorder.set_viewport(data.viewport);

			//	モデル類描画
			context.recorder.get_render_state_overrides().is_shadow_rendering = true;
			scene_renderer_commons::execute_shadow_draw_batches(context, *data.shadow_batches);
			//scene_renderer_commons::particle_render(context, context.render_data->effect_container.sprite_trail_shadow_datas, material::surface_paths::shadow_write_pass);
			context.recorder.get_render_state_overrides().is_shadow_rendering = false;
		});
	}

	//	@tool 方向光源のライティング
	void add_directional_pass(frame_graph& graph, const scene_frame& frame, const deferred_lighting_resources& resources, const light_render_proxy& light, const common_pipeline_states::shadow_map_constants& shadow_map_constant)
	{
		struct pass_data : lighting_pass_data
		{
			light_render_proxy light;
			common_pipeline_states::shadow_map_constants shadow_map_constant{};
		};
		graph.add_pass<pass_data>("render_directional_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
		{
			setup_lighting_pass(builder, data, frame, frame.textures.shadow_map);
			//	@tool 描画側はマテリアルの複製を参照する（ジョブ内で共有マテリアルに触れない）
			data.lighting_material = resources.deferred_lighting_material->get_render_proxy();
			data.light = light;
			data.shadow_map_constant = shadow_map_constant;
		},
		[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
		{
			//	コンテキストに情報登録
			context.recorder.get_bind_table().set_constants(resource_view_bind_layer::pass, common_pipeline_states::deferred_light_constant_buffer_hash_key, common_pipeline_states::deferred_light_constants{ data.light.data });
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::shadow_maps_hash_key, resources.get_texture(data.textures.shadow_map));
			context.recorder.get_bind_table().set_constants(resource_view_bind_layer::pass, common_pipeline_states::shadow_map_constant_buffer_hash_key, data.shadow_map_constant);

			//	G-Bufferテクスチャ設定
			bind_g_buffers(context, resources, data.textures);

			//	ビューポートの設定
			context.recorder.set_viewport(data.viewport);
			{
				auto pipeline_state =
					data.lighting_material->get_pipeline_state(material::system_paths::deferred_directional);

				context.recorder.set_pipeline(*pipeline_state);
				context.recorder.bind_draw(*pipeline_state, *data.lighting_material, nullptr);
				scene_renderer_commons::draw(context, pipeline_state, 4);
				context.recorder.end_pipeline();
			}
		});
	}

	//	@tool 点光源のシャドウマップ（定数を求め、描き直す場合はパスを追加する）
	void add_point_shadow_pass(frame_graph& graph, scene_frame& frame, const light_render_proxy& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, int atlas_slice, bool render_shadow)
	{
		math::float3 position = light.position;
		math::vector3f position_vector(position);
		float range = light.range;

		//	キューブマップ6面分の視線方向・上方向(0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z)
		static const math::float3 cube_face_directions[6] =
		{
			{ +1.0f,  0.0f,  0.0f },
			{ -1.0f,  0.0f,  0.0f },
			{  0.0f, +1.0f,  0.0f },
			{  0.0f, -1.0f,  0.0f },
			{  0.0f,  0.0f, +1.0f },
			{  0.0f,  0.0f, -1.0f },
		};
		static const math::float3 cube_face_ups[6] =
		{
			{ 0.0f, 1.0f,  0.0f },
			{ 0.0f, 1.0f,  0.0f },
			{ 0.0f, 0.0f, -1.0f },
			{ 0.0f, 0.0f, +1.0f },
			{ 0.0f, 1.0f,  0.0f },
			{ 0.0f, 1.0f,  0.0f },
		};

		const math::matrixf ProjectionMatrix = math::matrixf::perspective_fov(math::constants<float>::pi_half, 1.0f, 1.0f, range);

		//	定数バッファ設定
		shadow_map_constant.shadow_map_count = 6;
		shadow_map_constant.shadow_attenuation = light.shadow_attenuation;
		shadow_map_constant.shadow_mask = 0;
		shadow_map_constant.shadow_slice_offset = (atlas_slice >= 0) ? static_cast<UINT>(atlas_slice) : 0;

		for (UINT face = 0; face < 6; ++face)
		{
			math::vector3f direction_vector(cube_face_directions[face]);
			math::vector3f up_vector(cube_face_ups[face]);
			const math::matrixf ViewMatrix = math::matrixf::look_at(position_vector, position_vector + direction_vector, up_vector);
			shadow_map_constant.light_view_projection_transform[face] = (ViewMatrix * ProjectionMatrix).to_float4x4();

			//	面選択はワールド方向で行うため、カスケード用の深度バンドは常に真になる値にしておく
			shadow_map_constant.shadow_split_distance[face].x = -1.0f;
			shadow_map_constant.shadow_split_distance[face].y = -1.0f;
			shadow_map_constant.shadow_split_distance[face].z = FLT_MAX;
			shadow_map_constant.shadow_split_distance[face].w = FLT_MAX;

			//	バイアス処理
			auto& bias_data = light.get_shadow_bias(face);
			float depth_bias, slope_depth_bias;
			depth_bias = bias_data.max_depth_bias / math::max(0.01f, range);
			depth_bias *= 0.5f;
			depth_bias *= bias_data.depth_bias;
			slope_depth_bias = bias_data.slope_depth_bias_scale * bias_data.slope_depth_bias;
			shadow_map_constant.shadow_bias[face].x = std::max(0.0f, depth_bias);
			shadow_map_constant.shadow_bias[face].y = std::max(0.0f, depth_bias * slope_depth_bias);
			shadow_map_constant.shadow_bias[face].z = bias_data.max_slope_depth_bias;
			shadow_map_constant.shadow_bias[face].w = bias_data.sigma_factor; // 分散値
		}

		//	影のキャッシュが使える場合は描き直さない
		if (!render_shadow)
			return;

		//	シャドウマップアトラスに描く場合は割り当てられたスライス範囲に書き込む
		shadow_atlas& atlas = get_shadow_atlas(frame);
		shadow_pass_data source;
		source.shadow_map_constant = shadow_map_constant;
		//	@tool 影の描画コマンドはパスを追加する時に作っておく（カリングとマテリアル毎のまとめもここで行う）
		source.shadow_batches = scene_renderer_commons::build_shadow_draw_batches(*frame.context.render_data, frame.context.render_data->mesh_container.shadow_items, position, range, material::surface_paths::shadow_write_pass);
		add_shadow_pass(graph, frame, "render_point_shadow_map_pass", source, make_shadow_target(atlas, atlas_slice, 6));
	}

	//	@tool スポットライトのシャドウマップ（定数を求め、描き直す場合はパスを追加する）
	void add_spot_shadow_pass(frame_graph& graph, scene_frame& frame, const light_render_proxy& light, common_pipeline_states::shadow_map_constants& shadow_map_constant, int atlas_slice, bool render_shadow)
	{
		constexpr UINT directional_shadow_map_count = deferred_lighting_resources::directional_shadow_map_count;

		math::float4 rotation = light.rotation.to_quaternion().to_float4();
		math::float3 position = light.position;
		math::matrixf TransformMatrix, ViewMatrix, ProjectionMatrix;
		TransformMatrix = math::matrixf::rotation_quaternion(math::quaternionf(rotation))
						* math::matrixf::translation(position.x, position.y, position.z);
		ViewMatrix = TransformMatrix.inverted();
		ProjectionMatrix = math::matrixf::perspective_fov(light.outer_cone * 2.0f, 1.0f, 1.0f, light.range);

		//	定数バッファ情報設定
		shadow_map_constant.shadow_map_count = 1;
		shadow_map_constant.shadow_attenuation = light.shadow_attenuation;
		shadow_map_constant.shadow_split_distance[0].x = -1.0f;
		shadow_map_constant.shadow_split_distance[0].y = -1.0f;
		shadow_map_constant.shadow_split_distance[0].z = FLT_MAX;
		shadow_map_constant.shadow_split_distance[0].w = FLT_MAX;

		//	バイアス納入
		{
			//	ライトの範囲で処理
			float near_clip = 0.1f;
			float far_clip = near_clip + light.range;

			auto& bias_data = light.get_shadow_bias(0);
			float depth_bias, slope_depth_bias;
			depth_bias = bias_data.max_depth_bias / directional_shadow_map_count;
			depth_bias *= 0.5f;
			depth_bias *= bias_data.depth_bias;
			slope_depth_bias	= bias_data.slope_depth_bias_scale * bias_data.slope_depth_bias;
			shadow_map_constant.shadow_bias[0].x = std::max(0.0f, depth_bias);
			shadow_map_constant.shadow_bias[0].y = std::max(0.0f, depth_bias * slope_depth_bias);
			shadow_map_constant.shadow_bias[0].z = bias_data.max_slope_depth_bias;
			shadow_map_constant.shadow_bias[0].w = bias_data.sigma_factor; // 分散値
		}

		shadow_map_constant.shadow_mask = 0;
		//	シャドウマップアトラスの先頭スライス
		shadow_map_constant.shadow_slice_offset = (atlas_slice >= 0) ? static_cast<UINT>(atlas_slice) : 0;
		shadow_map_constant.light_view_projection_transform[0] = (ViewMatrix * ProjectionMatrix).to_float4x4();

		//	モデル類描画
		collision::frustum_culling frustum_culling;
		{
			frustum_culling.create_planes(shadow_map_constant.light_view_projection_transform[0]);
			//	シャドウマップアトラスではカメラに依存しないよう、ライト自身の視錐台の頂点を使う
			if (atlas_slice >= 0)
				frustum_culling.create_frustum_corners(math::matrixf(shadow_map_constant.light_view_projection_transform[0]).inverted());
			else
				frustum_culling.create_frustum_corners(math::matrixf(frame.context.scene_constant.view_projection_transform).inverted());
		}

		//	影のキャッシュが使える場合は描き直さない
		if (!render_shadow)
			return;

		//	シャドウマップアトラスに描く場合は割り当てられたスライス範囲に書き込む
		shadow_atlas& atlas = get_shadow_atlas(frame);
		shadow_pass_data source;
		source.shadow_map_constant = shadow_map_constant;
		//	@tool 影の描画コマンドはパスを追加する時に作っておく（カリングとマテリアル毎のまとめもここで行う）
		source.shadow_batches = scene_renderer_commons::build_shadow_draw_batches(*frame.context.render_data, frame.context.render_data->mesh_container.shadow_items, frustum_culling, material::surface_paths::shadow_write_pass);
		add_shadow_pass(graph, frame, "render_spot_shadow_map_pass", source, make_shadow_target(atlas, atlas_slice, 1));
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	深度プリパス
void scene_rendering::add_depth_prepass(frame_graph& graph, const scene_frame& frame)
{
	struct pass_data
	{
		renderer_context context;
		scene_renderer_commons::mesh_draw_command_list_ptr opaque_commands;
		scene_renderer_commons::mesh_draw_command_list_ptr mask_commands;
		scene_renderer_commons::mesh_draw_command_list_ptr particle_opaque_commands;
		scene_renderer_commons::mesh_draw_command_list_ptr particle_mask_commands;
		D3D11_VIEWPORT viewport{};
	};
	graph.add_pass<pass_data>("depth_pre_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		builder.set_render_targets({}, scene_depth_attachment(frame, true, true));
		data.viewport = frame.config.get_viewport();

		//	描画コマンドはパスを追加する時に作っておく
		const renderer_context& renderer_context = frame.context;
		const view_render_data& frame_data = *renderer_context.render_data;
		data.context = renderer_context;
		data.opaque_commands = scene_renderer_commons::build_model_indirect_draw_commands(frame_data, renderer_context.render_mesh_indices->opaque, renderer_context.render_mesh_indices->lengths, material::surface_paths::depth_write_pass);
		data.mask_commands = scene_renderer_commons::build_model_indirect_draw_commands(frame_data, renderer_context.render_mesh_indices->mask, renderer_context.render_mesh_indices->lengths, material::surface_paths::depth_write_pass);
		data.particle_opaque_commands = scene_renderer_commons::build_particle_draw_commands(frame_data, renderer_context.render_particle_indices->opaque, material::surface_paths::depth_write_pass);
		data.particle_mask_commands = scene_renderer_commons::build_particle_draw_commands(frame_data, renderer_context.render_particle_indices->mask, material::surface_paths::depth_write_pass);
	},
	[](const pass_data& data, const frame_graph::resources&, render_job_context& context)
	{
		//	ビューポートの設定
		context.recorder.set_viewport(data.viewport);

		//	モデル類描画
		scene_renderer_commons::execute_model_indirect_draw_commands(context, *data.opaque_commands, *data.context.current_view_state->get_meshlet_culling());
		scene_renderer_commons::execute_model_indirect_draw_commands(context, *data.mask_commands, *data.context.current_view_state->get_meshlet_culling());
		scene_renderer_commons::execute_particle_draw_commands(context, *data.particle_opaque_commands);
		scene_renderer_commons::execute_particle_draw_commands(context, *data.particle_mask_commands);
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	オクルージョンカリング
void scene_rendering::add_occlusion_culling_pass(frame_graph& graph, const scene_frame& frame)
{
	const renderer_context& context = frame.context;
	if (!context.current_view_state)
		return;

	const auto& state = *context.current_view_state;
	auto occlusion = state.get_occlusion_culling();
	auto hiz = state.get_hiz_buffer();
	auto meshlet_culling = state.get_meshlet_culling();
	if (!occlusion || !hiz || !meshlet_culling)
		return;

	hiz->resize_buffer(static_cast<int>(frame.config.width), static_cast<int>(frame.config.height));

	struct pass_data
	{
		frame_graph::texture_handle scene_depth;
		std::shared_ptr<occlusion_culling_system> occlusion;
		std::shared_ptr<hiz_buffer> hiz;
		std::shared_ptr<meshlet_culling_system> meshlet_culling;
		std::vector<std::size_t> visible_draw_indices;
		std::shared_ptr<render_mesh_indices> render_mesh_indices;
		bool freeze_culling = false;
	};
	graph.add_pass<pass_data>("occlusion_culling_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		//	@tool 深度を読み、結果は視点の状態（GPU 側）に残す
		data.scene_depth = builder.read(frame.textures.scene_depth);
		builder.side_effect();
		builder.use_immediate_context();

		data.occlusion = occlusion;
		data.hiz = hiz;
		data.meshlet_culling = meshlet_culling;
#if defined(USE_IMGUI)
		data.freeze_culling = state.is_freeze_culling();
#endif	//	defined(USE_IMGUI)

		//	メッシュレットカリング情報
		data.render_mesh_indices = context.render_mesh_indices;
		if (context.render_mesh_indices)
		{
			data.visible_draw_indices.reserve(context.render_mesh_indices->opaque.size() + context.render_mesh_indices->mask.size());
			data.visible_draw_indices.insert(data.visible_draw_indices.end(), context.render_mesh_indices->opaque.begin(), context.render_mesh_indices->opaque.end());
			data.visible_draw_indices.insert(data.visible_draw_indices.end(), context.render_mesh_indices->mask.begin(), context.render_mesh_indices->mask.end());
		}
	},
	[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		context.recorder.begin_event(L"occlusion_culling");

		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_depth_map_hash_key, resources.get_texture(data.scene_depth));

		//	階層Zバッファ構築
		data.hiz->build(context);

		//	オクルージョンカリング実行
		data.occlusion->update(context, *data.hiz);

		//	メッシュレットカリング実行
		if (!data.freeze_culling && data.render_mesh_indices)
		{
			data.meshlet_culling->build_and_cull(context, data.visible_draw_indices, data.render_mesh_indices->lengths, *data.hiz);
		}

		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::scene_depth_map_hash_key, nullptr);

		context.recorder.end_event();
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	G-Buffer
void scene_rendering::add_gbuffer_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const gbuffer_resources>& resources)
{
	struct pass_data
	{
		std::shared_ptr<const gbuffer_resources> resources;
		renderer_context context;
		scene_renderer_commons::mesh_draw_command_list_ptr opaque_commands;
		scene_renderer_commons::mesh_draw_command_list_ptr mask_commands;
		scene_renderer_commons::mesh_draw_command_list_ptr particle_opaque_commands;
		scene_renderer_commons::mesh_draw_command_list_ptr particle_mask_commands;
		D3D11_VIEWPORT viewport{};
	};
	graph.add_pass<pass_data>("gbuffer_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		//	GBufferパス生成
		FLOAT clear_color[][4] =
		{
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	base_color
			{ 0.0f, 0.0f, 0.0f, common_pipeline_states::gbuffer_max_linear_depth },	//	normal_depth
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	parameter
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	velocity
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	unique_data
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	shade_color [エンジン改造:G-Buffer]
			{ 0.0f, 0.0f, 0.0f, 0.0f },		//	ilm_color [エンジン改造:G-Buffer]
		};
		std::vector<frame_graph::color_attachment> colors;
		for (int i = 0; i < static_cast<int>(common_pipeline_states::gbuffer_id::max); ++i)
		{
			auto& color = colors.emplace_back();
			color.texture = frame.textures.g_buffers[i];
			color.clear = true;
			color.clear_value = { clear_color[i][0], clear_color[i][1], clear_color[i][2], clear_color[i][3] };
		}
		{
			//	emissive用にシーンバッファ追加
			auto& color = colors.emplace_back();
			color.texture = frame.textures.scene_color;
			color.clear = true;
		}
		builder.set_render_targets(std::move(colors), scene_depth_attachment(frame, false, true));
		data.viewport = frame.config.get_viewport();
		data.resources = resources;

		//	@tool 描画コマンドはパスを追加する時に作っておく（描画ジョブは実行するだけ）
		const renderer_context& renderer_context = frame.context;
		const view_render_data& frame_data = *renderer_context.render_data;
		data.context = renderer_context;
		data.opaque_commands = scene_renderer_commons::build_model_indirect_draw_commands(frame_data, renderer_context.render_mesh_indices->opaque, renderer_context.render_mesh_indices->lengths, material::surface_paths::g_buffer_pass);
		data.mask_commands = scene_renderer_commons::build_model_indirect_draw_commands(frame_data, renderer_context.render_mesh_indices->mask, renderer_context.render_mesh_indices->lengths, material::surface_paths::g_buffer_pass);
		data.particle_opaque_commands = scene_renderer_commons::build_particle_draw_commands(frame_data, renderer_context.render_particle_indices->opaque, material::surface_paths::g_buffer_pass);
		data.particle_mask_commands = scene_renderer_commons::build_particle_draw_commands(frame_data, renderer_context.render_particle_indices->mask, material::surface_paths::g_buffer_pass);
	},
	[](const pass_data& data, const frame_graph::resources&, render_job_context& context)
	{
		//	ビューポートの設定
		context.recorder.set_viewport(data.viewport);

		//	モデル類描画
		scene_renderer_commons::execute_model_indirect_draw_commands(context, *data.opaque_commands, *data.context.current_view_state->get_meshlet_culling());
		scene_renderer_commons::execute_model_indirect_draw_commands(context, *data.mask_commands, *data.context.current_view_state->get_meshlet_culling());
		scene_renderer_commons::execute_particle_draw_commands(context, *data.particle_opaque_commands);
		scene_renderer_commons::execute_particle_draw_commands(context, *data.particle_mask_commands);

		//	スカイライト処理
		for (const auto& light_probe : context.render_data->light_container.light_probes)
		{
			if (!light_probe.is_flag(light_probe::flags::active) || !light_probe.is_flag(light_probe::flags::unbind))
			{
				continue;
			}

			//	IBLテクスチャ設定
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::deferred_diffuse_iem_hash_key, light_probe.diffuse_ibl_texture);
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::deferred_specular_pmrem_hash_key, light_probe.specular_ibl_texture);
			context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::deferred_lut_ggx_hash_key, light_probe.lut_ggx_texture);

			//	間接光の情報を設定
			common_pipeline_states::deferred_light_probe_constants deferred_light_probe_constant{};
			deferred_light_probe_constant.light_probe_data = light_probe.data;
			deferred_light_probe_constant.light_probe_attribute = light_probe.attribute;
			context.recorder.get_bind_table().set_constants(resource_view_bind_layer::pass, common_pipeline_states::deferred_light_probe_constant_buffer_hash_key, deferred_light_probe_constant);

			//	全画面描画
			context.recorder.set_pipeline(*data.resources->skybox_pipeline_states);
			scene_renderer_commons::draw(context, data.resources->skybox_pipeline_states, 4);
			context.recorder.end_pipeline();
			break;
		}
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	G-Bufferデカール
void scene_rendering::add_gbuffer_decal_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const decal_resources>& resources)
{
	if (frame.context.render_decal_indices->draw_list.empty())
		return;

	struct pass_data
	{
		std::shared_ptr<const decal_resources> resources;
		std::shared_ptr<render_decal_indices> render_decal_indices;
		frame_graph::texture_handle normal_depth;
		frame_graph::texture_handle unique_data;
		frame_graph::texture_handle normal_depth_copy;
		D3D11_VIEWPORT viewport{};
	};

	//	@tool 法線/深度の複製先は一時テクスチャとして要求する（G-Buffer の法線/深度と同じ形式）
	const frame_graph::texture_handle normal_depth_copy = graph.create_texture("decal_normal_depth_copy", scene_renderer_commons::make_work_texture_desc(frame.config, DXGI_FORMAT_R32G32B32A32_FLOAT));

	//	法線/深度コピー
	//	@tool 描画先を持たないパスに分ける（デカールのパスでは法線/深度が描画先になるため）
	graph.add_pass<pass_data>("gbuffer_decal_copy_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		data.normal_depth = builder.read(frame.textures.g_buffer(common_pipeline_states::gbuffer_id::normal_depth), frame_graph::texture_usage::copy_source);
		data.normal_depth_copy = builder.write(normal_depth_copy, frame_graph::texture_usage::copy_destination);
	},
	[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> destination, source;
		resources.get_texture(data.normal_depth_copy)->get_shader_resource_view()->GetResource(destination.ReleaseAndGetAddressOf());
		resources.get_texture(data.normal_depth)->get_shader_resource_view()->GetResource(source.ReleaseAndGetAddressOf());
		context.recorder.copy_resource(destination.Get(), source.Get());
	});

	graph.add_pass<pass_data>("gbuffer_decal_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		data.normal_depth_copy = builder.read(normal_depth_copy);
		data.unique_data = builder.read(frame.textures.g_buffer(common_pipeline_states::gbuffer_id::unique_data));
		data.normal_depth = frame.textures.g_buffer(common_pipeline_states::gbuffer_id::normal_depth);

		//	GBufferパス生成
		frame_graph::color_attachment base_color;
		base_color.texture = frame.textures.g_buffer(common_pipeline_states::gbuffer_id::base_color);
		frame_graph::color_attachment normal_depth;
		normal_depth.texture = frame.textures.g_buffer(common_pipeline_states::gbuffer_id::normal_depth);
		builder.set_render_targets({ base_color, normal_depth }, scene_depth_attachment(frame, false, true));
		data.viewport = frame.config.get_viewport();
		data.resources = resources;
		data.render_decal_indices = frame.context.render_decal_indices;
	},
	[](const pass_data& data, const frame_graph::resources& resources, render_job_context& context)
	{
		//	@tool 一時テクスチャの実体は記録時に取得する
		const texture_ptr normal_depth_copy_texture = resources.get_texture(data.normal_depth_copy);
		const texture_ptr normal_depth_texture = resources.get_texture(data.normal_depth);
		const texture_ptr unique_data_texture = resources.get_texture(data.unique_data);

		//	ビューポートの設定
		context.recorder.set_viewport(data.viewport);

		//	デカール
		{
			//	カメラ球体
			collision::bounding_sphere sphere;
			{
				math::float3 center;
				center.x = context.scene_constant.camera_position.x;
				center.y = context.scene_constant.camera_position.y;
				center.z = context.scene_constant.camera_position.z;
				sphere = collision::bounding_sphere(math::double3(center), context.scene_constant.camera_clip_distance.x * 1.01f);
			}

			//	必要なテクスチャを設定
			{
				context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[static_cast<UINT>(common_pipeline_states::gbuffer_id::normal_depth)], normal_depth_copy_texture);
				context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[static_cast<UINT>(common_pipeline_states::gbuffer_id::unique_data)], unique_data_texture);
			}

			//	デカール描画
			for (auto& decal_index : data.render_decal_indices->draw_list)
			{
				auto& decal = context.render_data->decal_container.draw_items[decal_index];

				//	バッファ情報生成
				mesh_draw_item draw_item;
				{
					const math::matrixf W = math::matrixf::scaling(decal.scale.x, decal.scale.y, decal.scale.z)
										* math::matrixf::rotation_quaternion(decal.rotation.to_quaternion())
										* math::matrixf::translation(decal.position.x, decal.position.y, decal.position.z);
					//	プリミティブバッファ更新
					{
						common_pipeline_states::decal_primitive_constants dpc;
						dpc.decal_transform = W.to_float4x4();
						auto front = decal.rotation.up().to_float3();
						dpc.decal_direction.x = front.x;
						dpc.decal_direction.y = front.y;
						dpc.decal_direction.z = front.z;
						dpc.decal_direction.w = 0;
						dpc.decal_inverse_transform = W.inverted().to_float4x4();
						context.recorder.update_subresource(data.resources->decal_primitive_primitive_buffer.Get(), 0, 0, &dpc, 0, 0);
					}
					draw_item.primitive_buffer = data.resources->decal_primitive_primitive_buffer;
				}

				//	ステンシル書き込み
				{
					context.recorder.set_pipeline(*data.resources->stencil_write_pipeline_state);
					context.recorder.bind_draw(*data.resources->stencil_write_pipeline_state, *decal.material, &draw_item);
					scene_renderer_commons::draw_geometric_primitive(context, *data.resources->cube);
					context.recorder.end_pipeline();
				}

				//	デカール書き込み
				auto pass = sphere.intersection(&decal.box) ? material::decal_paths::fullscreen : material::decal_paths::primitive;
				if (auto pipeline_state = decal.material->get_pipeline_state(pass))
				{
					context.recorder.set_pipeline(*pipeline_state);
					if (pass == material::decal_paths::fullscreen)
					{
						context.recorder.get_render_state_overrides().force_two_sided = true;
						context.recorder.bind_draw(*pipeline_state, *decal.material, &draw_item);
						scene_renderer_commons::draw(context, pipeline_state, 4);
						context.recorder.get_render_state_overrides().force_two_sided = false;
					}
					else
					{
						context.recorder.bind_draw(*pipeline_state, *decal.material, &draw_item);
						scene_renderer_commons::draw_geometric_primitive(context, *data.resources->cube);
					}
					context.recorder.end_pipeline();
				}
			}
		}

		context.recorder.get_bind_table().set_texture(resource_view_bind_layer::pass, common_pipeline_states::gbuffer_hash_keys[static_cast<UINT>(common_pipeline_states::gbuffer_id::normal_depth)], normal_depth_texture);
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	ディファードライティング
void scene_rendering::add_deferred_lighting_passes(frame_graph& graph, scene_frame& frame, const std::shared_ptr<const deferred_lighting_resources>& resources)
{
	const renderer_context& context = frame.context;
	//	@tool シャドウマップアトラスは視点の状態に持つ（視点の状態は framework が必ず用意する）
	if (!context.current_view_state)
		return;

	//	間接光
	add_indirect_pass(graph, frame, *resources);

	//	影を落とす点光源/スポットライトにシャドウマップアトラスを割り当てる
	struct atlas_slot
	{
		UINT first_slice = 0;
		bool needs_render = true;
	};
	std::unordered_map<const light_render_proxy*, atlas_slot> atlas_slots;
	//	@tool 設定は抽出時点の値を使う
	const render_settings& settings = context.render_data->settings;
	shadow_atlas& atlas = get_shadow_atlas(frame);
	atlas.begin_frame(settings.shadow_atlas);
	if (settings.shadow_atlas.enable)
	{
		struct atlas_candidate
		{
			const light_render_proxy* light;
			float priority;
		};
		std::vector<atlas_candidate> candidates;
		const math::vector3f camera(context.scene_constant.camera_position.x, context.scene_constant.camera_position.y, context.scene_constant.camera_position.z);
		for (const auto& light : context.render_data->light_container.lights)
		{
			if (!light.is_flag(light::flags::active) || !light.is_flag(light::flags::use_shadow) || clustered_lighting_system::is_target(light, settings))
				continue;
			if (light.type != light_type::point && light.type != light_type::spot)
				continue;
			const float distance = (math::vector3f(light.position) - camera).length();
			candidates.push_back({ &light, light.range / (std::max)(distance, light.range * 0.1f) });
		}
		std::stable_sort(candidates.begin(), candidates.end(), [](const atlas_candidate& l, const atlas_candidate& r) { return l.priority > r.priority; });

		const UINT max_lights = settings.shadow_atlas.max_lights;
		for (const auto& candidate : candidates)
		{
			if (atlas.get_allocated_lights() >= max_lights)
				break;

			bool dynamic = false;
			const std::uint64_t signature = compute_shadow_signature(*candidate.light, *context.render_data, dynamic);
			atlas_slot slot;
			if (atlas.acquire(candidate.light->id, candidate.light->type == light_type::point ? 6u : 1u, signature, dynamic, slot.first_slice, slot.needs_render))
				atlas_slots[candidate.light] = slot;
		}
	}
	atlas.end_frame();

	//	ライティング処理
	for (const auto& light : context.render_data->light_container.lights)
	{
		if (!light.is_flag(light::flags::active))
			continue;

		//	クラスタードライティング対象は後でまとめて描画する
		if (clustered_lighting_system::is_target(light, settings))
			continue;

		switch (light.type)
		{
			case light_type::directional:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant{};
				if (light.is_flag(light::flags::use_shadow))
				{
					add_directional_shadow_pass(graph, frame, light, shadow_map_constant);
				}

				//	ライティング処理
				add_directional_pass(graph, frame, *resources, light, shadow_map_constant);
				break;
			}
			case light_type::point:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant{};
				const auto atlas_it = atlas_slots.find(&light);
				const int atlas_slice = (atlas_it != atlas_slots.end()) ? static_cast<int>(atlas_it->second.first_slice) : -1;
				const bool render_shadow = (atlas_it == atlas_slots.end()) || atlas_it->second.needs_render;
				if (light.is_flag(light::flags::use_shadow))
				{
					add_point_shadow_pass(graph, frame, light, shadow_map_constant, atlas_slice, render_shadow);
				}

				//	ライティング処理
				//	シャドウマップアトラスに描いた場合はアトラスを参照する
				const texture_ptr shadow_texture = (atlas_slice >= 0) ? atlas.get_texture() : frame.shadow_map;
				add_light_pass(graph, frame, *resources, "render_point_pass", light, shadow_map_constant, shadow_texture, material::system_paths::deferred_point);
				break;
			}
			case light_type::spot:
			{
				//	シャドウマップ生成
				common_pipeline_states::shadow_map_constants shadow_map_constant{};
				const auto atlas_it = atlas_slots.find(&light);
				const int atlas_slice = (atlas_it != atlas_slots.end()) ? static_cast<int>(atlas_it->second.first_slice) : -1;
				const bool render_shadow = (atlas_it == atlas_slots.end()) || atlas_it->second.needs_render;
				if (light.is_flag(light::flags::use_shadow))
				{
					add_spot_shadow_pass(graph, frame, light, shadow_map_constant, atlas_slice, render_shadow);
				}

				//	ライティング処理
				//	シャドウマップアトラスに描いた場合はアトラスを参照する
				const texture_ptr shadow_texture = (atlas_slice >= 0) ? atlas.get_texture() : frame.shadow_map;
				add_light_pass(graph, frame, *resources, "render_spot_pass", light, shadow_map_constant, shadow_texture, material::system_paths::deferred_spot);
				break;
			}
		}
	}

	//	影無しの点光源/スポットライト
	add_clustered_pass(graph, frame);
}

#if defined(USE_IMGUI)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	デバッグ描画パス
void scene_rendering::add_debug_draw_pass(frame_graph& graph, const scene_frame& frame, const std::shared_ptr<const debug_draw_resources>& resources)
{
	struct pass_data
	{
		std::shared_ptr<const debug_draw_resources> resources;
		D3D11_VIEWPORT viewport{};
	};
	graph.add_pass<pass_data>("debug_draw_pass", frame.context, [&](frame_graph::builder& builder, pass_data& data)
	{
		//	@tool 出力先（バックバッファ、またはビューの出力テクスチャ）へ重ねて描く
		frame_graph::color_attachment output;
		output.texture = frame.textures.output;
		output.external_view = frame.output.view;
		output.clear = false;
		builder.set_render_targets({ output }, scene_depth_attachment(frame, false, true));
		data.viewport = frame.output.viewport;
		data.resources = resources;
	},
	[](const pass_data& data, const frame_graph::resources&, render_job_context& context)
	{
		//	ビューポートの設定
		context.recorder.set_viewport(data.viewport);

		//	ライン描画
		for (UINT index = 0; index < static_cast<int>(debug_draw_primitive_container::line_buffer_id::max); ++index)
		{
			context.recorder.set_pipeline(*data.resources->debug_draw_line_pipeline_states[index]);
			if (!context.render_data->debug_draw_container.line_draw_items[index].color_meshs.empty())
			{
				auto& mesh = context.render_data->debug_draw_container.line_draw_items[index].color_meshs[0];
				auto it = mesh.vertex_buffer_views->begin();
				UINT stride = static_cast<UINT>(it->second.stride_in_bytes);
				UINT offset = static_cast<UINT>(it->second.offset());
				context.recorder.set_vertex_buffers(0, 1, it->second.buffer.GetAddressOf(), &stride, &offset);

				context.recorder.draw(static_cast<UINT>(it->second.count()), 0);
			}
			context.recorder.end_pipeline();
		}
	});
}
#endif // defined(USE_IMGUI)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	@tool 共有資源の生成（深度プリパス・G-Buffer・デカール・ディファードライティング・デバッグ描画）
void scene_rendering::initialize_deferred_features(scene_render_features& features)
{
	pipeline_state_manager* instance = pipeline_state_manager::instance();

	//	G-Buffer
	{
		auto next = std::make_shared<gbuffer_resources>();
		//	シェーダー準備
		instance->generate_pipeline_state<skybox_pipeline_state>("skybox", &next->skybox_pipeline_states);
		next->skybox_pipeline_states->set_domain(pipeline_state_base::domains::system);
		features.gbuffer = next;
	}

	//	G-Bufferデカール
	{
		auto next = std::make_shared<decal_resources>();

		//	書き込み用デフォルトプリミティブ
		next->cube = std::make_shared<geometric_cube>(render_data::instance()->get_device().Get());

		//	定数バッファの生成
		{
			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			buffer_desc.CPUAccessFlags = 0;
			buffer_desc.MiscFlags = 0;
			buffer_desc.StructureByteStride = 0;
			{
				buffer_desc.ByteWidth = sizeof(common_pipeline_states::decal_primitive_constants);
				HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, next->decal_primitive_primitive_buffer.GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			}
		}

		//	ステンシル書き込みシェーダー取得
		instance->generate_pipeline_state<deferred_decal_stencil_write_pipeline_state>("deferred_decal_stencil_write", &next->stencil_write_pipeline_state);
		features.decal = next;
	}

	//	ディファードライティング
	{
		auto next = std::make_shared<deferred_lighting_resources>();
		std::shared_ptr<material>& deferred_lighting_material = next->deferred_lighting_material;

		//	ディファードレンダリング系シェーダー準備
		{
			static constexpr const char* shader_names[] =
			{
				"deferred_indirect_lighting",
				"deferred_directional_lighting",
				"deferred_point_lighting",
				"deferred_spot_lighting",
			};
			//	
			deferred_lighting_material = std::make_shared<material>();
			deferred_lighting_material->set_domain(material::domains::system);
			deferred_lighting_material->set_flag(material::flags::two_sided, true);
			deferred_lighting_material->set_blend_mode(material::blend_modes::addative);	//	加算合成

			static constexpr std::string_view vs_path_list[] =
			{
				"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
				"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
				"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
				"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			};
			static constexpr std::string_view ps_path_list[] =
			{
				"Shaders/DeferredRendering/deferred_rendering_indirect_ps.hlsl",
				"Shaders/DeferredRendering/deferred_rendering_directional_ps.hlsl",
				"Shaders/DeferredRendering/deferred_rendering_point_ps.hlsl",
				"Shaders/DeferredRendering/deferred_rendering_spot_ps.hlsl",
			};
			static constexpr material::system_paths system_paths_list[] =
			{
				material::system_paths::deferred_indirect,
				material::system_paths::deferred_directional,
				material::system_paths::deferred_point,
				material::system_paths::deferred_spot,
			};
				
			//	deferred_indirect_lighting
			for (UINT index = 0; index < ARRAYSIZE(shader_names); ++ index)
			{
				std::shared_ptr<graphics_custom_pipeline_state> pso;
				instance->generate_pipeline_state<graphics_custom_pipeline_state>(shader_names[index], &pso);
				pso->set_domain(pipeline_state_base::domains::system);
				pso->set_flag(pipeline_state_base::flags::hidden, false);
				//	頂点シェーダー読み込み
				{
					load_shader_file desc;
					desc.file_path = vs_path_list[index];
	#if defined(_DEBUG)
					system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
					system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
	#endif	//	defined(_DEBUG)

					std::shared_ptr<vertex_shader> shader;
					shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
					pso->set_shader(shader);
				}
				//	ピクセルシェーダー読み込み
				{
					load_shader_file desc;
					desc.file_path = ps_path_list[index];
	#if defined(_DEBUG)
					system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
					system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
	#endif	//	defined(_DEBUG)

					std::shared_ptr<pixel_shader> shader;
					shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
					pso->set_shader(shader);
				}
				deferred_lighting_material->set_pipeline_state(system_paths_list[index], shader_names[index]);
			}
		}
		features.deferred_lighting = next;
	}

#if defined(USE_IMGUI)
	//	デバッグ描画
	{
		auto next = std::make_shared<debug_draw_resources>();
		static constexpr std::string_view pso_names[] =
		{
			"debug_draw_line_0",
			"debug_draw_line_1",
		};
		//	パイプラインステート生成
		int index;
		index = static_cast<int>(debug_draw_primitive_container::line_buffer_id::normal);
		instance->generate_pipeline_state<debug_draw_line_pipeline_state>(pso_names[index], &next->debug_draw_line_pipeline_states[index]);
		next->debug_draw_line_pipeline_states[index]->set_domain(pipeline_state_base::domains::system);

		index = static_cast<int>(debug_draw_primitive_container::line_buffer_id::disable_depth_test);
		instance->generate_pipeline_state<debug_draw_line_disable_depth_test_pipeline_state>(pso_names[index], &next->debug_draw_line_pipeline_states[index]);
		next->debug_draw_line_pipeline_states[index]->set_domain(pipeline_state_base::domains::system);
		features.debug_draw = next;
	}
#endif	//	defined(USE_IMGUI)
}
