#pragma once

#include "Process/framework_process.h"

//	“ü—ÍƒvƒƒZƒX
class input_process : public framework_process
{
public:
	using framework_process::framework_process;

	orders get_order() const override { return orders::input; }

	bool enter() override;
	void exit() override;
	void update(float elapsed_time) override;
};
