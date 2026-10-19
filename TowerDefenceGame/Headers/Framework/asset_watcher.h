#pragma once

#include <memory>

//	@tool アセットの更新を検知して読み直す（メインスレッドで毎フレーム呼ぶ）
//	@tool エディター構成でのみ動く。シェーダー・テクスチャは更新時刻を見て、モデル(.emo)・エフェクト(.eme)は Assets フォルダーの変更通知を見て読み直す
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
	std::unique_ptr<directory_watch> watch;
#endif	//	defined(USE_IMGUI)
};
