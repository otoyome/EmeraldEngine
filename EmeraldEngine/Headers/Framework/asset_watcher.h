#pragma once

//	アセットの更新を検知して読み直す
class asset_watcher
{
public:
	asset_watcher();
	~asset_watcher();
	asset_watcher(const asset_watcher&) = delete;
	asset_watcher& operator=(const asset_watcher&) = delete;

	void update();
};
