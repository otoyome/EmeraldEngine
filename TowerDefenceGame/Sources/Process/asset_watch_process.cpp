#include "Process/asset_watch_process.h"
#include "framework.h"

bool asset_watch_process::enter()
{
	return true;
}

void asset_watch_process::exit()
{
}

void asset_watch_process::update(float elapsed_time)
{
	watcher.update();
}
