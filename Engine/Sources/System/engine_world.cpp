#include "System/engine_world.h"

engine_world::engine_world(std::string_view name)
: name(name)
{
	engine_world* previous = current;

	//	Œã‚©‚ç’Ç‰Á‚µ‚½‚à‚Ì‚ð
	current = this;
	instances.reserve(descriptors.size());
	for (const auto& target : descriptors)
	{
		void* instance = target.create();
		instances.emplace_back(instance);
		target.bind(instance);
	}

	if (previous)
		previous->make_current();
}

engine_world::~engine_world()
{
	engine_world* previous = current;

	//	”jŠü’†ˆ—‚Í‚±‚Ìƒ[ƒ‹ƒh‚ðŽQÆ‚Å‚«‚é‚æ‚¤‚É‚µ‚Ä‚¨‚­
	make_current();
	for (std::size_t index = instances.size(); index > 0; --index)
	{
		descriptors[index - 1].destroy(instances[index - 1]);
	}
	instances.clear();

	current = nullptr;
	if (previous && previous != this)
		previous->make_current();
}

void engine_world::make_current()
{
	current = this;
	for (std::size_t index = 0; index < instances.size(); ++index)
	{
		descriptors[index].bind(instances[index]);
	}
}
