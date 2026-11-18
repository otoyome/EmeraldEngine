#pragma once

#include "Graphics/View/camera.h"
#include "object/camera_object.h"

#if	defined(USE_IMGUI)
//	エディターカメラ
class editor_camera_object : public camera_object
{
	GAME_OBJECT_DECRLEAR(editor_camera_object, camera_object)

public:
	static constexpr std::string_view left_click_down_identifier = "editor_camera::left_click_down_identifier";
	static constexpr std::string_view left_click_hold_identifier = "editor_camera::left_click_hold_identifier";
	static constexpr std::string_view left_click_release_identifier = "editor_camera::left_click_release_identifier";
	struct mouse_position_data
	{
		math::float2 position;
		math::float2 viewport_size = { 1280.0f, 720.0f };
	};

public:
	void set_rotation(const math::rotatorf& rotation) {this->rotation = rotation;}
	const math::rotatorf& get_rotation() const {return rotation;}
	void set_position(const math::vector3d& position) { this->position = position; }
	const math::vector3d& get_position() const { return position; }

	void set_rotation_speed(float rotation_speed) { this->rotation_speed = rotation_speed; }
	float get_rotation_speed() const { return rotation_speed; }
	void set_move_speed(float move_speed) { this->move_speed = move_speed; }
	float get_move_speed() const { return move_speed; }

public:
	editor_camera_object(std::string_view name, const math::vector3d& position, const math::rotatorf& rotation, float speed_scale = 1.0f);
	~editor_camera_object() override;

	void post_update(float elapsed_time) override;
	void debug_update(float elapsed_time) override;

protected:
	void on_active() override;

private:
	void handle_input(float elapsed_time);
	std::shared_ptr<camera_object> find_game_camera() const;
	void apply_view(const view_parameters* base);

private:
	bool is_auto_rotate = false;

	math::rotatorf rotation;
	math::vector3d position;

	float rotation_speed = 0.0f;
	float move_speed = 0.0f;
	std::weak_ptr<camera_object> synced_game_camera;
};

#endif	//	defined(USE_IMGUI)
