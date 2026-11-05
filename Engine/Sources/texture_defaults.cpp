#include "texture_defaults.h"

#include "texture_factory.h"

texture_defaults::texture_defaults()
{
	white = texture_factory::create_solid_color(1, 1, 0xFFFFFFFF);
	black = texture_factory::create_solid_color(1, 1, 0xFF000000);
	missing = texture_factory::create_solid_color(1, 1, 0xFFFF00FF);
	loading = texture_factory::create_solid_color(1, 1, 0xFF808080);
}

texture_defaults::~texture_defaults()
{
}
