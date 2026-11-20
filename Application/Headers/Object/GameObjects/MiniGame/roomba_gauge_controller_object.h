#pragma once

#include "object/ui/ui_object.h"
#include "object/ui/ui_image.h"
#include "object/ui/ui_text.h"

//	ルンバ：ゲージ用オブジェクト
class roomba_gauge_controller_object : public game_object
{
	GAME_OBJECT_DECRLEAR(roomba_gauge_controller_object, game_object);

private:
	enum class animation_state
	{
		main,
		sub,
		idle,
	};

public:
	enum class ui_id
	{
		cover,
		base,
		main,
	};

public:
	struct shake_descs
	{
		float time = 0.0f;
		float power = 0.0f;
	};

	struct object_descs
	{
		std::string name;
		std::shared_ptr<ui_image> objects[magic_enum::enum_count<ui_id>()];
	};

public:
	roomba_gauge_controller_object(const object_descs& object_desc);
	~roomba_gauge_controller_object() override;

	void fixed_update(float elapsed_time) override;

	void post_update(float elapsed_time) override;

	void set_getter(std::function<int()> get_value, std::function<int()> get_max_value);
	void call_shake(const shake_descs& shake_desc);

	void set_alpha(float alpha);

private:
	std::function<int()> get_value;
	std::function<int()> get_max_value;

	int value = 1;
	int max_value = 1;
	int main_value_inner = 1;
	float animation_speed = 0.025f;
	shake_descs shake_desc;

	std::shared_ptr<ui_image> ui[magic_enum::enum_count<ui_id>()];
	math::float4 origin_rects[magic_enum::enum_count<ui_id>()];
	math::float4 origin_colors[magic_enum::enum_count<ui_id>()];
	math::float2 shake_offset = {0, 0};
};
