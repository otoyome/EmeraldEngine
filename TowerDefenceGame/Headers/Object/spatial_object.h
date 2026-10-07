#pragma once

#include <mutex>

#include "CoordinateSystem/spatial.h"

//	親子関係と空間上の姿勢を持つオブジェクト
class spatial_object : public game_object
{
	GAME_OBJECT_DECRLEAR(spatial_object, game_object)

public:
	using attach_rule = ::spatial::attach_rule;

public:
	const std::shared_ptr<::spatial>& get_spatial() const;

	//	ローカル姿勢
	const math::transformd& get_local() const;
	math::rotatord get_local_euler() const;
	void set_local(const math::transformd& local);
	void set_local_scale(const math::vector3d& scale);
	void set_local_rotation(const math::quaterniond& rotation);
	void set_local_euler(const math::rotatord& euler);
	void set_local_translation(const math::vector3d& translation);

	//	ワールド姿勢
	const math::transformd& get_world() const;
	math::vector3d get_world_scale() const;
	math::quaterniond get_world_rotation() const;
	math::rotatord get_world_euler() const;
	math::vector3d get_world_translation() const;
	void set_world(const math::transformd& world);
	void set_world_scale(const math::vector3d& scale);
	void set_world_rotation(const math::quaterniond& rotation);
	void set_world_euler(const math::rotatord& euler);
	void set_world_translation(const math::vector3d& translation);

public:
	spatial_object(std::string_view name);
	spatial_object(std::string_view name, const math::transformd& local);
	virtual ~spatial_object();

	//	親子関係の付け替え
	bool attach(const std::shared_ptr<spatial_object>& parent, attach_rule rule = attach_rule::keep_world);
	void detach(attach_rule rule = attach_rule::keep_world);

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

private:
	std::shared_ptr<spatial> spatial;
};
