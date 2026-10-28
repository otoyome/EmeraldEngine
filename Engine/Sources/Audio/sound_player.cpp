#include "Audio/sound_player.h"

#include "System/logger.h"

namespace audio
{
	//////////////////////////////////////////////////////////////////////////
	//	@tool sound_bank_handle
	//////////////////////////////////////////////////////////////////////////

	sound_bank_handle::sound_bank_handle(system_utility::hash_key key)
	: key(key)
	{
	}

	sound_bank_handle::sound_bank_handle(sound_bank_handle&& other) noexcept
	: key(std::exchange(other.key, 0))
	{
	}

	sound_bank_handle& sound_bank_handle::operator=(sound_bank_handle&& other) noexcept
	{
		if (this != &other)
		{
			release();
			key = std::exchange(other.key, 0);
		}
		return *this;
	}

	sound_bank_handle::~sound_bank_handle()
	{
		release();
	}

	void sound_bank_handle::release()
	{
		if (key != 0 && sound_player::is_valid())
			sound_player::instance()->release_bank(key);
		key = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	//	@tool sound_instance
	//////////////////////////////////////////////////////////////////////////

	sound_instance::sound_instance(sound_handle handle, std::shared_ptr<const sound_cue_desc> cue)
	: handle(handle)
	, cue(std::move(cue))
	{
	}

	sound_instance::sound_instance(sound_instance&& other) noexcept
	: handle(std::exchange(other.handle, {}))
	, cue(std::move(other.cue))
	{
	}

	sound_instance& sound_instance::operator=(sound_instance&& other) noexcept
	{
		if (this != &other)
		{
			stop();
			handle = std::exchange(other.handle, {});
			cue = std::move(other.cue);
		}
		return *this;
	}

	sound_instance::~sound_instance()
	{
		stop();
	}

	void sound_instance::stop()
	{
		if (handle.is_valid() && audio_manager::is_valid())
			audio_manager::instance()->stop(handle, cue ? cue->fade_out_time : 0.0f);
		handle = {};
	}

	bool sound_instance::is_playing() const
	{
		return handle.is_valid() && audio_manager::is_valid() && audio_manager::instance()->is_playing(handle);
	}

	//////////////////////////////////////////////////////////////////////////
	//	@tool sound_player
	//////////////////////////////////////////////////////////////////////////

	sound_player::sound_player()
	: banks()
	, cues()
	, file_references()
	, bgm_cue()
	, random(std::random_device{}())
	{
	}

	sound_player::~sound_player()
	{
		//	@tool 持ち主が残っていても、キャッシュは外しておく
		DEBUG_CHECK_WARNING(!banks.empty(), "SoundBank Remained[%zu]", banks.size());
		if (audio_manager::is_valid())
		{
			for (const auto& [file_path, count] : file_references)
				audio_manager::instance()->release(file_path);
		}
	}

	sound_bank_handle sound_player::load_bank(std::string_view file_path)
	{
		const system_utility::hash_key key = system_utility::generate_hash(file_path);
		if (auto it = banks.find(key); it != banks.end())
		{
			++it->second.references;
			return sound_bank_handle(key);
		}

		sound_bank_desc desc;
		if (!desc.load_file(file_path))
			return {};

		bank_entry& entry = banks[key];
		entry.file_path = std::string(file_path);
		entry.references = 1;
		add_cues(entry, std::move(desc));
		DEBUG_LOG_CATEGORY(logger::categorys::system, "Load SoundBank[%s][%zu cues]", entry.file_path.c_str(), entry.cues.size());
		return sound_bank_handle(key);
	}

	bool sound_player::reload_bank(std::string_view file_path)
	{
		auto it = banks.find(system_utility::generate_hash(file_path));
		if (it == banks.end())
			return false;

		sound_bank_desc desc;
		if (!desc.load_file(file_path))
			return false;

		//	@tool 新しい定義のファイルを先に数えてから古い方を外す（共通のファイルを読み直さないため）
		bank_entry& entry = it->second;
		auto old_cues = std::move(entry.cues);
		entry.cues.clear();
		remove_cues(old_cues);
		add_cues(entry, std::move(desc));
		for (const auto& cue : old_cues)
			remove_file_references(*cue);
		DEBUG_LOG_CATEGORY(logger::categorys::system, "Reload SoundBank[%s][%zu cues]", entry.file_path.c_str(), entry.cues.size());
		return true;
	}

	bool sound_player::is_bank_loaded(std::string_view file_path) const
	{
		return banks.contains(system_utility::generate_hash(file_path));
	}

	void sound_player::play(const sound_cue& cue)
	{
		if (auto found = find_cue(cue))
			start_core(found);
	}

	sound_instance sound_player::start(const sound_cue& cue)
	{
		auto found = find_cue(cue);
		if (!found)
			return {};

		const sound_handle handle = start_core(found);
		return sound_instance(handle, found);
	}

	void sound_player::stop_bgm()
	{
		if (audio_manager::is_valid())
			audio_manager::instance()->stop_bgm(bgm_cue ? bgm_cue->fade_out_time : 0.0f);
		bgm_cue.reset();
	}

	sound_instance sound_player::preview(const sound_cue_desc& cue)
	{
		auto copied = std::make_shared<const sound_cue_desc>(cue);
		const sound_handle handle = start_core(copied);
		return sound_instance(handle, copied);
	}

	void sound_player::release_bank(system_utility::hash_key key)
	{
		auto it = banks.find(key);
		if (it == banks.end() || --it->second.references > 0)
			return;

		bank_entry& entry = it->second;
		remove_cues(entry.cues);
		for (const auto& cue : entry.cues)
			remove_file_references(*cue);
		DEBUG_LOG_CATEGORY(logger::categorys::system, "Release SoundBank[%s]", entry.file_path.c_str());
		banks.erase(it);
	}

	void sound_player::add_cues(bank_entry& entry, sound_bank_desc&& desc)
	{
		for (auto& cue_desc : desc.cues)
		{
			auto cue = std::make_shared<const sound_cue_desc>(std::move(cue_desc));
			const auto [it, added] = cues.try_emplace(system_utility::generate_hash(cue->name), cue);
			DEBUG_CHECK_WARNING(!added, "Sound Cue Duplicated[%s][%s]", cue->name.c_str(), entry.file_path.c_str());
			add_file_references(*cue);
			entry.cues.push_back(std::move(cue));
		}
	}

	void sound_player::remove_cues(const std::vector<std::shared_ptr<const sound_cue_desc>>& targets)
	{
		for (const auto& cue : targets)
		{
			//	@tool 同じ名前で別のバンクが登録したものは消さない
			auto it = cues.find(system_utility::generate_hash(cue->name));
			if (it != cues.end() && it->second == cue)
				cues.erase(it);
		}
	}

	void sound_player::add_file_references(const sound_cue_desc& cue)
	{
		//	@tool BGM はストリームで鳴らすので先読みしない
		if (cue.target_bus == bus::bgm)
			return;

		for (const auto& file_path : cue.files)
		{
			if (++file_references[file_path] == 1 && audio_manager::is_valid())
				audio_manager::instance()->preload(file_path);
		}
	}

	void sound_player::remove_file_references(const sound_cue_desc& cue)
	{
		if (cue.target_bus == bus::bgm)
			return;

		for (const auto& file_path : cue.files)
		{
			auto it = file_references.find(file_path);
			if (it == file_references.end() || --it->second > 0)
				continue;
			file_references.erase(it);
			if (audio_manager::is_valid())
				audio_manager::instance()->release(file_path);
		}
	}

	std::shared_ptr<const sound_cue_desc> sound_player::find_cue(const sound_cue& cue) const
	{
		auto it = cues.find(cue.id);
		if (it == cues.end() || it->second->name != cue.name)
		{
			DEBUG_WARNING_CATEGORY(logger::categorys::system, "Sound Cue Not Found[%s]", std::string(cue.name).c_str());
			return nullptr;
		}
		return it->second;
	}

	sound_handle sound_player::start_core(const std::shared_ptr<const sound_cue_desc>& cue)
	{
		if (!audio_manager::is_valid() || cue->files.empty())
			return {};

		//	@tool 候補と揺らぎを選ぶ
		std::uniform_int_distribution<size_t> file_distribution(0, cue->files.size() - 1);
		std::uniform_real_distribution<float> volume_distribution(-cue->volume_random, cue->volume_random);
		std::uniform_real_distribution<float> pitch_distribution(-cue->pitch_random, cue->pitch_random);
		const std::string& file_path = cue->files[file_distribution(random)];
		const float volume = (std::max)(cue->volume + volume_distribution(random), 0.0f);
		const float pitch = (std::max)(cue->pitch + pitch_distribution(random), 0.01f);

		auto* manager = audio_manager::instance();
		if (cue->target_bus == bus::bgm)
		{
			bgm_cue = cue;
			return manager->play_bgm(file_path, cue->fade_in_time, volume);
		}

		play_parameters parameters;
		parameters.target_bus = cue->target_bus;
		parameters.volume = volume;
		parameters.pitch = pitch;
		parameters.loop = cue->loop;
		parameters.fade_in_time = cue->fade_in_time;
		return manager->play(file_path, parameters);
	}
}
