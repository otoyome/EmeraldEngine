#pragma once

#include "camera.h"
#include "object/camera_object.h"

#if	defined(USE_IMGUI)
class editor_camera : public game_object
{
public:
	static constexpr std::string_view left_click_down_identifier = "editor_camera::left_click_down_identifier";
	static constexpr std::string_view left_click_hold_identifier = "editor_camera::left_click_hold_identifier";
	static constexpr std::string_view left_click_release_identifier = "editor_camera::left_click_release_identifier";
	struct mouse_position_data
	{
		math::float2 position;
	};

public:
	inline camera_ptr get_camera() const {return camera;}
	void set_rotation(const math::rotatorf& rotation) {this->rotation = rotation;}
	const math::rotatorf& get_rotation() {return rotation;}
	void set_position(const math::float3& position) { this->position = position; }
	const math::float3& get_position() { return position; }

	void set_rotation_speed(float rotation_speed) { this->rotation_speed = rotation_speed; }
	float get_rotation_speed() { return rotation_speed; }
	void set_move_speed(float move_speed) { this->move_speed = move_speed; }
	float get_move_speed() { return move_speed; }

public:
	editor_camera(std::shared_ptr<camera_object> source);
	~editor_camera();

	void debug_update(float elapsed_time) override;
	void pre_render() override;

	void on_execute();

protected:
	void on_active() override;
	void on_deactive() override;

private:
	std::shared_ptr<camera_object> source;
	camera_ptr camera;

	bool execute;
	bool is_auto_rotate = false;

	math::rotatorf rotation;
	math::float3 position;
	math::rotatorf restore_rotation;
	math::float3 restore_position;

	POINT cursor_position{ 0, 0 };
	float rotation_speed = 0.0125f * 0.5f;
	float move_speed = system_units::convert_m_to_unit(0.2f) * 0.5f;
};

#endif	//	defined(USE_IMGUI)
