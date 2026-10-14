#include "Scenes/MiniGameScene/minigame_stage_01_scene.h"

#include "editor/editor_camera.h"

#include "model_instance.h"
#include "camera.h"
#include "effect_instance.h"

#include "Collision/cubic_sdf.h"
#include "Collision/combined_mesh.h"

#include "object/light_object.h"
#include "object/model_object.h"
#include "object/gameobjects/stage_object.h"
#include "object/gameobjects/system/start_point_object.h"

minigame_stage_01_scene::minigame_stage_01_scene()
: scene_base()
{
}

minigame_stage_01_scene::~minigame_stage_01_scene()
{
}

bool minigame_stage_01_scene::initialize_core()
{
	//	平行光源生成
	{
		auto directional = std::make_shared<directional_light_object>("平行光源");
		{
			int index = 0;
			auto bias = directional->get_light()->get_shadow_bias(index);
			bias.depth_bias = 0;
			directional->get_light()->set_shadow_bias(index, bias);
		}
		{
			int index = 1;
			auto bias = directional->get_light()->get_shadow_bias(index);
			bias.depth_bias = 0;
			directional->get_light()->set_shadow_bias(index, bias);
		}
		{
			int index = 2;
			auto bias = directional->get_light()->get_shadow_bias(index);
			bias.depth_bias = 0;
			bias.sigma_factor = 2.0f;
			directional->get_light()->set_shadow_bias(index, bias);
		}
		{
			int index = 3;
			auto bias = directional->get_light()->get_shadow_bias(index);
			bias.depth_bias = 0.03f;
			bias.sigma_factor = 2.0f;
			directional->get_light()->set_shadow_bias(index, bias);
		}
		{
			int index = 4;
			auto bias = directional->get_light()->get_shadow_bias(index);
			bias.depth_bias = 0.05f;
			bias.sigma_factor = 2.0f;
			directional->get_light()->set_shadow_bias(index, bias);
		}
		{
			int index = 5;
			auto bias = directional->get_light()->get_shadow_bias(index);
			bias.depth_bias = 0.07f;
			bias.sigma_factor = 2.0f;
			directional->get_light()->set_shadow_bias(index, bias);
		}
		register_object(directional);
	}

	//	その他光源設定
	if (0)
	{
		auto point = std::make_shared<point_light_object>("点光源");
		point->set_flag(light::flags::use_shadow, true);
		point->set_world_translation(system_units::convert_m_to_unit(math::double3(0.0, 5.0, -20.0)));
		point->set_range(system_units::convert_m_to_unit(10.0f));
		point->set_color({ 1.0f, 1.0f, 1.0f, 1.0f });
		register_object(point);

		auto spot = std::make_shared<spot_light_object>("スポットライト");
		spot->set_flag(light::flags::use_shadow, true);
		spot->set_world_translation(system_units::convert_m_to_unit(math::double3(0.0, 5.0, -30.0)));
		spot->set_world_euler(math::rotatord(math::constants<double>::pi * 0.2, 0.0, 0.0));
		spot->set_range(system_units::convert_m_to_unit(20.0f));
		spot->set_inner_cone(30.0f);
		spot->set_outer_cone(60.0f);
		spot->set_color({ 1.0f, 1.0f, 1.0f, 10.0f });
		register_object(spot);
	}

	//	間接光生成
	{
		auto sky_light = std::make_shared<sky_light_object>("スカイライト");
		texture_ptr ibl_texture;
		texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.dds");
		sky_light->set_diffuse_ibl_texture(ibl_texture);
		texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.dds");
		sky_light->set_specular_ibl_texture(ibl_texture);
		texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/lut_ggx.dds");
		sky_light->set_lut_ggx_texture(ibl_texture);
		register_object(sky_light);
	}

	//	ステージ
	{
		auto file_path = "Assets/Resources/Models/バーダス・コロシアム/バーダス・コロシアム.emo";
		std::filesystem::path path(file_path);
		assert (std::filesystem::exists(path));

		//	ステージ
		auto model = std::make_shared<model_object>("ステージ", path.string());
		register_object(model);
		model->get_model_instance()->set_flag(model_instance::flags::culling, true);
		model->get_model_instance()->set_flag(model_instance::flags::shadow_caster, true);
		auto stage = std::make_shared<stage_object>("ステージ制御", model);
		register_object(stage);

		//	メッシュ分当たり判定生成
		//	動かさない事前提
		const math::matrixf transform = stage->get_model_object()->get_world().to_relative_matrix(math::vector3d::zero());
		generate_model_collision(model->get_model_instance()->get_resource(), transform);
		generate_model_sdf(model->get_model_instance()->get_resource());

		//	プレイヤースタートポイント設定
		{
			auto player_point = std::make_shared<start_point_object>("プレイヤー開始位置 0", "Player Start 0");
			player_point->set_world_translation(system_units::convert_m_to_unit(math::double3(0.0, 0.0, -50.0)));
			player_point->set_world_rotation(math::rotatord(0, 0, 0));
			player_point->attach(model);
			register_object(player_point);
		}
		{
			auto player_point = std::make_shared<start_point_object>("プレイヤー開始位置 1", "Player Start 1");
			player_point->set_world_translation(system_units::convert_m_to_unit(math::double3(-50.0, 0.0, 0.0)));
			player_point->set_world_rotation(math::rotatord(0, 0, 0));
			player_point->attach(model);
			register_object(player_point);
		}
		{
			auto player_point = std::make_shared<start_point_object>("プレイヤー開始位置 2", "Player Start 2");
			player_point->set_world_translation(system_units::convert_m_to_unit(math::double3(0.0, 0.0, +50.0)));
			player_point->set_world_rotation(math::rotatord(0, 0, 0));
			player_point->attach(model);
			register_object(player_point);
		}
		{
			auto player_point = std::make_shared<start_point_object>("プレイヤー開始位置 3", "Player Start 3");
			player_point->set_world_translation(system_units::convert_m_to_unit(math::double3(+50.0, 0.0, 0.0)));
			player_point->set_world_rotation(math::rotatord(0, 0, 0));
			player_point->attach(model);
			register_object(player_point);
		}

		//	パーティクルポイント設定
		static constexpr float DustSetLength = 40;
		static constexpr float BubbleSetLength = DustSetLength / 2;
		static constexpr math::double3 DustBlock[] =
		{
			system_units::convert_m_to_unit(math::double3(-DustSetLength, 0.0, -DustSetLength)),
			system_units::convert_m_to_unit(math::double3(+DustSetLength, 0.0, -DustSetLength)),
			system_units::convert_m_to_unit(math::double3(           0.0, 0.0,            0.0)),
			system_units::convert_m_to_unit(math::double3(-DustSetLength, 0.0, +DustSetLength)),
			system_units::convert_m_to_unit(math::double3(+DustSetLength, 0.0, +DustSetLength)),
		};
		static constexpr math::double3 BubbleBlock[] =
		{
			system_units::convert_m_to_unit(math::double3(-BubbleSetLength, 0, -BubbleSetLength)),
			system_units::convert_m_to_unit(math::double3(+BubbleSetLength, 0, -BubbleSetLength)),
			system_units::convert_m_to_unit(math::double3(-BubbleSetLength, 0, +BubbleSetLength)),
			system_units::convert_m_to_unit(math::double3(+BubbleSetLength, 0, +BubbleSetLength)),
		};
		for (int index = 0; index < ARRAYSIZE(DustBlock); ++index)
		{
			std::string name = "ゴミ発生位置 " + std::to_string(index);
			std::string tag = "Dust Point " + std::to_string(index);
			auto particle_point = std::make_shared<start_point_object>(name, tag);
			particle_point->set_world_translation(DustBlock[index]);
			particle_point->set_world_rotation(math::rotatord(0, 0, 0));
			particle_point->attach(model);
			register_object(particle_point);
		}
		for (int index = 0; index < ARRAYSIZE(BubbleBlock); ++index)
		{
			std::string name = "泡発生位置 " + std::to_string(index);
			std::string tag = "Bubble Point " + std::to_string(index);
			auto particle_point = std::make_shared<start_point_object>(name, tag);
			particle_point->set_world_translation(BubbleBlock[index]);
			particle_point->set_world_rotation(math::rotatord(0, 0, 0));
			particle_point->attach(model);
			register_object(particle_point);
		}
	}

	return true;
}

