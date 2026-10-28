#include "Audio/audio_manager.h"

#include "System/console_manager.h"
#include "System/logger.h"
#include "System/system_utility.h"

#include <algorithm>
#include <array>

namespace
{
	//	コンソール変数
	console_variable<float> master_volume_variable("audio.volume.master", 1.0f, "全体の音量（0～1）", console_flags::user_setting);
	console_variable<float> bgm_volume_variable("audio.volume.bgm", 1.0f, "BGM の音量（0～1）", console_flags::user_setting);
	console_variable<float> se_volume_variable("audio.volume.se", 1.0f, "効果音の音量（0～1）", console_flags::user_setting);
	console_variable<float> ui_volume_variable("audio.volume.ui", 1.0f, "UI の操作音の音量（0～1）", console_flags::user_setting);

	console_variable<float>& get_bus_variable(audio::bus target_bus)
	{
		switch (target_bus)
		{
		case audio::bus::bgm:	return bgm_volume_variable;
		case audio::bus::ui:	return ui_volume_variable;
		default:				return se_volume_variable;
		}
	}

	float clamp_volume(float volume)
	{
		return std::clamp(volume, 0.0f, 1.0f);
	}

	ma_uint64 to_milliseconds(float seconds)
	{
		return seconds > 0.0f ? static_cast<ma_uint64>(seconds * 1000.0f) : 0;
	}

#if	defined(USE_IMGUI)
	bool parse_bus(std::string_view text, audio::bus& target_bus)
	{
		if (text == "bgm")
			target_bus = audio::bus::bgm;
		else if (text == "se")
			target_bus = audio::bus::se;
		else if (text == "ui")
			target_bus = audio::bus::ui;
		else
			return false;
		return true;
	}

	//	動作確認用のコンソールコマンド
	console_command play_command("audio.play", "音を再生する（audio.play ファイル [bgm|se|ui]）",
		[](const console_command::arguments& args)
		{
			auto& manager = console_manager::instance();
			if (args.empty())
			{
				manager.print("usage: audio.play ファイル [bgm|se|ui]");
				return;
			}
			audio::play_parameters parameters;
			if (args.size() >= 2 && !parse_bus(args[1], parameters.target_bus))
			{
				manager.print("バスは bgm/se/ui のいずれかです: " + args[1]);
				return;
			}
			if (!audio_manager::is_valid() || !audio_manager::instance()->play(args[0], parameters).is_valid())
				manager.print("再生できません: " + args[0]);
		});

	console_command bgm_command("audio.bgm", "BGM を切り替える（audio.bgm ファイル [フェード秒]）",
		[](const console_command::arguments& args)
		{
			auto& manager = console_manager::instance();
			if (args.empty())
			{
				manager.print("usage: audio.bgm ファイル [フェード秒]");
				return;
			}
			float fade_time = 1.0f;
			if (args.size() >= 2 && !console_detail::from_string(args[1], fade_time))
			{
				manager.print("フェード秒が数値ではありません: " + args[1]);
				return;
			}
			if (audio_manager::is_valid())
				audio_manager::instance()->play_bgm(args[0], fade_time);
		});

	console_command stop_command("audio.stop", "全ての音を止める（audio.stop [フェード秒]）",
		[](const console_command::arguments& args)
		{
			float fade_time = 0.0f;
			if (!args.empty() && !console_detail::from_string(args[0], fade_time))
			{
				console_manager::instance().print("フェード秒が数値ではありません: " + args[0]);
				return;
			}
			if (audio_manager::is_valid())
			{
				audio_manager::instance()->stop_bgm(fade_time);
				audio_manager::instance()->stop_all(fade_time);
			}
		});
#endif	//	defined(USE_IMGUI)
}

audio_manager::audio_manager()
: impl(std::make_unique<backend>())
{
	ma_engine_config config = ma_engine_config_init();
	ma_result result = ma_engine_init(&config, &impl->engine);
	if (result != MA_SUCCESS)
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "Audio Engine Init Failed[%s]", ma_result_description(result));
		return;
	}

	for (size_t i = 0; i < impl->groups.size(); ++i)
	{
		result = ma_sound_group_init(&impl->engine, 0, nullptr, &impl->groups[i]);
		if (result != MA_SUCCESS)
		{
			DEBUG_WARNING_CATEGORY(logger::categorys::system, "Audio Group Init Failed[%s]", ma_result_description(result));
			for (size_t j = 0; j < i; ++j)
				ma_sound_group_uninit(&impl->groups[j]);
			ma_engine_uninit(&impl->engine);
			return;
		}
	}
	impl->initialized = true;

	voices.resize(max_voices);
	free_indices.reserve(max_voices);
	for (uint32_t index = max_voices; index > 0; --index)
		free_indices.push_back(index - 1);

	apply_volumes();

	DEBUG_LOG_CATEGORY(logger::categorys::system, "Audio Engine Initialized[%u Hz, %u ch]", ma_engine_get_sample_rate(&impl->engine), ma_engine_get_channels(&impl->engine));
}

