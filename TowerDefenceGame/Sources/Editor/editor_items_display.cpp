#include "Editor/editor_items.h"

#if defined(USE_IMGUI)
#include <commdlg.h>
#include <random>
#include <set>

void editor_items::render_model_bones(const model_instance& model_instance, const std::vector<std::string_view>& selected_bones)
{
	std::vector<const char*> list;
	for (auto& selected_bone : selected_bones)
	{
		list.push_back(selected_bone.data());
	}
	editor_items::render_model_bones(model_instance, list.data(), list.size());
}
void editor_items::render_model_bones(const model_instance& model_instance, const std::vector<std::string>& selected_bones)
{
	std::vector<const char*> list;
	for (auto& selected_bone : selected_bones)
	{
		list.push_back(selected_bone.data());
	}
	editor_items::render_model_bones(model_instance, list.data(), list.size());
}
void editor_items::render_model_bones(const model_instance& model_instance, const char** selected_bones, std::size_t selected_bone_size)
{
	auto& nodes = model_instance.get_nodes();
	auto& meshes = model_instance.get_meshes();

	auto is_select_bone = [&](std::string_view selected_bone_name)
	{
		for (UINT index = 0; index < selected_bone_size; ++index)
		{
			if (selected_bone_name == std::string_view(selected_bones[index]))
			{
				return true;
			}
		}
		return false;
	};

	math::matrixf WorldMatrix(model_instance.get_world_transform());
	for (UINT node_index = 0; node_index < nodes.size(); ++node_index)
	{
		auto& node = nodes[node_index];

		//	メッシュノードはいったん除外
		{
			bool find = false;
			for (UINT mesh_node_index = 0; mesh_node_index < meshes.size(); ++mesh_node_index)
			{
				if (node_index == meshes[mesh_node_index].mesh->node_index)
				{
					find = true;
					break;
				}
			}
			if (find)
				continue;
		}

		math::matrixf NodeWorldMatrix = math::matrixf(node.global_transform) * WorldMatrix;
		math::float3 start;
		start = NodeWorldMatrix.to_vector3(3).to_float3();

		std::vector<size_t> children;
		for (UINT child_node_index = 0; child_node_index < nodes.size(); ++child_node_index)
		{
			if (node_index == nodes[child_node_index].parent_index)
			{
				children.push_back(child_node_index);
			}
		}

		math::float4 color = math::float4(0.0f, 0.0f, 0.0f, 1.0f);
		if (is_select_bone(node.name.data()))
		{
			color = math::float4(1.0f, 0.0f, 0.0f, 1.0f);
		}

		//	軸描画
		{
			math::vector3f Scale, Translation;
			math::quaternionf Rotation;
			NodeWorldMatrix.decompose(Scale, Rotation, Translation);
			math::float4 rotation;
			rotation = Rotation.to_float4();
			auto rotator = math::rotatorf(math::quaternionf(rotation));
			auto front = rotator.front().to_float3();
			auto right = rotator.right().to_float3();
			auto up = rotator.up().to_float3();

			math::float3 end[3];
			static constexpr float joint_length = 0.3f;
			end[0].x = start.x + right.x * joint_length;
			end[0].y = start.y + right.y * joint_length;
			end[0].z = start.z + right.z * joint_length;

			end[1].x = start.x + up.x * joint_length;
			end[1].y = start.y + up.y * joint_length;
			end[1].z = start.z + up.z * joint_length;

			end[2].x = start.x + front.x * joint_length;
			end[2].y = start.y + front.y * joint_length;
			end[2].z = start.z + front.z * joint_length;
			QUEUE_DEBUG_PRIMITIVE_LINE(start, end[0], math::float4(1.0f, 0.0f, 0.0f, 1.0f), false);
			QUEUE_DEBUG_PRIMITIVE_LINE(start, end[1], math::float4(0.0f, 1.0f, 0.0f, 1.0f), false);
			QUEUE_DEBUG_PRIMITIVE_LINE(start, end[2], math::float4(0.0f, 0.0f, 1.0f, 1.0f), false);
		}

		//	子供なしの時
		if (children.empty())
		{
			QUEUE_DEBUG_PRIMITIVE_SPHERE(start, 0.1f, color, false);
			continue;
		}

		//	子供有りの時
		for (auto& child : children)
		{
			auto& child_node = nodes[child];
			math::float3 end;
			end = ((math::matrixf(child_node.global_transform) * WorldMatrix).to_vector3(3)).to_float3();

			math::vector3f Start(start);
			math::vector3f End(end);
			math::vector3f Vector = End - Start;
			float length = Vector.length();
			if (length <= 0.000001f)
			{
				QUEUE_DEBUG_PRIMITIVE_SPHERE(start, 0.1f, color, false);
				continue;
			}

			math::vector3f Front = Vector.normalized();
			math::vector3f Up(0, 1, 0);
			if (fabsf(fabsf(Front.z) - 1) <= 0.000001f)
			{
				Up = math::vector3f(0, 0, -Front.z);
			}
			math::vector3f Right = (math::vector3f::cross(Front, Up)).normalized();
			Up = (math::vector3f::cross(Right, Front)).normalized();
			Right = (math::vector3f::cross(Front, Up)).normalized();
			math::float3 front, right, up;
			right = Right.to_float3();
			up = Up.to_float3();
			front = Front.to_float3();

			math::float3 center, cpxpy, cpxmy, cmxpy, cmxmy;
			float bone_length = 0.1f * std::min(1.0f, length);
			center.x = start.x + front.x * length * bone_length;
			center.y = start.y + front.y * length * bone_length;
			center.z = start.z + front.z * length * bone_length;

			cpxpy.x = center.x + right.x * bone_length + up.x * bone_length;
			cpxpy.y = center.y + right.y * bone_length + up.y * bone_length;
			cpxpy.z = center.z + right.z * bone_length + up.z * bone_length;
			cpxmy.x = center.x + right.x * bone_length - up.x * bone_length;
			cpxmy.y = center.y + right.y * bone_length - up.y * bone_length;
			cpxmy.z = center.z + right.z * bone_length - up.z * bone_length;

			cmxmy.x = center.x - right.x * bone_length + up.x * bone_length;
			cmxmy.y = center.y - right.y * bone_length + up.y * bone_length;
			cmxmy.z = center.z - right.z * bone_length + up.z * bone_length;
			cmxpy.x = center.x - right.x * bone_length - up.x * bone_length;
			cmxpy.y = center.y - right.y * bone_length - up.y * bone_length;
			cmxpy.z = center.z - right.z * bone_length - up.z * bone_length;

			QUEUE_DEBUG_PRIMITIVE_LINE(start, cpxpy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(start, cpxmy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(start, cmxpy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(start, cmxmy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cpxpy, cpxmy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cpxmy, cmxpy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cmxpy, cmxmy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cmxmy, cpxpy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cpxpy, end, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cpxmy, end, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cmxpy, end, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cmxmy, end, color, false);
		}
	}
}

void editor_items::render_model_select_bones(const model_instance& model_instance, const std::vector<std::string_view>& selected_bones)
{
	std::vector<const char*> list;
	for (auto& selected_bone : selected_bones)
	{
		list.push_back(selected_bone.data());
	}
	editor_items::render_model_select_bones(model_instance, list.data(), list.size());
}
void editor_items::render_model_select_bones(const model_instance& model_instance, const std::vector<std::string>& selected_bones)
{
	std::vector<const char*> list;
	for (auto& selected_bone : selected_bones)
	{
		list.push_back(selected_bone.data());
	}
	editor_items::render_model_select_bones(model_instance, list.data(), list.size());
}
void editor_items::render_model_select_bones(const model_instance& model_instance, const char** selected_bones, std::size_t selected_bone_size)
{
	auto& nodes = model_instance.get_nodes();
	auto& meshes = model_instance.get_meshes();

	auto is_select_bone = [&](std::string_view selected_bone_name)
		{
			for (UINT index = 0; index < selected_bone_size; ++index)
			{
				if (selected_bone_name == std::string_view(selected_bones[index]))
				{
					return true;
				}
			}
			return false;
		};

	math::matrixf WorldMatrix(model_instance.get_world_transform());
	for (UINT node_index = 0; node_index < nodes.size(); ++node_index)
	{
		auto& node = nodes[node_index];
		if (!is_select_bone(node.name.data()))
			continue;

		math::matrixf NodeWorldMatrix = math::matrixf(node.global_transform) * WorldMatrix;
		math::float3 start;
		start = NodeWorldMatrix.to_vector3(3).to_float3();

		std::vector<size_t> children;
		for (UINT child_node_index = 0; child_node_index < nodes.size(); ++child_node_index)
		{
			if (node_index == nodes[child_node_index].parent_index)
			{
				children.push_back(child_node_index);
			}
		}

		math::float4 color = math::float4(0.0f, 0.0f, 0.0f, 1.0f);

		//	軸描画
		{
			math::vector3f Scale, Translation;
			math::quaternionf Rotation;
			NodeWorldMatrix.decompose(Scale, Rotation, Translation);
			math::float4 rotation;
			rotation = Rotation.to_float4();
			auto rotator = math::rotatorf(math::quaternionf(rotation));
			auto front = rotator.front().to_float3();
			auto right = rotator.right().to_float3();
			auto up = rotator.up().to_float3();

			math::float3 end[3];
			static constexpr float joint_length = 0.3f;
			end[0].x = start.x + right.x * joint_length;
			end[0].y = start.y + right.y * joint_length;
			end[0].z = start.z + right.z * joint_length;

			end[1].x = start.x + up.x * joint_length;
			end[1].y = start.y + up.y * joint_length;
			end[1].z = start.z + up.z * joint_length;

			end[2].x = start.x + front.x * joint_length;
			end[2].y = start.y + front.y * joint_length;
			end[2].z = start.z + front.z * joint_length;
			QUEUE_DEBUG_PRIMITIVE_LINE(start, end[0], math::float4(1.0f, 0.0f, 0.0f, 1.0f), false);
			QUEUE_DEBUG_PRIMITIVE_LINE(start, end[1], math::float4(0.0f, 1.0f, 0.0f, 1.0f), false);
			QUEUE_DEBUG_PRIMITIVE_LINE(start, end[2], math::float4(0.0f, 0.0f, 1.0f, 1.0f), false);
		}

		//	子供なしの時
		if (children.empty())
		{
			QUEUE_DEBUG_PRIMITIVE_SPHERE(start, 0.1f, color, false);
			continue;
		}

		//	子供有りの時
		for (auto& child : children)
		{
			auto& child_node = nodes[child];
			math::float3 end;
			end = ((math::matrixf(child_node.global_transform) * WorldMatrix).to_vector3(3)).to_float3();

			math::vector3f Start(start);
			math::vector3f End(end);
			math::vector3f Vector = End - Start;
			float length = Vector.length();
			if (length <= 0.000001f)
			{
				QUEUE_DEBUG_PRIMITIVE_SPHERE(start, 0.1f, color, false);
				continue;
			}

			math::vector3f Front = Vector.normalized();
			math::vector3f Up(0, 1, 0);
			if (fabsf(fabsf(Front.z) - 1) <= 0.000001f)
			{
				Up = math::vector3f(0, 0, -Front.z);
			}
			math::vector3f Right = (math::vector3f::cross(Front, Up)).normalized();
			Up = (math::vector3f::cross(Right, Front)).normalized();
			Right = (math::vector3f::cross(Front, Up)).normalized();
			math::float3 front, right, up;
			right = Right.to_float3();
			up = Up.to_float3();
			front = Front.to_float3();

			math::float3 center, cpxpy, cpxmy, cmxpy, cmxmy;
			float bone_length = 0.1f * std::min(1.0f, length);
			center.x = start.x + front.x * length * bone_length;
			center.y = start.y + front.y * length * bone_length;
			center.z = start.z + front.z * length * bone_length;

			cpxpy.x = center.x + right.x * bone_length + up.x * bone_length;
			cpxpy.y = center.y + right.y * bone_length + up.y * bone_length;
			cpxpy.z = center.z + right.z * bone_length + up.z * bone_length;
			cpxmy.x = center.x + right.x * bone_length - up.x * bone_length;
			cpxmy.y = center.y + right.y * bone_length - up.y * bone_length;
			cpxmy.z = center.z + right.z * bone_length - up.z * bone_length;

			cmxmy.x = center.x - right.x * bone_length + up.x * bone_length;
			cmxmy.y = center.y - right.y * bone_length + up.y * bone_length;
			cmxmy.z = center.z - right.z * bone_length + up.z * bone_length;
			cmxpy.x = center.x - right.x * bone_length - up.x * bone_length;
			cmxpy.y = center.y - right.y * bone_length - up.y * bone_length;
			cmxpy.z = center.z - right.z * bone_length - up.z * bone_length;

			QUEUE_DEBUG_PRIMITIVE_LINE(start, cpxpy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(start, cpxmy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(start, cmxpy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(start, cmxmy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cpxpy, cpxmy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cpxmy, cmxpy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cmxpy, cmxmy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cmxmy, cpxpy, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cpxpy, end, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cpxmy, end, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cmxpy, end, color, false);
			QUEUE_DEBUG_PRIMITIVE_LINE(cmxmy, end, color, false);
		}
	}
}

#endif	//	defined(USE_IMGUI)


