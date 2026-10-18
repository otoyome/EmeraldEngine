#pragma once

//	@tool アセットの更新を検知して読み直す（メインスレッドで毎フレーム呼ぶ）
//	@tool 今はエディター構成でのみ、シェーダー・テクスチャの更新時刻を見て読み直す
class asset_watcher
{
public:
	void update();
};
