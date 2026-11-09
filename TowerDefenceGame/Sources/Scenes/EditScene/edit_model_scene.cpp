#include "scenes/editscene/edit_model_scene.h"
#include "Object/engine_context.h"
#include "Texture/texture_resource_utility.h"
#include "System/asset_browser.h"
#include "Resource/resource_manager.h"
#include "Texture/texture_importer.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <tuple>

#include "Model/model_instance.h"
#include "Model/model_collision_builder.h"
#include "Graphics/camera.h"

#include "object/light_object.h"

#include "object/model_object.h"

#include "collision/mesh.h"

#include "collision/cubic_sdf.h"

#include "Model/model_resource_utility.h"
#include "Model/model_buffer_pool.h"

#include "Graphics/custom_pipeline_state.h"

#include "Effect/effect_instance.h"

#if defined(USE_IMGUI)

edit_model_scene::edit_model_scene()
: scene_base()
{
}

edit_model_scene::~edit_model_scene()
{
}

bool edit_model_scene::initialize_core()
{
	//	メインカメラ生成
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(30.0f);
		main_camera->get_camera()->get_parameters().clip_distance = {1.0f, 0.0f};
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
		}

		//	間接光生成
		{
			auto sky_light = std::make_shared<sky_light_object>("スカイライト");

			texture_ptr ibl_texture;
			ibl_texture = texture_resource_utility::load_texture("Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.etx");
			sky_light->set_diffuse_ibl_texture(ibl_texture);
			ibl_texture = texture_resource_utility::load_texture("Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.etx");
			sky_light->set_specular_ibl_texture(ibl_texture);
			ibl_texture = texture_resource_utility::load_texture("Assets/Resources/Textures/lut_ggx.etx");
			sky_light->set_lut_ggx_texture(ibl_texture);

			register_object(sky_light);
		}
	}

	//	デフォルトでポストプロセスを適応する
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		post_process_volume = std::make_shared<post_process_volume_object>("ポストプロセスボリューム");
		post_process_volume->set_flag(post_process_volume::flags::unbind, true);
		{
			auto bloom_constant = post_process_volume->get_volume()->get_bloom_constant();
			auto lens_ghosts_constant = post_process_volume->get_volume()->get_lens_ghosts_constant();
			auto depth_of_field_constant = post_process_volume->get_volume()->get_depth_of_field_constant();
			auto screen_space_reflection_constant = post_process_volume->get_volume()->get_screen_space_reflection_constant();
			auto motion_blur_constant = post_process_volume->get_volume()->get_motion_blur_constant();
			bloom_constant.set_active(false);
			lens_ghosts_constant.set_active(false);
			depth_of_field_constant.set_active(false);
			screen_space_reflection_constant.set_active(false);
			motion_blur_constant.set_active(false);
			post_process_volume->get_volume()->set_bloom_constant(bloom_constant);
			post_process_volume->get_volume()->set_lens_ghosts_constant(lens_ghosts_constant);
			post_process_volume->get_volume()->set_depth_of_field_constant(depth_of_field_constant);
			post_process_volume->get_volume()->set_screen_space_reflection_constant(screen_space_reflection_constant);
			post_process_volume->get_volume()->set_motion_blur_constant(motion_blur_constant);
		}
		register_object(post_process_volume);

		//	マテリアル登録
		static constexpr std::string_view texts_list[][2] =
		{
			{
				"Assets/GameShader/PostProcess/color_filter_before_dof.hlsl",
				"被写界深度前ポストプロセスマテリアル",
			},
			{
				"Assets/GameShader/PostProcess/color_filter_after_dof.hlsl",
				"被写界深度後ポストプロセスマテリアル",
			},
			{
				"Assets/GameShader/PostProcess/color_filter_after_dof_translucency.hlsl",
				"被写界深度後半透明描画ポストプロセスマテリアル",
			},
			{
				"Assets/GameShader/PostProcess/color_filter_before_bloom.hlsl",
				"ブルーム前ポストプロセスマテリアル",
			},
			{
				"Engine/EngineShader/Materials/PostProcess/color_filter_after_tone_mapping.hlsl",
				"トーンマッピング後ポストプロセスマテリアル",
			}
		};
		material_resource::blendable_locations path_list[] =
		{
			material_resource::blendable_locations::before_dof,
			material_resource::blendable_locations::after_dof,
			material_resource::blendable_locations::after_dof_translucency,
			material_resource::blendable_locations::before_bloom,
			material_resource::blendable_locations::after_tone_mapping,
		};
		for (UINT index = 0; index < ARRAYSIZE(path_list); ++index)
		{
			std::shared_ptr<material_resource> post_process_material = std::make_shared<material_resource>();
			post_process_material->set_name(texts_list[index][1]);
			post_process_material->set_domain(material_resource::domains::post_process);
			post_process_material->set_flag(material_resource::flags::two_sided, true);
			post_process_material->set_blend_mode(material_resource::blend_modes::opaque);
			post_process_material->set_blendable_location(path_list[index]);
			post_process_material->set_shader_path(texts_list[index][0]);

			//	マテリアル登録
			post_process_volume->get_volume()->register_postprocess_materials(post_process_material);
		}
	}

	return initialize_edit_core();
}

