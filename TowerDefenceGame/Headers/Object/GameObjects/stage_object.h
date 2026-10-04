#pragma once

#include "object/model_object.h"

#include "collision/mesh.h"

//	ステージオブジェクト
class stage_object : public game_object
{
	GAME_OBJECT_DECRLEAR(stage_object, game_object)

public:
	std::shared_ptr<model_object> get_model_object() const;
	void set_model_object(std::shared_ptr<model_object> model);

	const math::float3& get_scale() const;
	void set_scale(const math::float3& scale);

	const math::rotatorf& get_rotation() const;
	void set_rotation(const math::rotatorf& rotator);

	const math::float3& get_position() const;
	void set_position(const math::float3& position);

public:
	stage_object(std::string_view name, std::shared_ptr<model_object> model);
	~stage_object() override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

private:
	void update_transform();

private:
	std::shared_ptr<model_object> model;
};
