#pragma once

#include "Process/framework_process.h"

//	サウンドプロセス
class audio_process : public framework_process
{
public:
	using framework_process::framework_process;

	orders get_order() const override { return orders::audio; }

	bool enter() override;
	void exit() override;
	void update(float elapsed_time) override;
};
