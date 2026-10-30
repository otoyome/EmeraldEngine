#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Resource/resource_operation_state.h"
#include "Resource/resource_operation.h"

namespace resource
{
	//	型ごとの読込処理の基底クラス
	class loader_base
	{
	public:
		loader_base() = default;
		virtual ~loader_base() = default;

		loader_base(const loader_base&) = delete;
		loader_base& operator=(const loader_base&) = delete;

		//	ファイル読込/デシリアライズ
		virtual operation_state load(std::string_view file_path) = 0;

		//	GPUリソース生成やマネージャーへの登録などの構築処理
		//	必ずメインスレッドから呼ばれる
		virtual operation_state build() { return operation_state::success; }

		//	読込結果のリソース
		virtual std::shared_ptr<void> get_resource() const = 0;

	public:
		const std::vector<std::shared_ptr<load_operation_base>>& get_dependencies() const { return dependencies; }
		const std::string& get_error_message() const { return error_message; }

	protected:
		//	依存リソースの登録(load()内でresource_manager::load_async()した操作を渡す)
		void add_dependency(std::shared_ptr<load_operation_base> operation)
		{
			if (operation)
				dependencies.emplace_back(std::move(operation));
		}

		//	失敗理由の設定(load_operation_base::get_error_message()で取得できる)
		void set_error_message(std::string_view message)
		{
			error_message = message;
		}

	private:
		std::vector<std::shared_ptr<load_operation_base>> dependencies;
		std::string error_message;
	};

	//	型ごとに特殊化して使用する読込処理
	template<class Type>
	class loader;
}	//	namespace resource
