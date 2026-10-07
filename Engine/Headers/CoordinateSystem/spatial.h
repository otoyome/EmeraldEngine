#pragma once

#include <memory>

#include "Math/math_all.h"
#include "Math/transform.h"
#include "CoordinateSystem/hierarchy.h"

//	姿勢制御
class spatial : public hierarchy
{
	enum class flags
	{
		dirty,	//	再計算フラグ
		euler_hint,	//	local_euler_hint が有効か
	};

public:
	//	親子付け替え時の姿勢の扱い
	enum class attach_rule
	{
		keep_relative,	//	ローカル姿勢を維持
		keep_world,		//	ワールド姿勢を維持
	};

public:
	bool is_flag(flags flag) const;
	void set_flag(flags flag, bool value);

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

	std::uint32_t get_version() const;

	std::shared_ptr<spatial> get_parent() const;
	std::shared_ptr<spatial> get_child(std::size_t index) const;
	std::shared_ptr<spatial> get_root();

public:
	spatial();
	explicit spatial(const math::transformd& local);
	~spatial() override;

	//	親子関係の付け替え
	bool attach(const std::shared_ptr<spatial>& parent, attach_rule rule = attach_rule::keep_world);
	void detach(attach_rule rule = attach_rule::keep_world);

	//	自分以下の再計算が必要なワールド姿勢をまとめて計算する
	void update_descendants() const;

	template<class Archive>
	void serialize(Archive& archive, int version);

private:
	void mark_dirty();
	void update_world() const;

private:
	mutable UINT system_flags;			//	フラグ類
	std::uint32_t version;				//	変更回数

	math::transformd local;					//	ローカル姿勢
	mutable math::transformd world;			//	ワールド姿勢
	math::rotatord local_euler_hint;	//	最後に設定されたローカル回転のオイラー角
};

CEREAL_CLASS_VERSION(spatial, 1)
