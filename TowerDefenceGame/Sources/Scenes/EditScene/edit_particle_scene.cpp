#include "scenes/editscene/edit_particle_scene.h"

#include "model_instance.h"
#include "camera.h"

#include "object/light_object.h"
#include "object/model_object.h"
#include "object/post_process_volume_object.h"

#include "custom_pipeline_state.h"

#include "effect_instance.h"
#include "Graphics/debug_primitive_collector.h"

#if defined(USE_IMGUI)
#include "Editor/editor_camera.h"

namespace
{
	//	テストコード
	static constexpr std::string_view file_pathes[] =
	{
		"Assets/Resources/Effects/BasicBomb.eme",
		"Assets/Resources/Effects/BasicTrail.eme",
	};

	//	エフェクト
	constexpr const TCHAR* EffectFileDialogFilter =
		L"エフェクトファイル (*.eme)\0*.eme;\0\
		すべてのファイル (*.*)\0*.*\0";

	//	cubic SDF
	constexpr const TCHAR* SdfFileDialogFilter =
		L"cubic SDF (*.esdf)\0*.esdf;\0\
		すべてのファイル (*.*)\0*.*\0";

	//	ポストプロセスマテリアル
	constexpr material_resource::blendable_locations postprocess_material_path_list[] =
	{
		material_resource::blendable_locations::before_dof,
		material_resource::blendable_locations::after_dof,
		material_resource::blendable_locations::after_dof_translucency,
		material_resource::blendable_locations::before_bloom,
		material_resource::blendable_locations::after_tone_mapping,
	};
	constexpr std::string_view postprocess_material_texts_list[ARRAYSIZE(postprocess_material_path_list)][2] =
	{
		{
			"Shaders/Materials/PostProcess/color_filter_before_dof.hlsl",
			"被写界深度前ポストプロセスマテリアル",
		},
		{
			"Shaders/Materials/PostProcess/color_filter_after_dof.hlsl",
			"被写界深度後ポストプロセスマテリアル",
		},
		{
			"Shaders/Materials/PostProcess/color_filter_after_dof_translucency.hlsl",
			"被写界深度後半透明描画ポストプロセスマテリアル",
		},
		{
			"Shaders/Materials/PostProcess/color_filter_before_bloom.hlsl",
			"ブルーム前ポストプロセスマテリアル",
		},
		{
			"Shaders/Materials/PostProcess/color_filter_after_tone_mapping.hlsl",
			"トーンマッピング後ポストプロセスマテリアル",
		}
	};

	void new_effect(effect_resource_ptr& resource)
	{
		resource = std::make_shared<effect_resource>();

		auto& node_datas = resource->get_nodes_for_editor();
		auto& animations = resource->get_animations_for_editor();

		//	ルートノード生成
		{
			auto& node = node_datas.emplace_back();
			node.name = "root";
		}

		//	アニメーション生成
		{
			auto& animation_data = animations.emplace_back();
			animation_data.name = "default";
			animation_data.end_time = 1.0f;
			animation_data.nodes.emplace_back().node_name = node_datas.back().name;
		}
	}
}

edit_particle_scene::edit_particle_scene()
: scene_base()
{
	generate_default_effect();
}

edit_particle_scene::~edit_particle_scene()
{
}