bool minigame_stage_01_scene::uninitialize_core()
{
	if (!sdf_ids.empty() && effect_manager::is_valid())
	{
		auto device_context = render_data::instance()->get_immediate_context();
		for (auto sdf_id : sdf_ids)
		{
			effect_manager::instance()->remove_cubic_sdf(device_context.Get(), sdf_id);
		}
		sdf_ids.clear();
	}

	return true;
}

void minigame_stage_01_scene::update_core(float elapsed_time)
{
}

#if defined(USE_IMGUI)
void minigame_stage_01_scene::imgui_render_core()
{
	if (ImGui::TreeNode("minigame_stage_01_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)

void minigame_stage_01_scene::generate_model_collision(model_resource_ptr resource, const math::matrixf& transform)
{
	const std::string_view file_path = resource->get_file_path();
	const std::filesystem::path info_path = collision::combined_mesh::make_info_path(file_path);
	collision::combined_mesh info;
	if (!std::filesystem::exists(info_path) || !info.deserialize(info_path.string()))
	{
		DEBUG_LOG_CATEGORY(logger::categorys::model, "Load Failed CombinedMesh[%s]", info_path.string().c_str());
		return;
	}

	const auto& materials = resource->get_materials();
	for (size_t combined_index = 0; combined_index < info.material_indices.size(); ++combined_index)
	{
		std::shared_ptr<collision::bounding_mesh> mesh = std::make_shared<collision::bounding_mesh>();
		const std::filesystem::path mesh_path = collision::combined_mesh::make_mesh_path(file_path, combined_index);
		if (!std::filesystem::exists(mesh_path) || !mesh->deserialize(mesh_path.string()))
			continue;

		//	物理情報は統合時の代表マテリアルから取得する
		const std::uint32_t material_index = info.material_indices[combined_index];
		const bool has_material = material_index != collision::combined_mesh::invalid_material && material_index < materials.size();

		collision_object_ptr collision = std::make_shared<collision_object>();
#if defined(USE_IMGUI)
		std::string name = has_material ? std::string(materials[material_index].get_name()) : std::string("combined");
		name += " [" + std::to_string(combined_index) + "]";
		collision->set_name(name);
#endif	//	defined(USE_IMGUI)
		collision->set_bounding_object(mesh);
		collision->set_filter("stage");
		collision->set_flag(collision_object::flags::static_object, true);
		if (has_material)
			collision->set_physical_data(materials[material_index].get_physical_data());
		register_object(collision);
	}
}

void minigame_stage_01_scene::generate_model_sdf(model_resource_ptr resource)
{
	std::string file_path = std::string(resource->get_file_path());
	std::filesystem::path sdf_path(file_path);

	auto device_context = render_data::instance()->get_immediate_context();
	for (size_t mesh_index = 0; mesh_index < resource->get_meshes().size(); ++mesh_index)
	{
		//	シリアライズ済みファイルがあるか確認
		std::string extension = std::to_string(mesh_index) + "." + std::string(collision::cubic_sdf::extension);
		sdf_path = file_path;
		sdf_path.replace_extension(extension);
		if (!std::filesystem::exists(sdf_path))
			continue;

		collision::cubic_sdf sdf;
		if (!sdf.deserialize(sdf_path.string()))
			continue;

		uint32_t sdf_id = effect_manager::instance()->add_cubic_sdf(device_context.Get(), sdf);
		if (sdf_id != 0xFFFFFFFF)
			sdf_ids.push_back(sdf_id);
	}
}