#include "Object/Collision/model_collision_object.h"

#include <filesystem>

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"

#include "Collision/cubic_sdf.h"
#include "Collision/combined_mesh.h"
#include "Collision/mesh.h"

#include "Effect/effect_instance.h"
#include "Model/model_resource.h"
#include "Object/engine_context.h"
#include "Object/game_object_registry.h"
#include "Object/game_object_serializer.h"
#include "System/asset_browser.h"
#include "Resource/asset_registry.h"

model_collision_object::model_collision_object(std::string_view name)
: collision_placement_object(name)
{
}

model_collision_object::~model_collision_object()
{
}

void model_collision_object::set_model(const core::uuid& value)
{
	model = value;
	rebuild_collisions();
}

void model_collision_object::set_register_sdf(bool value)
{
	register_sdf = value;
	rebuild_collisions();
}

void model_collision_object::create_collisions(std::vector<collision_object_ptr>& out_collisions)
{
	const std::string file_path = game_object_serializer::resolve_asset_path(model);
	if (file_path.empty())
		return;

	model_resource_ptr resource;
	if (!model_resource_manager::instance()->load(resource, file_path) || !resource)
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::model, "model_collision_object model load failed[%s]", file_path.c_str());
		return;
	}

	//	メッシュ毎に当たり判定を生成(なので事前にモデルエディターで生成しておいたほうが良い)
	const std::filesystem::path info_path = collision::combined_mesh::make_info_path(file_path);
	collision::combined_mesh info;
	if (!std::filesystem::exists(info_path) || !info.deserialize(info_path.string()))
	{
		DEBUG_LOG_CATEGORY(logger::categorys::model, "Load Failed CombinedMesh[%s]", info_path.string().c_str());
	}
	else
	{
		const auto& materials = resource->get_materials();
		for (size_t combined_index = 0; combined_index < info.material_indices.size(); ++combined_index)
		{
			std::shared_ptr<collision::bounding_mesh> mesh = std::make_shared<collision::bounding_mesh>();
			const std::filesystem::path mesh_path = collision::combined_mesh::make_mesh_path(file_path, combined_index);
			if (!std::filesystem::exists(mesh_path) || !mesh->deserialize(mesh_path.string()))
				continue;

			const std::uint32_t material_index = info.material_indices[combined_index];
			const bool has_material = material_index != collision::combined_mesh::invalid_material && material_index < materials.size() && materials[material_index];

			collision_object_ptr collision = std::make_shared<collision_object>();
#if defined(USE_IMGUI)
			std::string name = has_material ? std::string(materials[material_index]->get_name()) : std::string("combined");
			name += " [" + std::to_string(combined_index) + "]";
			collision->set_name(name);
#endif	//	defined(USE_IMGUI)
			collision->set_bounding_object(mesh);
			if (has_material)
				collision->set_physical_data(materials[material_index]->get_physical_data());
			out_collisions.emplace_back(std::move(collision));
		}
	}

	//	パーティクル用のSDF
	if (!register_sdf)
		return;
	for (size_t mesh_index = 0; mesh_index < resource->get_meshes().size(); ++mesh_index)
	{
		std::filesystem::path sdf_path(file_path);
		sdf_path.replace_extension(std::to_string(mesh_index) + "." + std::string(collision::cubic_sdf::extension));
		if (!std::filesystem::exists(sdf_path))
			continue;

		collision::cubic_sdf sdf;
		if (!sdf.deserialize(sdf_path.string()))
			continue;

		const uint32_t sdf_id = get_context().get_effects().add_cubic_sdf(sdf);
		if (sdf_id != 0xFFFFFFFF)
			sdf_ids.push_back(sdf_id);
	}
}

void model_collision_object::release_resources()
{
	if (has_context())
	{
		for (uint32_t sdf_id : sdf_ids)
			get_context().get_effects().remove_cubic_sdf(sdf_id);
	}
	sdf_ids.clear();
}

template<class Archive>
void model_collision_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	game_object_serializer::serialize_asset_reference(archive, "model", model);
	archive(CEREAL_NVP(register_sdf));
}

void model_collision_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	game_object_serializer::save_fields(archive, "model_collision_object", *this);
}

void model_collision_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	game_object_serializer::load_fields(archive, "model_collision_object", *this);
}

#if defined(USE_IMGUI)
void model_collision_object::imgui_render_core()
{
	base::imgui_render_core();

	//	ドロップ
	const cache_string path(game_object_serializer::resolve_asset_path(model));
	ImGui::Text("model : %s", model.is_empty() ? "(none)" : path.get<utf8_cache>().data());
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(asset_browser::drag_payload_type))
		{
			const std::string file_path(static_cast<const char*>(payload->Data));
			if (asset_registry::is_valid())
			{
				const core::uuid uuid = asset_registry::instance()->find_uuid(file_path);
				if (!uuid.is_empty())
					set_model(uuid);
			}
		}
		ImGui::EndDragDropTarget();
	}
	bool value = register_sdf;
	if (ImGui::Checkbox("register sdf", &value))
		set_register_sdf(value);
	ImGui::Text("sdf : %d", static_cast<int>(sdf_ids.size()));
}
#endif	//	defined(USE_IMGUI)