bool edit_particle_scene::initialize_core()
{
	//	メインカメラ生成
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(30.0f);
	//	main_camera->get_camera()->set_flag(camera::flags::hybrid_projection, true);
	//	main_camera->get_camera()->set_hybrid_rate(0.3f);	//	透視投影
		register_object(main_camera);
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

	//	ライト生成
	{
		//	平行光源生成
		{
			auto light = std::make_shared<directional_light_object>("平行光源");
			auto color = light->get_color();
			color.w = 1.0f;
			light->set_color(color);
			light->set_flag(light::flags::use_shadow, false);
			register_object(light);
		}

		//	間接光生成
		{
			auto sky_light = std::make_shared<sky_light_object>("スカイライト");
			auto color = sky_light->get_color();
			color.w = 1.0f;
			sky_light->set_color(color);

			texture_ptr ibl_texture;
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.dds");
			sky_light->set_diffuse_ibl_texture(ibl_texture);
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.dds");
			sky_light->set_specular_ibl_texture(ibl_texture);
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/lut_ggx.dds");
			sky_light->set_lut_ggx_texture(ibl_texture);

			register_object(sky_light);
		}
	}

	//	ポストプロセス生成
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		auto post_process_volume = std::make_shared<post_process_volume_object>("ポストプロセスボリューム");
		post_process_volume->set_flag(post_process_volume::flags::unbind, true);
		{
			auto bloom_constant = post_process_volume->get_volume()->get_bloom_constant();
			auto lens_ghosts_constant = post_process_volume->get_volume()->get_lens_ghosts_constant();
			auto depth_of_field_constant = post_process_volume->get_volume()->get_depth_of_field_constant();
			auto screen_space_reflection_constant = post_process_volume->get_volume()->get_screen_space_reflection_constant();
			auto motion_blur_constant = post_process_volume->get_volume()->get_motion_blur_constant();
			auto exposure_compensation_constant = post_process_volume->get_volume()->get_exposure_compensation_constant();
			bloom_constant.set_active(false);
			lens_ghosts_constant.set_active(false);
			depth_of_field_constant.set_active(false);
			screen_space_reflection_constant.set_active(false);
			motion_blur_constant.set_active(false);
			exposure_compensation_constant.set_active(false);
			post_process_volume->get_volume()->set_bloom_constant(bloom_constant);
			post_process_volume->get_volume()->set_lens_ghosts_constant(lens_ghosts_constant);
			post_process_volume->get_volume()->set_depth_of_field_constant(depth_of_field_constant);
			post_process_volume->get_volume()->set_screen_space_reflection_constant(screen_space_reflection_constant);
			post_process_volume->get_volume()->set_motion_blur_constant(motion_blur_constant);
			post_process_volume->get_volume()->set_exposure_compensation_constant(exposure_compensation_constant);
		}
		register_object(post_process_volume);

		for (UINT index = 0; index < ARRAYSIZE(postprocess_material_path_list); ++index)
		{
			std::shared_ptr<material_resource> post_process_material = std::make_shared<material_resource>();
			post_process_material->set_name(postprocess_material_texts_list[index][1]);
			post_process_material->set_domain(material_resource::domains::post_process);
			post_process_material->set_flag(material_resource::flags::two_sided, true);
			post_process_material->set_blend_mode(material_resource::blend_modes::opaque);
			post_process_material->set_blendable_location(postprocess_material_path_list[index]);
			post_process_material->set_shader_path(postprocess_material_texts_list[index][0]);

			//	マテリアル登録
			post_process_volume->get_volume()->register_postprocess_materials(post_process_material);
		}
	}

	//	エディターカメラ生成
	{
		editor_camera_request = editor_camera_manager::instance()->request(
			system_units::convert_m_to_unit(math::vector3d(0.0, 1.0, 8.0)),
			{ 0, math::constants<float>::pi, 0 },
			0.25f);
	}

	//	エディターアイテム用コンテキスト生成
	{
		editor_context = editor_items::generate_context();
	}

	//	新規パーティクル生成
	{
		on_new();
	}
	return true;
}

bool edit_particle_scene::uninitialize_core()
{
	if (edit_instance)
		effect_manager::instance()->unregister_instance(edit_instance);

	if (!edit_sdfs.empty() && effect_manager::is_valid())
	{
		for (auto& sdf : edit_sdfs)
		{
			effect_manager::instance()->remove_cubic_sdf(sdf.sdf_id);
		}
		edit_sdfs.clear();
	}

	return true;
}

void edit_particle_scene::update_core(float elapsed_time)
{
	draw_sdf_debug_primitives();
}