bool edit_model_scene::uninitialize_core()
{
	get_context().get_objects().unregister_object(edit_model);
	get_context().get_objects().unregister_object(post_process_volume);
	return uninitialize_edit_core();
}

void edit_model_scene::update_core(float elapsed_time)
{
	if (load_model_path.empty())
	{
		std::string file_path;
		if (get_context().take_open_file(file_path))
		{
			load_model_path = file_path;
			is_edit_vertex_color = false;
		}
	}

	if (convert_option_data.kind != convert_option_data::kinds::none)
	{
		combine_backup_meshes.clear();
		combine_backup_nodes.clear();
		has_combine_backup = false;

		if (edit_model)
		{
			on_model_release();
		}

		//	モデルデータ生成
		{
			bool is_load = false;
			std::shared_ptr<model_resource> resource = std::make_shared<model_resource>();
			switch (convert_option_data.kind)
			{
				case convert_option_data::kinds::mmd:
				{
					data_converter::mmd::convert_model_data data;
					convert_option_data.mmd.file_path = load_model_path;
					is_load = data_converter::mmd::convert(*resource, convert_option_data.mmd);
					break;
				}
				case convert_option_data::kinds::gltf:
				{
					convert_option_data.gltf.file_path = load_model_path;
					is_load = data_converter::gltf::convert(*resource, convert_option_data.gltf);
					break;
				}
				case convert_option_data::kinds::emo:
				{
					resource = resource_manager::instance()->load_uncached<model_resource>(load_model_path);
					is_load = resource != nullptr;
					break;
				}
			}
			if (is_load && resource)
			{
				//	メッシュレット生成判定
				bool has_meshlets = false;
				if (convert_option_data.kind == convert_option_data::kinds::emo)
				{
					for (auto& mesh : resource->get_meshes())
					{
						if (!mesh.meshlets.empty())
						{
							has_meshlets = true;
							break;
						}
					}
				}
				if (!has_meshlets)
				{
					model_resource_utility::optimize_meshs(*resource);
					model_resource_utility::generate_meshlets(*resource);
				}

				//	モデル生成
				edit_model = std::make_shared<model_object>(load_model_path, resource);
			}
		}

		if (edit_model)
			on_model_setup();

		convert_option_data.kind = convert_option_data::kinds::none;
		load_model_path.clear();
	}

	update_edit_core(elapsed_time);
}

void edit_model_scene::imgui_render_core()
{
	//	ロードウィンドウ
	if (get_context().begin_window("Model"))
		imgui_model_loader_window();
	ImGui::End();

	//	エディターウィンドウ関連
	imgui_render_edit_core();
}

void edit_model_scene::save_model()
{
	if (!edit_model)
		return;

	//	EMO形式で吐き出す
	auto resource = edit_model->get_model_instance()->get_resource_for_editor();

	//	保存
	resource_manager::instance()->save(resource);
}

void edit_model_scene::generate_lod()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	model_resource_utility::generate_lod_meshs(*resource, generate_lod_level);

	edit_model->get_model_instance()->set_lod_index(0);
}

