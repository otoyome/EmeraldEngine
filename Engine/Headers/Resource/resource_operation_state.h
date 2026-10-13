#pragma once

#include <cstddef>

namespace resource
{
	//	@tool リソース操作の状況
	enum class operation_state
	{
		idle,				//	@tool 待機
		processing,			//	@tool 処理中
		success,			//	@tool 成功
		not_found,			//	@tool ファイルが見つからない
		type_mismatch,		//	@tool 同じパスが別の型で読込済み/読込中
		deserialize_failed,	//	@tool デシリアライズに失敗
		build_failed,		//	@tool 構築に失敗
		dependency_failed,	//	@tool 依存リソースが完了しなかった(循環依存など)
		canceled,			//	@tool 中断
	};

	//	@tool 読込優先度(ワーカーは高い物から処理する)
	enum class load_priority
	{
		low,
		normal,
		high,

		max,
	};
	static constexpr size_t load_priority_count = static_cast<size_t>(load_priority::max);

	//	@tool 処理が終了しているか(成否は問わない)
	inline bool is_finished(operation_state state)
	{
		return state != operation_state::idle && state != operation_state::processing;
	}

	//	@tool 表示用の名前
	inline const char* to_string(operation_state state)
	{
		switch (state)
		{
			case operation_state::idle:					return "idle";
			case operation_state::processing:			return "processing";
			case operation_state::success:				return "success";
			case operation_state::not_found:			return "not_found";
			case operation_state::type_mismatch:		return "type_mismatch";
			case operation_state::deserialize_failed:	return "deserialize_failed";
			case operation_state::build_failed:			return "build_failed";
			case operation_state::dependency_failed:	return "dependency_failed";
			case operation_state::canceled:				return "canceled";
		}
		return "unknown";
	}

	//	@tool 表示用の名前
	inline const char* to_string(load_priority priority)
	{
		switch (priority)
		{
			case load_priority::low:	return "low";
			case load_priority::normal:	return "normal";
			case load_priority::high:	return "high";
			default:					break;
		}
		return "unknown";
	}
}	//	namespace resource
