#pragma once

#include <mutex>

#include "CoordinateSystem/spatial.h"

//	親子関係と空間上の姿勢を持つオブジェクト
class spatial_object : public game_object
{
	GAME_OBJECT_DECRLEAR(spatial_object, game_object)

	friend class game_object_serializer;
	static constexpr std::uint32_t serialize_version = 1;

public:
	using attach_rule = ::spatial::attach_rule;

	//	動く物体か
	enum class mobilities : std::uint8_t
	{
		fixed,		//	固定
		movable,	//	動体
	};

public:
	const std::shared_ptr<::spatial>& get_spatial() const;

	mobilities get_mobility() const { return mobility; }
	bool set_mobility(mobilities value);

	//	親子関係の取得
	std::shared_ptr<spatial_object> get_parent() const { return parent_object.lock(); }
	std::shared_ptr<spatial_object> get_root() const;
	std::vector<std::shared_ptr<spatial_object>> get_children() const;
	std::vector<std::shared_ptr<spatial_object>> get_siblings() const;
	std::vector<std::shared_ptr<spatial_object>> get_descendants() const;

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

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI

public:
	//	親子関係の付け替え
	bool attach(const std::shared_ptr<spatial_object>& parent, attach_rule rule = attach_rule::keep_world);
	void detach(attach_rule rule = attach_rule::keep_world);
	static bool attach_object(const std::shared_ptr<game_object>& child, const std::shared_ptr<game_object>& parent);

	//	自分が指定オブジェクトの祖先か
	bool is_ancestor_of(const spatial_object& object) const;

	//	名前で子を検索
	std::shared_ptr<spatial_object> find_child(std::string_view name, bool recursive = false) const;

	//	型で子を検索
	template<class Type>
	std::shared_ptr<Type> find_child(bool recursive = false) const
	{
		for (const std::weak_ptr<spatial_object>& weak_child : child_objects)
		{
			const std::shared_ptr<spatial_object> child = weak_child.lock();
			if (!child)
				continue;
			if (std::shared_ptr<Type> result = std::dynamic_pointer_cast<Type>(child))
				return result;
			if (recursive)
			{
				if (std::shared_ptr<Type> result = child->find_child<Type>(true))
					return result;
			}
		}
		return nullptr;
	}

	//	型で子を全て検索
	template<class Type>
	std::vector<std::shared_ptr<Type>> find_children(bool recursive = false) const
	{
		std::vector<std::shared_ptr<Type>> results;
		collect_children<Type>(results, recursive);
		return results;
	}

	//	型で親をさかのぼって検索
	template<class Type>
	std::shared_ptr<Type> find_parent() const
	{
		for (std::shared_ptr<spatial_object> parent = parent_object.lock(); parent; parent = parent->parent_object.lock())
		{
			if (std::shared_ptr<Type> result = std::dynamic_pointer_cast<Type>(parent))
				return result;
		}
		return nullptr;
	}

public:
	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

private:
	template<class Archive>
	void serialize_fields(Archive& archive, std::uint32_t version);

protected:
	bool can_move() const;

private:
	void detach_record();

	template<class Type>
	void collect_children(std::vector<std::shared_ptr<Type>>& results, bool recursive) const
	{
		for (const std::weak_ptr<spatial_object>& weak_child : child_objects)
		{
			const std::shared_ptr<spatial_object> child = weak_child.lock();
			if (!child)
				continue;
			if (std::shared_ptr<Type> result = std::dynamic_pointer_cast<Type>(child))
				results.emplace_back(std::move(result));
			if (recursive)
				child->collect_children<Type>(results, true);
		}
	};

private:
	std::shared_ptr<spatial> spatial;
	mobilities mobility = mobilities::movable;
	std::weak_ptr<spatial_object> parent_object;
	std::vector<std::weak_ptr<spatial_object>> child_objects;
};