audio_manager::~audio_manager()
{
	if (!is_available())
		return;

	std::lock_guard lock(mutex);
	commands.clear();
	for (uint32_t index = 0; index < voices.size(); ++index)
	{
		if (voices[index].allocated)
			release_voice(index);
	}
	release_cache();

	for (auto& group : impl->groups)
		ma_sound_group_uninit(&group);
	ma_engine_uninit(&impl->engine);
}

bool audio_manager::is_available() const
{
	return impl && impl->initialized;
}

void audio_manager::update(float elapsed_time)
{
	(void)elapsed_time;

	if (!is_available())
		return;

	apply_volumes();

	//	積まれた命令を積まれた順に実行する
	{
		std::lock_guard lock(mutex);
		executing_commands.swap(commands);
	}
	for (auto& item : executing_commands)
		execute_command(item);
	executing_commands.clear();

	std::lock_guard lock(mutex);

	//	再生が終わった音を片付ける
	for (uint32_t index = 0; index < voices.size(); ++index)
	{
		voice& target = voices[index];
		if (target.sound && !target.paused && !ma_sound_is_playing(target.sound.get()))
			release_voice(index);
	}

	if (bgm_handle.is_valid() && !find_voice(bgm_handle))
	{
		bgm_handle = {};
		bgm_file_path.clear();
	}
}

audio::sound_handle audio_manager::play(std::string_view file_path, const audio::play_parameters& parameters)
{
	if (!is_available() || file_path.empty())
		return {};

	std::lock_guard lock(mutex);
	return allocate_voice(file_path, parameters);
}

void audio_manager::stop(audio::sound_handle handle, float fade_out_time)
{
	std::lock_guard lock(mutex);
	push_stop(handle, fade_out_time);
}

void audio_manager::stop_bus(audio::bus target_bus, float fade_out_time)
{
	std::lock_guard lock(mutex);
	for (uint32_t index = 0; index < voices.size(); ++index)
	{
		const voice& target = voices[index];
		if (target.allocated && target.target_bus == target_bus)
			push_stop({ index, target.generation }, fade_out_time);
	}
}

void audio_manager::stop_all(float fade_out_time)
{
	std::lock_guard lock(mutex);
	for (uint32_t index = 0; index < voices.size(); ++index)
	{
		const voice& target = voices[index];
		if (target.allocated)
			push_stop({ index, target.generation }, fade_out_time);
	}
}

bool audio_manager::is_playing(audio::sound_handle handle) const
{
	std::lock_guard lock(mutex);
	const voice* target = find_voice(handle);
	return target && !target->paused;
}

void audio_manager::set_paused(audio::sound_handle handle, bool paused)
{
	std::lock_guard lock(mutex);
	if (!find_voice(handle))
		return;

	command item;
	item.type = command::types::set_paused;
	item.handle = handle;
	item.flag = paused;
	commands.push_back(std::move(item));
}

void audio_manager::set_volume(audio::sound_handle handle, float volume)
{
	std::lock_guard lock(mutex);
	if (!find_voice(handle))
		return;

	command item;
	item.type = command::types::set_volume;
	item.handle = handle;
	item.value = (std::max)(volume, 0.0f);
	commands.push_back(std::move(item));
}

void audio_manager::set_pitch(audio::sound_handle handle, float pitch)
{
	std::lock_guard lock(mutex);
	if (!find_voice(handle))
		return;

	command item;
	item.type = command::types::set_pitch;
	item.handle = handle;
	item.value = (std::max)(pitch, 0.01f);
	commands.push_back(std::move(item));
}

audio::sound_handle audio_manager::play_bgm(std::string_view file_path, float fade_time, float volume)
{
	if (!is_available())
		return {};

	std::lock_guard lock(mutex);
	if (bgm_file_path == file_path && find_voice(bgm_handle))
		return bgm_handle;

	push_stop(bgm_handle, fade_time);

	audio::play_parameters parameters;
	parameters.target_bus = audio::bus::bgm;
	parameters.loop = true;
	parameters.stream = true;
	parameters.fade_in_time = fade_time;
	parameters.volume = volume;
	bgm_handle = allocate_voice(file_path, parameters);
	bgm_file_path = bgm_handle.is_valid() ? std::string(file_path) : std::string();
	return bgm_handle;
}

