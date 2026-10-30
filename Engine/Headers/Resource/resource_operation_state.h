#pragma once

#include <cstddef>

namespace resource
{
	//	リソース操作の状況
	enum class operation_state
	{
		idle,				//	待機
		processing,			//	処理中
		success,			//	成功
		not_found,			//	ファイルが見つからない
		type_mismatch,		//	同じパスが別の型で読込済み/読込中
		deserialize_failed,	//	デシリアライズに失敗
		build_failed,		//	構築に失敗
		dependency_failed,	//	依存リソースが完了しなかった(循環依存など)
		canceled,			//	中断
	};

	//	読込優先度
	enum class load_priority
	{
		low,
		normal,
		high,
	};

	//	処理が終了しているか?
	inline bool is_finished(operation_state state)
	{
		return state != operation_state::idle && state != operation_state::processing;
	}

	//	表示用の名前
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

	//	表示用の名前取得
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
