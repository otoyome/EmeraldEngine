#include "SceneSystems/Editor/edit_particle_system.h"
#include "Object/engine_context.h"
#include "Texture/texture_resource_utility.h"
#include "System/asset_browser.h"
#include "Resource/resource_manager.h"
#include "Material/material_shader_map.h"
#include "Texture/texture_importer.h"

#include "Model/model_instance.h"
#include "Graphics/View/camera.h"

#include "object/light_object.h"
#include "object/model_object.h"
#include "object/post_process_volume_object.h"

#include "Graphics/Pipeline/custom_pipeline_state.h"

#include "Effect/effect_instance.h"
#include "Graphics/Debug/debug_primitive_collector.h"

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

edit_particle_system::edit_particle_system()
: scene_system()
{
	generate_default_effect();
}

edit_particle_system::~edit_particle_system()
{
}

bool edit_particle_system::initialize_core()
{
	//	平行光源の向き
	math::rotatorf light_rotation;

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
			light->set_flag(light::flags::use_shadow, false);
			register_object(light);
			light_rotation = math::rotatorf(light->get_world().get_euler());
		}

		//	間接光生成
		{
			auto sky_light = std::make_shared<sky_light_object>("スカイライト");

			texture_resource_ptr ibl_texture;
			ibl_texture = texture_resource_utility::load_texture_resource("Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.etx");
			sky_light->set_diffuse_ibl_texture(ibl_texture);
			ibl_texture = texture_resource_utility::load_texture_resource("Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.etx");
			sky_light->set_specular_ibl_texture(ibl_texture);
			ibl_texture = texture_resource_utility::load_texture_resource("Assets/Resources/Textures/lut_ggx.etx");
			sky_light->set_lut_ggx_texture(ibl_texture);

			register_object(sky_light);
		}
	}

	//	エディターカメラ生成
	{
		//	原点を見て、平行光源の向きの反対へ約10m離れた位置から映す
		const math::float3 light_front = light_rotation.front().to_float3();
		const double distance = system_units::convert_m_to_unit(10.0f);
		const math::vector3d camera_position(-light_front.x * distance, -light_front.y * distance, -light_front.z * distance);
		const math::rotatorf camera_rotation = math::rotatorf::from_direction(math::vector3f(light_front.x, light_front.y, light_front.z));

		//	エディターカメラは始めにメインカメラの姿勢へ合わせるので、メインカメラも同じ姿勢にしておく
		main_camera->set_world_translation(camera_position);
		main_camera->set_world_euler(math::rotatord(camera_rotation));
		main_camera->get_camera()->get_parameters().position = camera_position;
		main_camera->get_camera()->get_parameters().rotation = camera_rotation;

		editor_camera = std::make_shared<editor_camera_object>("エディターカメラ",
			camera_position,
			camera_rotation,
			0.25f);
		register_object(editor_camera);
	}

	//	エディターアイテム用コンテキスト生成
	{
		editor_context = editor_items::generate_context();
		editor_context->set_window_id(get_context().get_window_id());
	}

	//	新規パーティクル生成
	{
		on_new();
	}
	return true;
}

bool edit_particle_system::uninitialize_core()
{
	if (edit_instance)
		get_context().get_effects().unregister_instance(edit_instance);

	if (!edit_sdfs.empty())
	{
		for (auto& sdf : edit_sdfs)
		{
			get_context().get_effects().remove_cubic_sdf(sdf.sdf_id);
		}
		edit_sdfs.clear();
	}

	return true;
}

void edit_particle_system::update_core(float elapsed_time)
{
	{
		std::string file_path;
		if (get_context().take_open_file(file_path))
			open_effect(file_path);
	}

	draw_sdf_debug_primitives();
}

void edit_particle_system::open_effect(const std::string& file_path)
{
	effect_resource_ptr resource = resource_manager::instance()->load_uncached<effect_resource>(file_path);
	if (!resource)
		return;

	if (edit_instance)
		get_context().get_effects().unregister_instance(edit_instance);
	edit_resource = resource;
	edit_instance = std::make_shared<effect_instance>(edit_resource);
	get_context().get_effects().register_instance(edit_instance);
}

bool edit_particle_system::has_panel(panel_regions region) const
{
	switch (region)
	{
		case panel_regions::menu_bar:
		case panel_regions::left:
		{
			return true;
		}
		case panel_regions::right:
		case panel_regions::bottom:
		{
			return edit_resource != nullptr;
		}
	}
	return false;
}