void audio_manager::stop_bgm(float fade_time)
{
	std::lock_guard lock(mutex);
	push_stop(bgm_handle, fade_time);
	bgm_handle = {};
	bgm_file_path.clear();
}

float audio_manager::get_master_volume() const
{
	return clamp_volume(master_volume_variable.get());
}

void audio_manager::set_master_volume(float volume)
{
	master_volume_variable.set(clamp_volume(volume));
}

float audio_manager::get_bus_volume(audio::bus target_bus) const
{
	return clamp_volume(get_bus_variable(target_bus).get());
}

void audio_manager::set_bus_volume(audio::bus target_bus, float volume)
{
	get_bus_variable(target_bus).set(clamp_volume(volume));
}

void audio_manager::clear_cache()
{
	if (!is_available())
		return;

	std::lock_guard lock(mutex);
	command item;
	item.type = command::types::clear_cache;
	commands.push_back(std::move(item));
}

void audio_manager::preload(std::string_view file_path)
{
	if (!is_available() || file_path.empty())
		return;

	std::lock_guard lock(mutex);
	command item;
	item.type = command::types::preload;
	item.file_path = std::string(file_path);
	commands.push_back(std::move(item));
}

void audio_manager::release(std::string_view file_path)
{
	if (!is_available() || file_path.empty())
		return;

	std::lock_guard lock(mutex);
	command item;
	item.type = command::types::release;
	item.file_path = std::string(file_path);
	commands.push_back(std::move(item));
}

size_t audio_manager::get_playing_count() const
{
	std::lock_guard lock(mutex);
	return voices.size() - free_indices.size();
}

audio_manager::voice* audio_manager::find_voice(audio::sound_handle handle)
{
	if (!handle.is_valid() || handle.index >= voices.size())
		return nullptr;

	voice& target = voices[handle.index];
	return (target.allocated && target.generation == handle.generation) ? &target : nullptr;
}

const audio_manager::voice* audio_manager::find_voice(audio::sound_handle handle) const
{
	return const_cast<audio_manager*>(this)->find_voice(handle);
}

void audio_manager::release_voice(uint32_t index)
{
	voice& target = voices[index];
	if (target.sound)
	{
		ma_sound_uninit(target.sound.get());
		target.sound.reset();
	}
	target.allocated = false;
	target.paused = false;
	++target.generation;
	free_indices.push_back(index);
}

audio::sound_handle audio_manager::allocate_voice(std::string_view file_path, const audio::play_parameters& parameters)
{
	const size_t bus_index = static_cast<size_t>(parameters.target_bus);
	if (bus_index >= magic_enum::enum_count<audio::bus>())
		return {};

	if (free_indices.empty())
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "Audio Voice Limit[%s]", std::string(file_path).c_str());
		return {};
	}

	//	枠だけ先に確保してハンドルを返す。音の生成は update() で行う
	const uint32_t index = free_indices.back();
	free_indices.pop_back();

	voice& target = voices[index];
	target.allocated = true;
	target.target_bus = parameters.target_bus;
	target.paused = false;
	const audio::sound_handle handle = { index, target.generation };

	command item;
	item.type = command::types::play;
	item.handle = handle;
	item.file_path = std::string(file_path);
	item.parameters = parameters;
	commands.push_back(std::move(item));
	return handle;
}

void audio_manager::push_stop(audio::sound_handle handle, float fade_out_time)
{
	if (!find_voice(handle))
		return;

	command item;
	item.type = command::types::stop;
	item.handle = handle;
	item.value = fade_out_time;
	commands.push_back(std::move(item));
}

void audio_manager::execute_command(command& item)
{
	//	再生のみ
	if (item.type == command::types::play)
	{
		execute_play(item);
		return;
	}

	//	@tool 展開は時間がかかるので、再生と同じくロックの外で行う
	if (item.type == command::types::preload)
	{
		cache_file(item.file_path);
		return;
	}

	//	コマンド処理
	std::lock_guard lock(mutex);
	switch (item.type)
	{
		case command::types::stop:
		{
			execute_stop(item.handle, item.value);
			break;
		}
		case command::types::set_paused:
		{
			voice* target = find_voice(item.handle);
			if (!target || !target->sound || target->paused == item.flag)
				break;

			//	ma_sound_stop は再生位置を保持するので、再開は ma_sound_start で続きから鳴る
			if (item.flag)
				ma_sound_stop(target->sound.get());
			else
				ma_sound_start(target->sound.get());
			target->paused = item.flag;
			break;
		}
		case command::types::set_volume:
		{
			voice* target = find_voice(item.handle);
			if (target && target->sound)
				ma_sound_set_volume(target->sound.get(), item.value);
			break;
		}
		case command::types::set_pitch:
		{
			voice* target = find_voice(item.handle);
			if (target && target->sound)
				ma_sound_set_pitch(target->sound.get(), item.value);
			break;
		}
		case command::types::clear_cache:
		{
			release_cache();
			break;
		}
		case command::types::release:
		{
			//	@tool 再生中の音は miniaudio 側の参照で残るので、鳴り終わるまで使える
			const std::wstring path = system_utility::convert_utf16(item.file_path);
			if (cached_files.erase(path) > 0)
				ma_resource_manager_unregister_file_w(ma_engine_get_resource_manager(&impl->engine), path.c_str());
			break;
		}
		default:
			break;
	}
}

