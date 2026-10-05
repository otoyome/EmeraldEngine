#pragma once

#include <memory>

#include "Math/math_all.h"
#include "CoordinateSystem/transform.h"
#include "CoordinateSystem/hierarchy.h"

//	姿勢制御
class spatial : public hierarchy
{
	enum class flags
	{
		dirty,	//	再計算フラグ
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

	const transformd& get_local() const;
	void set_local(const transformd& local);
	void set_local_scale(const math::vector3d& scale);
	void set_local_rotation(const math::quaterniond& rotation);
	void set_local_translation(const math::vector3d& translation);

	const transformd& get_world() const;
	void set_world(const transformd& world);

	std::uint32_t get_version() const;

	bool attach(const std::shared_ptr<spatial>& parent, attach_rule rule = attach_rule::keep_world);
	void detach(attach_rule rule = attach_rule::keep_world);

	std::shared_ptr<spatial> get_parent() const;
	std::shared_ptr<spatial> get_child(std::size_t index) const;
	std::shared_ptr<spatial> get_root();

	//	自分以下の再計算が必要なワールド姿勢をまとめて計算する
	void update_descendants() const;

public:
	spatial();
	explicit spatial(const transformd& local);
	~spatial() override;

private:
	void mark_dirty();
	void update_world() const;

private:
	mutable UINT system_flags;	//	フラグ類
	std::uint32_t version;		//	変更回数

	transformd local;			//	ローカル姿勢
	mutable transformd world;	//	ワールド姿勢
};