void edit_particle_scene::imgui_render_core()
{
	//	パーティクル読込み関係
	{
		ImGui::Text("Effect ");
		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			std::string file_path;
			if (editor_items::open_file_dialog(EffectFileDialogFilter, file_path))
			{
				effect_resource_ptr resource;
				if (effect_resource_manager::instance()->load(resource, file_path))
				{
					if (edit_instance)
						effect_manager::instance()->unregister_instance(edit_instance);
					edit_resource = resource;
					edit_instance = std::make_shared<effect_instance>(edit_resource);
					effect_manager::instance()->register_instance(edit_instance);
				}
			}
		}

		//	保存
		if (edit_instance)
		{
			ImGui::SameLine();
			if (ImGui::Button("Save"))
			{
				std::string file_path;
				const std::wstring default_extension = L"." + std::wstring(effect_resource::extension.begin(), effect_resource::extension.end());
				if (editor_items::save_file_dialog(EffectFileDialogFilter, file_path, default_extension.c_str()))
				{
					edit_resource->serialize(file_path);
				}
			}
		}

		//	新規作成
		if (edit_instance)
		{
			ImGui::SameLine();
			if (ImGui::Button("New"))
			{
				on_new();
			}
		}
	}

	//	cubic_sdf読込み関係
	if (ImGui::Begin("SDF"))
	{
		imgui_render_sdf();
	}
	ImGui::End();

	//	エフェクト編集
	if (edit_resource)
	{
		//	エフェクト編集
		if (ImGui::Begin("Edit Effect"))
		{
			bool edit = editor_items::edit_effect_noeds(editor_context, edit_instance);
			if (editor_items::edit_effect_animation(editor_context, edit_instance) || edit)
			{
				if (edit_instance)
					effect_manager::instance()->unregister_instance(edit_instance);
				edit_instance = std::make_shared<effect_instance>(edit_resource);
				effect_manager::instance()->register_instance(edit_instance);
			}
		}
		ImGui::End();
	}
}

void edit_particle_scene::on_new()
{
	if (edit_instance)
		effect_manager::instance()->unregister_instance(edit_instance);

	new_effect(edit_resource);
	edit_instance = std::make_shared<effect_instance>(edit_resource);
	effect_manager::instance()->register_instance(edit_instance);
}

