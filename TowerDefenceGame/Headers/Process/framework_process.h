#pragma once

#include <memory>

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
	explicit framework_process(framework& owner) : owner(owner) {}
	virtual ~framework_process() = default;

	framework_process(const framework_process&) = delete;
	framework_process& operator=(const framework_process&) = delete;

public:
	virtual orders get_order() const { return orders::phase; }

	virtual bool enter() { return true; }
	virtual void exit() {}
	virtual void update(float elapsed_time) {}
	virtual void render(float elapsed_time) {}
	virtual void submit_render_jobs(float elapsed_time) {}
	virtual void post_render() {}
#if defined(USE_IMGUI)
	virtual void imgui_render() {}
#endif	//	defined(USE_IMGUI)
	virtual std::unique_ptr<framework_process> next_process() { return nullptr; }

protected:
	framework& get_owner() { return owner; }

private:
	framework& owner;
};
