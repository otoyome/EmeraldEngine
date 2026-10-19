#pragma once

#include <memory>
#include <vector>

//	@tool アセットの更新を検知して読み直す（メインスレッドで毎フレーム呼ぶ）
//	@tool エディター構成でのみ動く。Assets（モデル .emo・エフェクト .eme・テクスチャ）と Shaders（.hlsl・.hlsli）フォルダーの変更通知を見て読み直す
class asset_watcher
{
public:
	asset_watcher();
	~asset_watcher();
	asset_watcher(const asset_watcher&) = delete;
	asset_watcher& operator=(const asset_watcher&) = delete;

	//	@tool モデルを読み直した時は true（呼び出し側で全ワールドのモデルインスタンスを作り直す）
	bool update();

private:
#if defined(USE_IMGUI)
	struct directory_watch;
	std::vector<std::unique_ptr<directory_watch>> watches;
	bool watch_started = false;
#endif	//	defined(USE_IMGUI)
};
