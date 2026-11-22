#pragma once

#include "Object/Graphics/model_object.h"

#include "collision/mesh.h"

//	ステージオブジェクト
class stage_object : public game_object
{
	GAME_OBJECT_DECRLEAR(stage_object, game_object)

public:
	std::shared_ptr<model_object> get_model_object() const;
	void set_model_object(std::shared_ptr<model_object> model);

public:
	stage_object(std::string_view name);
	stage_object(std::string_view name, std::shared_ptr<model_object> model);
	~stage_object() override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

private:
	void update_transform();

private:
	std::shared_ptr<model_object> model;
};