void edit_particle_scene::generate_default_effect()
{
	//	テスト用にスプライトパーティクルエミッター登録
	{
		static constexpr math::uint2 split_count = { 5, 5 };
		static constexpr float increment_time = system_units::convert_fps_to_sec(1.0f);

		effect_resource_ptr resource;
		if (!effect_resource_manager::instance()->load(resource, file_pathes[0]))
		{
			new_effect(resource);

			auto& node_datas = resource->get_nodes_for_editor();
			auto& particle_datas = resource->get_particles_for_editor();

			//	パーティクル生成
			{
				auto& node = node_datas.emplace_back();
				node.name = "bomb";
				node.parent_index = 0;
				auto& particle_data = particle_datas.emplace_back();
				particle_data.node_index = node_datas.size() - 1;
				//	レンダラー設定
				{
					auto renderer = particle_data.particle.register_item<particle::item::renderer>();

					renderer->type = particle::item::renderer::types::sprite;
					renderer->total_particle_count = 0x8FFF;

					//	UV設定
					{
						//renderer->uv.set_state(particle::item::renderer::texcoords::states::direct, true);
						renderer->uv.set_state(particle::item::renderer::texcoords::states::animation, true);
						renderer->uv.rect = {0, 0, 1, 1};
						renderer->uv.index = 0;
						renderer->uv.splits = split_count;
						renderer->uv.time = increment_time;
					}

					//	マテリアル設定
					{
						renderer->material = std::make_shared<material_resource>("default");
						auto material = renderer->material;
						material->set_domain(material_resource::domains::particle);
						material->set_alpha_clip_threshold(0.5f);
						material->set_shading_model(material_resource::shading_models::unlit);
						material->set_blend_mode(material_resource::blend_modes::mask);

						//	フラグ類設定
						material->set_flag(material_resource::flags::two_sided, true);
					//	material->set_flag(material_resource::flags::shadow_caster, true);
					//	material->set_flag(material_resource::flags::shadow_receiver, true);
						material->set_flag(material_resource::flags::indirect_lighting_off, true);

						//	デフォルトシェーダー設定
						material->set_shader_path(material_resource::default_particle_shader_path);

						//	パラメーター設定
						material->set_float4("base_color_factor", math::float4(1, 1, 1, 1));
						{
							texture_ptr texture;
							std::filesystem::path file_path = "Assets/Resources/Textures/Explosion01_5x5.png";
							if (texture_manager::instance()->load_texture_from_file(texture, file_path.string()))
							{
								texture->set_flag(texture::flags::srgb, true);
								material->set_texture("BaseColorMap", texture);
								texture_manager::instance()->save_texture(texture);
							}
						}
					}
				}
				//	発生設定
				{
					auto spawn = particle_data.particle.register_item<particle::item::spawn>();
					//spawn->set_state(particle::item::spawn::states::burst, true);
					spawn->set_state(particle::item::spawn::states::random, true);

					spawn->life_time_range =
					{
						split_count.x * split_count.y * increment_time,
						split_count.x * split_count.y * increment_time
					};
					spawn->scale_range =
					{
						3.0f,
						15.0f
					};
					spawn->area_min =
					{
						system_units::convert_m_to_unit(-4.0f),
						system_units::convert_m_to_unit(+0.0f),
						system_units::convert_m_to_unit(-4.0f)
					};
					spawn->area_max =
					{
						system_units::convert_m_to_unit(+4.0f),
						system_units::convert_m_to_unit(+0.0f),
						system_units::convert_m_to_unit(+4.0f)
					};
					spawn->range =
					{
						10,
						50,
					};
				}
			}
			//	ファイル保存
			resource->serialize(file_pathes[0]);
		}
	}

	//	テスト用にリボンパーティクルエミッター登録
	{
		static constexpr UINT TrailBodySize = 60;
		effect_resource_ptr resource;
		if (!effect_resource_manager::instance()->load(resource, file_pathes[1]))
		{
			new_effect(resource);

			auto& node_datas = resource->get_nodes_for_editor();
			auto& particle_datas = resource->get_particles_for_editor();

			//	パーティクル生成
			{
				auto& node = node_datas.emplace_back();
				node.name = "line particle";
				node.parent_index = 0;

				auto& particle_data = particle_datas.emplace_back();
				particle_data.node_index = node_datas.size() - 1;

				//	レンダラー設定
				{
					auto renderer = particle_data.particle.register_item<particle::item::renderer>();

					renderer->type = particle::item::renderer::types::ribon;
					renderer->total_particle_count = 0x1FFF;
					renderer->trail.max = TrailBodySize;

					//	UV設定
					{
						static constexpr math::uint2 split_count = { 5, 5 };
						static constexpr float increment_time = system_units::convert_fps_to_sec(1.0f);

						renderer->uv.set_state(particle::item::renderer::texcoords::states::animation, true);
						renderer->uv.rect = {0.001f, 0.001f, 0.999f, 0.999f};
						renderer->uv.index = 0;
						renderer->uv.splits = split_count;
						renderer->uv.time = increment_time;
					}

					//	マテリアル設定
					{
						renderer->material = std::make_shared<material_resource>("default");
						auto material = renderer->material;
						material->set_domain(material_resource::domains::particle);
						material->set_alpha_clip_threshold(0.01f);
						material->set_shading_model(material_resource::shading_models::unlit);
						material->set_blend_mode(material_resource::blend_modes::translucent);

						//	フラグ類設定
						material->set_flag(material_resource::flags::two_sided, true);
					//	material->set_flag(material_resource::flags::shadow_caster, true);
					//	material->set_flag(material_resource::flags::shadow_receiver, true);
						material->set_flag(material_resource::flags::indirect_lighting_off, true);

						//	デフォルトシェーダー設定
						material->set_shader_path(material_resource::default_particle_shader_path);

						//	パラメーター設定
						material->set_float4("base_color_factor", math::float4(1, 1, 1, 1));
						{
							texture_ptr texture;
							//std::filesystem::path file_path = "Assets/Resources/Textures/trail.png";
							std::filesystem::path file_path = "Assets/Resources/Textures/Explosion01_5x5.png";
							if (texture_manager::instance()->load_texture_from_file(texture, file_path.string()))
							{
								texture->set_flag(texture::flags::srgb, true);
								material->set_texture("BaseColorMap", texture);
								texture_manager::instance()->save_texture(texture);
							}
						}
					}
				}
				//	発生設定
				{
					auto spawn = particle_data.particle.register_item<particle::item::spawn>();
					spawn->set_state(particle::item::spawn::states::random, true);

					spawn->life_time_range =
					{
						10.0f,
						20.0f
					};
					spawn->scale_range =
					{
						1.5f,
						2.5f
					};
					spawn->area_min =
					{
						system_units::convert_m_to_unit(-4.0f),
						system_units::convert_m_to_unit(+0.0f),
						system_units::convert_m_to_unit(-4.0f)
					};
					spawn->area_max =
					{
						system_units::convert_m_to_unit(+4.0f),
						system_units::convert_m_to_unit(+4.0f),
						system_units::convert_m_to_unit(+4.0f)
					};
					spawn->range =
					{
						1,
						5,
					};
				}

				//	速度設定
				{
					auto velocity = particle_data.particle.register_item<particle::item::velocity>();
					velocity->set_state(particle::item::velocity::states::random_direction, true);
					velocity->set_state(particle::item::velocity::states::random_power, true);
					velocity->set_state(particle::item::velocity::states::random_acceleration, true);
					velocity->direction = { 0.0f, 1.0f, 0.0f };
					velocity->angle_range = { -math::constants<float>::pi / 4, +math::constants<float>::pi / 4 };
					velocity->power_range = { 100.0f, 200.0f };

					{
						velocity->acceleration_min.x = -10.0f;
						velocity->acceleration_min.y = -10.0f;
						velocity->acceleration_min.z = -10.0f;
						velocity->acceleration_max.x = +10.0f;
						velocity->acceleration_max.y = +10.0f;
						velocity->acceleration_max.z = +10.0f;
					}

					velocity->gravity_scale = { 1.0f, 1.0f, 1.0f };
				}
			}

			//	ファイル保存
			resource->serialize(file_pathes[1]);
		}
	}
}

