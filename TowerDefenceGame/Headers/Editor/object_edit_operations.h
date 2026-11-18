#pragma once

#if defined(USE_IMGUI)
#include <memory>
#include <optional>
#include <string_view>

class engine_context;
class game_object;

//	オブジェクト編集オペレーション
class object_edit_operations
{
public:
	virtual ~object_edit_operations() = default;

	//	編集しているワールドのコンテキスト
	virtual engine_context& get_edit_context() = 0;

	//	オブジェクトを型から作って置く
	virtual std::shared_ptr<game_object> place_object(std::string_view type, const std::optional<math::vector3d>& position = std::nullopt) = 0;
	//	オブジェクトを型から作って parent の子にする
	virtual std::shared_ptr<game_object> place_object_as_child(std::string_view type, const std::shared_ptr<game_object>& parent) = 0;

	//	オブジェクト削除
	virtual bool remove_object(const std::shared_ptr<game_object>& target) = 0;
	//	削除できるオブジェクトか
	virtual bool can_remove_object(const std::shared_ptr<game_object>& target) const = 0;

	//	親子関連付け
	virtual bool attach_object(const std::shared_ptr<game_object>& child, const std::shared_ptr<game_object>& parent) = 0;
};
#endif	//	defined(USE_IMGUI)