void audio_manager::execute_play(command& item)
{
	const std::string& file_path = item.file_path;
	const audio::play_parameters& parameters = item.parameters;
	const size_t bus_index = static_cast<size_t>(parameters.target_bus);
	const std::wstring path = system_utility::convert_utf16(file_path);

	//	生成に失敗したら枠を返す
	auto fail = [this, &item]()
	{
		std::lock_guard lock(mutex);
		if (find_voice(item.handle))
			release_voice(item.handle.index);
	};

	//	2D 再生
	ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
	if (parameters.stream)
	{
		flags |= MA_SOUND_FLAG_STREAM;
	}
	else
	{
		flags |= MA_SOUND_FLAG_DECODE;

		//	初回は展開してキャッシュに登録する
		if (!cache_file(file_path))
		{
			fail();
			return;
		}
	}

	auto sound = std::make_unique<ma_sound>();
	ma_result result = ma_sound_init_from_file_w(&impl->engine, path.c_str(), flags, &impl->groups[bus_index], nullptr, sound.get());
	if (result != MA_SUCCESS)
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "Audio Load Failed[%s][%s]", file_path.c_str(), ma_result_description(result));
		fail();
		return;
	}

	ma_sound_set_volume(sound.get(), (std::max)(parameters.volume, 0.0f));
	ma_sound_set_pitch(sound.get(), (std::max)(parameters.pitch, 0.01f));
	ma_sound_set_looping(sound.get(), parameters.loop ? MA_TRUE : MA_FALSE);
	if (parameters.fade_in_time > 0.0f)
		ma_sound_set_fade_in_milliseconds(sound.get(), 0.0f, 1.0f, to_milliseconds(parameters.fade_in_time));

	result = ma_sound_start(sound.get());
	if (result != MA_SUCCESS)
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "Audio Start Failed[%s][%s]", file_path.c_str(), ma_result_description(result));
		ma_sound_uninit(sound.get());
		fail();
		return;
	}

	std::lock_guard lock(mutex);
	if (voice* target = find_voice(item.handle))
		target->sound = std::move(sound);
	else
		ma_sound_uninit(sound.get());
}

bool audio_manager::cache_file(const std::string& file_path)
{
	const std::wstring path = system_utility::convert_utf16(file_path);
	if (cached_files.contains(path))
		return true;

	ma_result result = ma_resource_manager_register_file_w(ma_engine_get_resource_manager(&impl->engine), path.c_str(), MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE);
	if (result != MA_SUCCESS)
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "Audio Load Failed[%s][%s]", file_path.c_str(), ma_result_description(result));
		return false;
	}
	cached_files.insert(path);
	return true;
}

void audio_manager::execute_stop(audio::sound_handle handle, float fade_out_time)
{
	voice* target = find_voice(handle);
	if (!target)
		return;

	//	フェード無し/一時停止中はすぐに解放する。フェード有りは止まった後に update() で解放する
	if (fade_out_time <= 0.0f || target->paused || !target->sound)
	{
		release_voice(handle.index);
		return;
	}
	ma_sound_stop_with_fade_in_milliseconds(target->sound.get(), to_milliseconds(fade_out_time));
}

void audio_manager::release_cache()
{
	ma_resource_manager* resource_manager = ma_engine_get_resource_manager(&impl->engine);
	for (const auto& path : cached_files)
		ma_resource_manager_unregister_file_w(resource_manager, path.c_str());
	cached_files.clear();
}

void audio_manager::apply_volumes()
{
	ma_engine_set_volume(&impl->engine, get_master_volume());
	for (size_t i = 0; i < impl->groups.size(); ++i)
		ma_sound_group_set_volume(&impl->groups[i], get_bus_volume(static_cast<audio::bus>(i)));
}
