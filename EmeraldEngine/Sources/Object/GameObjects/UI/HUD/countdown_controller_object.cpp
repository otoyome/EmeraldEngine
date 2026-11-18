#include "object/gameobjects/ui/hud/countdown_controller_object.h"

#include "Audio/audio_manager.h"

namespace
{
	constexpr std::string_view CountdownSoundPath = "Assets/Resources/Sounds/RawFiles/minigame_countdown.wav";
	constexpr std::string_view StartSoundPath = "Assets/Resources/Sounds/RawFiles/minigame_start.wav";
	constexpr std::string_view FinishSoundPath = "Assets/Resources/Sounds/RawFiles/minigame_finish.wav";
}

countdown_controller_object::countdown_controller_object(const object_descs& object_desc)
: game_object(object_desc.name)
, text(object_desc.text)
, start_image(object_desc.start_image)
, finish_image(object_desc.finish_image)
{
}

countdown_controller_object::~countdown_controller_object()
{
}

void countdown_controller_object::set_count(int count)
{
	change_display(displays::count, count);
	if (text)
	{
		auto color = text->get_color();
		color.w = 1.0f;
		text->set_color(color);
		text->set_texts(std::to_string(count));
	}
	if (start_image)
	{
		auto color = start_image->get_color();
		color.w = 0.0f;
		start_image->set_color(color);
	}
	if (finish_image)
	{
		auto color = finish_image->get_color();
		color.w = 0.0f;
		finish_image->set_color(color);
	}
}

void countdown_controller_object::set_start()
{
	change_display(displays::start);
	if (text)
	{
		auto color = text->get_color();
		color.w = 0.0f;
		text->set_color(color);
	}
	if (start_image)
	{
		auto color = start_image->get_color();
		color.w = 1.0f;
		start_image->set_color(color);
	}
	if (finish_image)
	{
		auto color = finish_image->get_color();
		color.w = 0.0f;
		finish_image->set_color(color);
	}
}

void countdown_controller_object::set_finish()
{
	change_display(displays::finish);
	if (text)
	{
		auto color = text->get_color();
		color.w = 0.0f;
		text->set_color(color);
	}
	if (start_image)
	{
		auto color = start_image->get_color();
		color.w = 0.0f;
		start_image->set_color(color);
	}
	if (finish_image)
	{
		auto color = finish_image->get_color();
		color.w = 1.0f;
		finish_image->set_color(color);
	}
}

void countdown_controller_object::change_display(displays display, int count)
{
	if (display == current_display && count == current_count)
		return;
	current_display = display;
	current_count = count;

	if (!audio_manager::is_valid())
		return;
	switch (display)
	{
	case displays::count:
		audio_manager::instance()->play(CountdownSoundPath);
		break;
	case displays::start:
		audio_manager::instance()->play(StartSoundPath);
		break;
	case displays::finish:
		audio_manager::instance()->play(FinishSoundPath);
		break;
	default:
		break;
	}
}

void countdown_controller_object::hide()
{
	change_display(displays::none);
	if (text)
	{
		auto color = text->get_color();
		color.w = 0.0f;
		text->set_color(color);
	}
	if (start_image)
	{
		auto color = start_image->get_color();
		color.w = 0.0f;
		start_image->set_color(color);
	}
	if (finish_image)
	{
		auto color = finish_image->get_color();
		color.w = 0.0f;
		finish_image->set_color(color);
	}
}