void edit_model_scene::generate_bounding_mesh_asset()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	//	当たり判定メッシュは常に統合して出力
	collision::generate_bounding_mesh_data data;
	data.WorldMatrix = math::matrixf();
	data.reverse_flag = bounding_mesh_option.reverse_flag;
	data.lod_level = bounding_mesh_option.use_lod ? bounding_mesh_option.lod_level : -1;

	const auto& meshes = resource->get_meshes();
	const auto& materials = resource->get_materials();

	//	各メッシュの中心と全体の範囲を求める
	struct mesh_center
	{
		size_t mesh_index;
		math::float3 center;
	};
	std::vector<mesh_center> centers;
	math::float3 all_min = { FLT_MAX, FLT_MAX, FLT_MAX };
	math::float3 all_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index)
	{
		math::float3 mesh_min, mesh_max;
		if (!collision::calculate_bounding_mesh_bounds(resource, mesh_index, data, mesh_min, mesh_max))
			continue;

		centers.push_back({ mesh_index, { (mesh_min.x + mesh_max.x) * 0.5f, (mesh_min.y + mesh_max.y) * 0.5f, (mesh_min.z + mesh_max.z) * 0.5f } });
		all_min = { std::min(all_min.x, mesh_min.x), std::min(all_min.y, mesh_min.y), std::min(all_min.z, mesh_min.z) };
		all_max = { std::max(all_max.x, mesh_max.x), std::max(all_max.y, mesh_max.y), std::max(all_max.z, mesh_max.z) };
	}
	if (centers.empty())
		return;

	//	中心が入るグリッドセルの番号
	const math::int3 divisions =
	{
		std::max(bounding_mesh_option.combine_grid_divisions.x, 1),
		std::max(bounding_mesh_option.combine_grid_divisions.y, 1),
		std::max(bounding_mesh_option.combine_grid_divisions.z, 1),
	};
	auto calculate_cell = [](float value, float min, float max, int division)
	{
		const float size = max - min;
		if (size <= FLT_EPSILON)
			return 0;
		return std::clamp(static_cast<int>((value - min) / size * static_cast<float>(division)), 0, division - 1);
	};

	//	摩擦係数とセルが同じメッシュを1グループにまとめる
	using group_key = std::tuple<float, int, int, int>;
	std::map<group_key, size_t> group_lookup;
	std::vector<std::vector<size_t>> groups;
	std::vector<std::uint32_t> group_materials;
	for (auto& mesh : centers)
	{
		const size_t material_index = meshes[mesh.mesh_index].material_index;
		const bool has_material = material_index < materials.size();
		const float friction = has_material ? materials[material_index]->get_physical_data().friction : material_resource::physical_datas{}.friction;

		group_key key
		{
			friction,
			calculate_cell(mesh.center.x, all_min.x, all_max.x, divisions.x),
			calculate_cell(mesh.center.y, all_min.y, all_max.y, divisions.y),
			calculate_cell(mesh.center.z, all_min.z, all_max.z, divisions.z),
		};
		auto [it, inserted] = group_lookup.try_emplace(key, groups.size());
		if (inserted)
		{
			groups.emplace_back();
			group_materials.push_back(has_material ? static_cast<std::uint32_t>(material_index) : collision::combined_mesh::invalid_material);
		}
		groups[it->second].push_back(mesh.mesh_index);
	}

	//	グループ毎に統合メッシュを出力して最後にリスト情報を書き出す
	const std::string_view model_path = resource->get_file_path();
	collision::combined_mesh info;
	for (size_t group_index = 0; group_index < groups.size(); ++group_index)
	{
		collision::bounding_mesh out_mesh;
		if (!collision::generate_combined_bounding_mesh(out_mesh, resource, groups[group_index], data))
			continue;

		const std::filesystem::path out_path = collision::combined_mesh::make_mesh_path(model_path, info.material_indices.size());
		if (!out_mesh.serialize(out_path.string()))
			continue;
		info.material_indices.push_back(group_materials[group_index]);
	}
	info.serialize(collision::combined_mesh::make_info_path(model_path).string());
}

void edit_model_scene::generate_cubic_sdf_asset()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	std::filesystem::path model_path{ std::string(resource->get_file_path()) };
	for (size_t mesh_index = 0; mesh_index < resource->get_meshes().size(); ++mesh_index)
	{
		collision::generate_bounding_mesh_data data;
		data.WorldMatrix = math::matrixf();
		data.reverse_flag = bounding_mesh_option.reverse_flag;
		data.lod_level = bounding_mesh_option.use_lod ? bounding_mesh_option.lod_level : -1;

		collision::cubic_sdf out_sdf;
		if (!collision::create_cubic_sdf(out_sdf, resource, mesh_index, data,
			static_cast<uint32_t>(cubic_sdf_option.resolution), cubic_sdf_option.bounds_padding_rate))
			continue;

		std::filesystem::path out_path = model_path;
		out_path.replace_extension(std::to_string(mesh_index) + "." + std::string(collision::cubic_sdf::extension));
		out_sdf.serialize(out_path.string());
	}
}

