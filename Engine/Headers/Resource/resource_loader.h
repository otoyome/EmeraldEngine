#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Resource/resource_operation_state.h"
#include "Resource/resource_operation.h"

namespace resource
{
	//	@tool 型ごとの読込処理の基底クラス
	//	@tool 1リクエストにつき1インスタンス生成されるので、load()とbuild()の間の中間データを持たせてよい
	//	@tool インスタンスの破棄は必ずメインスレッドで行われる
	class loader_base
	{
	public:
		loader_base() = default;
		virtual ~loader_base() = default;

		loader_base(const loader_base&) = delete;
		loader_base& operator=(const loader_base&) = delete;

		//	@tool ファイル読込/デシリアライズ
		//	@tool 通常はワーカースレッド、wait()時はメインスレッドから呼ばれる
		//	@tool スレッドセーフでない共有状態(各種マネージャー等)に触れないこと。resource_manager::load_async()は呼び出してよい
		virtual operation_state load(std::string_view file_path) = 0;

		//	@tool GPUリソース生成やマネージャーへの登録などの構築処理
		//	@tool 必ずメインスレッドから、add_dependency()した操作が全て完了した後に呼ばれる
		virtual operation_state build() { return operation_state::success; }

		//	@tool 読込結果のリソース
		virtual std::shared_ptr<void> get_resource() const = 0;

	public:
		const std::vector<std::shared_ptr<load_operation_base>>& get_dependencies() const { return dependencies; }
		const std::string& get_error_message() const { return error_message; }

	protected:
		//	@tool 依存リソースの登録(load()内でresource_manager::load_async()した操作を渡す)
		//	@tool 登録した操作が全て完了するまでbuild()は呼ばれない。依存先の成否はbuild()側で判断すること
		//	@tool 循環依存は不可(wait()時はdependency_failedになり、update()では完了しなくなる)
		void add_dependency(std::shared_ptr<load_operation_base> operation)
		{
			if (operation)
				dependencies.emplace_back(std::move(operation));
		}

		//	@tool 失敗理由の設定(load_operation_base::get_error_message()で取得できる)
		void set_error_message(std::string_view message)
		{
			error_message = message;
		}

	private:
		std::vector<std::shared_ptr<load_operation_base>> dependencies;
		std::string error_message;
	};

	//	@tool 型ごとに特殊化して使用する読込処理
	//	@tool 例) template<> class resource::loader<model_resource> : public resource::loader_base { ... };
	template<class Type>
	class loader;
}	//	namespace resource
