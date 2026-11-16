#pragma once

#include <memory>
#include <string_view>

class framework;

//	フレームワークプロセス
class framework_process
{
public:
	//	呼び出し順
	enum class orders : int
	{
		core = 0,			//	共通の準備
		asset_watch = 10,	//	アセットの読み直し
		input = 20,			//	入力
		graphics = 30,		//	描画関係
		phase = 50,			//	フェーズ処理(シェーダーのコンパイル・ゲームなど)
		audio = 60,			//	サウンド
		editor = 70,		//	エディター
	};

public:
	virtual orders get_order() const { return orders::phase; }
	virtual std::string_view get_key() const { return {}; }

	//	閉じているか？
	virtual bool is_closed() const { return false; }

	//	終了リクエストは済んでるか？
	bool is_close_requested() const { return close_requested; }

protected:
	framework& get_owner() { return owner; }

public:
	explicit framework_process(framework& owner) : owner(owner) {}
	virtual ~framework_process() = default;

	framework_process(const framework_process&) = delete;
	framework_process& operator=(const framework_process&) = delete;

public:
	virtual bool enter() { return true; }
	virtual void exit() {}
	virtual void update(float elapsed_time) {}
	virtual void render(float elapsed_time) {}
	virtual void submit_render_jobs(float elapsed_time) {}
	virtual void post_render() {}

#if defined(USE_IMGUI)
	virtual void imgui_render() {}
#endif	//	defined(USE_IMGUI)

	//	終了リクエスト
	void request_close() { close_requested = true; }

public:
	virtual bool create_display_resources() { return true; }
	virtual void release_display_resources() {}
	virtual std::unique_ptr<framework_process> next_process() { return nullptr; }

private:
	framework& owner;
	bool close_requested = false;
};
