#pragma once

#include "object/gameobjects/ui/ui_object.h"
#include "object/gameobjects/ui/ui_image.h"
#include "object/gameobjects/ui/ui_text.h"

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
	static constexpr size_t ui_max = magic_enum::enum_count<ui_id>();

	enum class directions
	{
		left,
		right,
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
		std::shared_ptr<ui_image> objects[ui_max];
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
	directions animation_direction = directions::left;
	shake_descs shake_desc;

	std::shared_ptr<ui_image> ui[ui_max];
	DirectX::XMFLOAT4 origin_rects[ui_max];
	DirectX::XMFLOAT4 origin_colors[ui_max];
	DirectX::XMFLOAT2 shake_offset = {0, 0};
};
