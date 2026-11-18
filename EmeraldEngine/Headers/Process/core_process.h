#pragma once

#include "Process/framework_process.h"

//	‹¤’Ê‚Ìí’“ƒvƒƒZƒX
class core_process : public framework_process
{
public:
	using framework_process::framework_process;

	orders get_order() const override { return orders::core; }

	bool enter() override;
	void exit() override;
};
