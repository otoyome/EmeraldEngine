#include "object/gameobjects/stage_object.h"
#include "Object/game_object_registry.h"
#include "Object/game_object_serializer.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"

std::shared_ptr<model_object> stage_object::get_model_object() const
{
	return model;
}

void stage_object::set_model_object(std::shared_ptr<model_object> model)
{
	this->model = model;
}

stage_object::stage_object(std::string_view name)
: game_object(name)
{
}

stage_object::stage_object(std::string_view name, std::shared_ptr<model_object> model)
: game_object(name)
, model(model)
{
}

stage_object::~stage_object()
{
}

void stage_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	game_object_serializer::save_object_reference(archive, "model", model);
}

void stage_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	game_object_serializer::load_object_reference(archive, "model", model);
}

#if defined(USE_IMGUI)
void stage_object::imgui_render_core()
{
}
#endif	//	defined(USE_IMGUI)

#if 0
size_t stage_object::ray_cast(const math::float3& start, const math::float3& end, collision::intersect_data& intersect_data)
{
	return ray_cast(math::vector3f(start), math::vector3f(end), intersect_data);
}

size_t stage_object::ray_cast(const math::vector3f& Start, const math::vector3f& End, collision::intersect_data& intersect_data)
{
	if (!model || !model->get_model_instance() || !model->get_model_instance()->get_resource())
		return stage_object::invalid_material_index;

	const math::vector3f WorldStart = Start;
	const math::vector3f WorldEnd = End;
	//	レイキャスト前に姿勢を確定
	update_transform();

	auto& instance = model->get_model_instance();
	auto& resource = instance->get_resource();

	size_t material_index = invalid_material_index;
	for(auto& mesh : resource->get_meshes())
	{
		if (mesh.node_index == model_resource::invalid_id)
			continue;

		auto it = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
		if (it == mesh.vertex_buffer_views.end())
			continue;

		auto& node = instance->get_nodes()[mesh.node_index];
		math::matrixf WorldTransform(node.world_transform);
		//	AABBと直線とで交差判定してレイキャストを飛ばせるようにする
		{
			mesh.bounds_min, mesh.bounds_max;
		}

		math::matrixf InverseWorldTransform = WorldTransform.inverted();
		math::vector3f LocalStart = math::vector3f::transform_coord(WorldStart, InverseWorldTransform);
		math::vector3f LocalEnd = math::vector3f::transform_coord(WorldEnd, InverseWorldTransform);
		collision::triangles triangles;
		triangles.positions = reinterpret_cast<const math::float3*>(it->second.row_data.data());
		triangles.indices = reinterpret_cast<const UINT*>(mesh.index_buffer_view.row_data.data());
		triangles.index_count = mesh.index_buffer_view.count();
		triangles.reverse_index = true;
		collision::intersect_data temp_intersect_data;
		if (collision::ray_cast(LocalStart, LocalEnd, triangles, &temp_intersect_data))
		{
			math::vector3f LocalPoint(temp_intersect_data[0].point);
			math::vector3f WorldPoint = math::vector3f::transform_coord(LocalPoint, WorldTransform);
			float WorldLength = (WorldPoint - WorldStart).length();

			float distance = WorldLength;
			if (intersect_data.size() == 0)
			{
				intersect_data = temp_intersect_data;
				math::vector3f LocalNormal(temp_intersect_data[0].normal);
				math::vector3f WorldNormal = math::vector3f::transform_normal(LocalNormal, WorldTransform);
				intersect_data[0].point = WorldPoint.to_float3();
				intersect_data[0].normal = WorldNormal.to_float3();
				intersect_data[0].length = distance;
				material_index = mesh.material_index;
			}
			else if (distance < intersect_data[0].length)
			{
				math::vector3f LocalNormal(temp_intersect_data[0].normal);
				math::vector3f WorldNormal = math::vector3f::transform_normal(LocalNormal, WorldTransform);
				intersect_data[0].point = WorldPoint.to_float3();
				intersect_data[0].normal = WorldNormal.to_float3();
				intersect_data[0].length = distance;
				material_index = mesh.material_index;
			}
		}
	}

	return material_index;
}
#endif

void stage_object::update_transform()
{
	if (!model || !model->get_model_instance() || !model->get_model_instance()->get_resource())
		return;

	model->update_transform();
}

GAME_OBJECT_REGISTER(stage_object);