void edit_particle_scene::imgui_render_sdf()
{
	ImGui::Text("Cubic SDF ");
	ImGui::SameLine();
	if (ImGui::Button("Load SDF"))
	{
		std::vector<std::string> file_paths;
		if (editor_items::open_file_dialog_multi(SdfFileDialogFilter, file_paths))
		{
			for (auto& file_path : file_paths)
			{
				collision::cubic_sdf sdf;
				if (!sdf.deserialize(file_path))
					continue;

				uint32_t sdf_id = effect_manager::instance()->add_cubic_sdf(sdf);
				if (sdf_id != 0xFFFFFFFF)
				{
					edit_sdf_data data;
					data.file_path = file_path;
					data.sdf_id = sdf_id;
					data.bounds_min = sdf.bounds_min;
					data.bounds_max = sdf.bounds_max;
					data.resolution = sdf.resolution;

					//	ボクセル可視化用に表面付近のボクセルだけ抽出
					{
						constexpr size_t max_points = 4000;
						const float voxel_size = sdf.voxel_size();
						const float threshold = voxel_size * 0.75f;

						std::vector<math::float4> candidates;
						for (uint32_t z = 0; z < sdf.resolution; ++z)
						{
							for (uint32_t y = 0; y < sdf.resolution; ++y)
							{
								for (uint32_t x = 0; x < sdf.resolution; ++x)
								{
									float distance = sdf.voxels[sdf.index_of(x, y, z)];
									if (fabsf(distance) <= threshold)
									{
										auto voxel_center = sdf.voxel_center(x, y, z);
										candidates.emplace_back(voxel_center.x, voxel_center.y, voxel_center.z, distance);
									}
								}
							}
						}

						if (candidates.size() <= max_points)
						{
							data.voxel_points = std::move(candidates);
						}
						else
						{
							//	数が多い場合は等間隔に間引く
							data.voxel_points.reserve(max_points);
							const float stride = static_cast<float>(candidates.size()) / static_cast<float>(max_points);
							for (size_t i = 0; i < max_points; ++i)
							{
								data.voxel_points.emplace_back(candidates[static_cast<size_t>(i * stride)]);
							}
						}
					}

					edit_sdfs.emplace_back(data);
				}
			}
		}
	}

	//	登録済みリスト
	for (size_t index = 0; index < edit_sdfs.size(); )
	{
		ImGui::PushID(static_cast<int>(index));
		ImGui::Text(edit_sdfs[index].file_path.get<utf8_cache>().data());
		ImGui::SameLine();
		ImGui::Checkbox("Voxels", &edit_sdfs[index].show_voxels);
		ImGui::SameLine();
		bool removed = false;
		if (ImGui::Button("Remove"))
		{
			effect_manager::instance()->remove_cubic_sdf(edit_sdfs[index].sdf_id);
			edit_sdfs.erase(edit_sdfs.begin() + index);
			removed = true;
		}
		ImGui::PopID();
		if (!removed)
			++index;
	}
}

