#pragma once

#include "Process/framework_process.h"
#include "Framework/asset_watcher.h"

//	アセット監視プロセス
class asset_watch_process : public framework_process
{
public:
	using framework_process::framework_process;

	orders get_order() const override { return orders::asset_watch; }

	bool enter() override;
	void exit() override;
	void update(float elapsed_time) override;

private:
	asset_watcher watcher;
};