void edit_model_scene::limit_texture_resolution_asset()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	//	マテリアルが参照しているテクスチャリソースを集める
	std::set<texture_resource*> textures;
	for (const auto& material : resource->get_materials())
	{
		for (const auto& shader_resource : material->get_shader_resources())
		{
			for (const auto& slot : shader_resource.second.resources)
			{
				if (slot.asset && !slot.asset->is_missing())
					textures.insert(slot.asset.get().get());
			}
		}
	}

	//	最大解像度の設定を変えて元画像から取り込み直す
	const std::uint32_t max_size = static_cast<std::uint32_t>(std::max(texture_max_resolution, 4));
	size_t changed_count = 0;
	size_t failed_count = 0;
	for (auto* texture : textures)
	{
		texture_import_settings settings = texture->get_settings();
		if (settings.max_resolution == max_size)
			continue;

		settings.max_resolution = max_size;
		const std::string file_path(texture->get_file_path());
		if (!texture_importer::instance()->import(settings.source_path, settings))
		{
			++failed_count;
			DEBUG_LOG_CATEGORY(logger::categorys::model, "Limit Texture Resolution Failed[%s]", file_path.c_str());
			continue;
		}
		++changed_count;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::model, "Limit Texture Resolution[max=%u] textures=%zu changed=%zu failed=%zu",
					   max_size, textures.size(), changed_count, failed_count);
}

void edit_model_scene::generate_shadow_mesh_asset()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	model_resource_utility::generate_shadow_meshes(*resource);
}

void edit_model_scene::discard_lod()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	model_resource_utility::clear_lod_meshs(*resource);
	edit_model->get_model_instance()->set_lod_index(0);
}

void edit_model_scene::discard_shadow_mesh()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	model_resource_utility::clear_shadow_meshes(*resource);
}

void edit_model_scene::optimize_mesh_asset()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	model_resource_utility::optimize_meshs(*resource);
}

void edit_model_scene::generate_meshlet_asset()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	model_resource_utility::generate_meshlets(*resource);
}

void edit_model_scene::strip_white_vertex_color_asset()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	//	全頂点が白の COLOR0 は削除
	static const semantic color0_semantic("COLOR", 0);
	size_t stripped_count = 0;
	size_t kept_count = 0;
	auto strip = [&](std::unordered_map<semantic, buffer_view>& vertex_buffer_views)
	{
		auto it = vertex_buffer_views.find(color0_semantic);
		if (it == vertex_buffer_views.end())
			return;

		const buffer_view& view = it->second;
		bool is_white = view.format == DXGI_FORMAT_R32G32B32A32_FLOAT && view.stride_in_bytes == sizeof(float) * 4 && !view.row_data.empty();
		if (is_white)
		{
			const float* colors = reinterpret_cast<const float*>(view.row_data.data());
			const size_t component_count = view.row_data.size() / sizeof(float);
			for (size_t index = 0; index < component_count; ++index)
			{
				if (colors[index] != 1.0f)
				{
					is_white = false;
					break;
				}
			}
		}
		if (!is_white)
		{
			++kept_count;
			return;
		}

		//	頂点プールの領域を返却してから削除
		std::unordered_map<semantic, buffer_view> released;
		released.emplace(it->first, std::move(it->second));
		vertex_buffer_views.erase(it);
		model_buffer_pool_manager::instance()->free_vertex_buffers(released);
		++stripped_count;
	};

	for (auto& mesh : resource->get_meshes_for_editor())
	{
		strip(mesh.vertex_buffer_views);
		for (auto& lod_mesh : mesh.lod_meshs)
			strip(lod_mesh.vertex_buffer_views);
	}
	DEBUG_LOG_CATEGORY(logger::categorys::model, "Strip White Vertex Color stripped=%zu kept=%zu", stripped_count, kept_count);
}

void edit_model_scene::combine_meshes()
{
	if (!edit_model)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	if (!has_combine_backup)
	{
		combine_backup_meshes = resource->get_meshes();
		combine_backup_nodes = resource->get_nodes();
		has_combine_backup = true;
	}
	else
	{
		resource->get_meshes_for_editor() = combine_backup_meshes;
		resource->get_nodes_for_editor() = combine_backup_nodes;
	}

	model_resource_utility::combine_unskinned_meshes(*resource, combine_grid_divisions);

	std::string path(resource->get_file_path());
	on_model_release();
	edit_model = std::make_shared<model_object>(path, resource);
	on_model_setup();
}

void edit_model_scene::clear_combined_meshes()
{
	if (!edit_model || !has_combine_backup)
		return;

	auto resource = edit_model->get_model_instance()->get_resource();
	if (!resource)
		return;

	resource->get_meshes_for_editor() = combine_backup_meshes;
	resource->get_nodes_for_editor() = combine_backup_nodes;
	combine_backup_meshes.clear();
	combine_backup_nodes.clear();
	has_combine_backup = false;

	std::string path(resource->get_file_path());
	on_model_release();
	edit_model = std::make_shared<model_object>(path, resource);
	on_model_setup();
}

#endif	//	defined(USE_IMGUI)