void edit_particle_system::imgui_render_panel(panel_regions region)
{
	switch (region)
	{
		case panel_regions::menu_bar:
		{
			imgui_render_menu_bar();
			break;
		}
		case panel_regions::left:
		{
			//	一覧をタブで切り替える
			if (ImGui::BeginTabBar("##particle_left_tabs"))
			{
				//	ノードのツリー。詳細は右に出す
				if (ImGui::BeginTabItem("Node"))
				{
					if (edit_resource && editor_items::edit_effect_node_tree(editor_context, get_context(), edit_instance, selected_node_index))
						rebuild_edit_instance();
					ImGui::EndTabItem();
				}
				//	cubic_sdf読込み関係
				if (ImGui::BeginTabItem("SDF"))
				{
					imgui_render_sdf();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
			break;
		}
		case panel_regions::right:
		{
			if (editor_items::edit_effect_node_detail(editor_context, get_context(), edit_instance, selected_node_index))
				rebuild_edit_instance();
			break;
		}
		case panel_regions::bottom:
		{
			//	エフェクト編集
			if (edit_resource && ImGui::BeginTable("##particle_bottom", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("##timeline", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("##inspector", ImGuiTableColumnFlags_WidthFixed, 320.0f);
				ImGui::TableNextRow();

				//	ドラッグで選ぶ時に窓が動かないようにする
				ImGui::TableSetColumnIndex(0);
				if (ImGui::BeginChild("##timeline", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove))
				{
					if (editor_items::edit_effect_animation(editor_context, edit_instance))
						rebuild_edit_instance();
				}
				ImGui::EndChild();

				ImGui::TableSetColumnIndex(1);
				if (ImGui::BeginChild("##inspector", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove))
					editor_items::edit_animation_timeline_inspector(editor_context);
				ImGui::EndChild();

				ImGui::EndTable();
			}
			break;
		}
	}
}

void edit_particle_system::imgui_render_menu_bar()
{
	//	パーティクル読込み関係
	if (ImGui::BeginMenu("File"))
	{
		//	保存
		if (ImGui::MenuItem("Save", nullptr, false, edit_resource && !edit_resource->get_file_path().empty()))
			resource_manager::instance()->save_uncached(edit_resource, edit_resource->get_file_path());
		ImGui::EndMenu();
	}

	//	cubic_sdf読込み関係
	if (ImGui::BeginMenu("SDF"))
	{
		if (ImGui::MenuItem("Load SDF"))
			load_sdf_files();
		ImGui::EndMenu();
	}
}

void edit_particle_system::rebuild_edit_instance()
{
	if (edit_instance)
		get_context().get_effects().unregister_instance(edit_instance);
	edit_instance = std::make_shared<effect_instance>(edit_resource);
	get_context().get_effects().register_instance(edit_instance);
}

void edit_particle_system::on_new()
{
	if (edit_instance)
		get_context().get_effects().unregister_instance(edit_instance);

	new_effect(edit_resource);
	edit_instance = std::make_shared<effect_instance>(edit_resource);
	get_context().get_effects().register_instance(edit_instance);
}

void edit_particle_system::generate_default_effect()
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
						material->set_shader_path(material_shader_map::get_default_shader_path(material_resource::domains::particle));

						//	パラメーター設定
						material->set_float4("base_color_factor", math::float4(1, 1, 1, 1));
						if (auto texture = texture_importer::instance()->import("Assets/Resources/Textures/Explosion01_5x5.png", texture_import_settings::make_preset(texture_import_settings::presets::color)))
							material->set_texture("BaseColorMap", texture);
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
			resource_manager::instance()->save(resource, file_pathes[0]);
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
						material->set_shader_path(material_shader_map::get_default_shader_path(material_resource::domains::particle));

						//	パラメーター設定
						material->set_float4("base_color_factor", math::float4(1, 1, 1, 1));
						if (auto texture = texture_importer::instance()->import("Assets/Resources/Textures/Explosion01_5x5.png", texture_import_settings::make_preset(texture_import_settings::presets::color)))
							material->set_texture("BaseColorMap", texture);
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
			resource_manager::instance()->save(resource, file_pathes[1]);
		}
	}
}

void edit_particle_system::load_sdf_files()
{
	std::vector<std::string> file_paths;
	if (!editor_items::open_file_dialog_multi(SdfFileDialogFilter, file_paths))
		return;

	for (auto& file_path : file_paths)
	{
		collision::cubic_sdf sdf;
		if (!sdf.deserialize(file_path))
			continue;

		uint32_t sdf_id = get_context().get_effects().add_cubic_sdf(sdf);
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

void edit_particle_system::imgui_render_sdf()
{
	ImGui::Text("Cubic SDF ");
	ImGui::SameLine();
	if (ImGui::Button("Load SDF"))
		load_sdf_files();

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
			get_context().get_effects().remove_cubic_sdf(edit_sdfs[index].sdf_id);
			edit_sdfs.erase(edit_sdfs.begin() + index);
			removed = true;
		}
		ImGui::PopID();
		if (!removed)
			++index;
	}
}

void edit_particle_system::draw_sdf_debug_primitives()
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
		QUEUE_DEBUG_PRIMITIVE_BOX(get_context(), center, rotation, half_extent, color, true);

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

				QUEUE_DEBUG_PRIMITIVE_LINE(get_context(), 
					math::float3(p.x - half_cross, p.y, p.z),
					math::float3(p.x + half_cross, p.y, p.z),
					point_color, true);
				QUEUE_DEBUG_PRIMITIVE_LINE(get_context(), 
					math::float3(p.x, p.y - half_cross, p.z),
					math::float3(p.x, p.y + half_cross, p.z),
					point_color, true);
				QUEUE_DEBUG_PRIMITIVE_LINE(get_context(), 
					math::float3(p.x, p.y, p.z - half_cross),
					math::float3(p.x, p.y, p.z + half_cross),
					point_color, true);
			}
		}
	}
}


#endif	//	defined(USE_IMGUI)