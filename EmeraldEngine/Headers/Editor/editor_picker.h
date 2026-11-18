#pragma once

#if defined(USE_IMGUI)
#include <memory>

#include "imgui.h"
#include "Model/model_resource.h"

class engine_context;
class game_object;
class model_object;

//	エディター用ピッカー
class editor_picker
{
public:
	//	オブジェクト選択
	static std::shared_ptr<game_object> pick(engine_context& context, const ImVec2& position, const ImVec2& image_size);

	//	レイキャスト
	static bool raycast(engine_context& context, const ImVec2& position, const ImVec2& image_size, bool include_shapeless,
						std::shared_ptr<game_object>& out_object, math::vector3d& out_position, float fallback_distance = 50.0f);

private:
	//	レイ情報
	struct ray
	{
		math::vector3f origin;
		math::vector3f direction;
	};

	//	モデルの三角形との交差
	static bool intersect_model(model_object& model, const ray& camera_ray, const math::vector3d& camera_position, float& nearest);

	//	形の無いオブジェクトは位置のまわりの球で当てる
	static bool intersect_sphere(const math::vector3f& center, float radius, const ray& camera_ray, float& nearest);

	//	三角形との交差
	static bool intersect_triangle(const math::vector3f& origin, const math::vector3f& direction, const math::vector3f& a, const math::vector3f& b, const math::vector3f& c, float& out_distance);

	//	姿勢行列算出
	static math::matrixf calculate_node_matrix(const model_resource& resource, size_t node_index);
};
#endif	//	defined(USE_IMGUI)