void edit_particle_scene::draw_sdf_debug_primitives()
{
	for (auto& sdf : edit_sdfs)
	{
		math::float3 center =
		{
			(sdf.bounds_min.x + sdf.bounds_max.x) * 0.5f,
			(sdf.bounds_min.y + sdf.bounds_max.y) * 0.5f,
			(sdf.bounds_min.z + sdf.bounds_max.z) * 0.5f,
		};
		math::float3 half_extent =
		{
			(sdf.bounds_max.x - sdf.bounds_min.x) * 0.5f,
			(sdf.bounds_max.y - sdf.bounds_min.y) * 0.5f,
			(sdf.bounds_max.z - sdf.bounds_min.z) * 0.5f,
		};
		math::float4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		math::float4 color = { 0.0f, 1.0f, 1.0f, 1.0f };
		QUEUE_DEBUG_PRIMITIVE_BOX(center, rotation, half_extent, color, true);

		//	表面付近のボクセルを小さい十字マークで表示
		//	赤 = 内側/負、緑 = 外側/表面付近
		if (sdf.show_voxels && sdf.resolution > 0)
		{
			const float voxel_size = (sdf.bounds_max.x - sdf.bounds_min.x) / static_cast<float>(sdf.resolution);
			const float half_cross = voxel_size * 0.4f;
			for (auto& point : sdf.voxel_points)
			{
				math::float3 p = { point.x, point.y, point.z };
				math::float4 point_color = point.w < 0.0f
					? math::float4(1.0f, 0.2f, 0.2f, 1.0f)
					: math::float4(0.2f, 1.0f, 0.2f, 1.0f);

				QUEUE_DEBUG_PRIMITIVE_LINE(
					math::float3(p.x - half_cross, p.y, p.z),
					math::float3(p.x + half_cross, p.y, p.z),
					point_color, true);
				QUEUE_DEBUG_PRIMITIVE_LINE(
					math::float3(p.x, p.y - half_cross, p.z),
					math::float3(p.x, p.y + half_cross, p.z),
					point_color, true);
				QUEUE_DEBUG_PRIMITIVE_LINE(
					math::float3(p.x, p.y, p.z - half_cross),
					math::float3(p.x, p.y, p.z + half_cross),
					point_color, true);
			}
		}
	}
}

#endif	//	defined(USE_IMGUI)