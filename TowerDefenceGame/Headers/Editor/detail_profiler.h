#pragma once

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>
#include <vector>

//	計測対象となるオブジェクトの関数
enum class object_profile_function
{
	fixed_update,
	update,
	post_update,
	pre_render,
	post_render,
};

//	測定は以下のマクロのみで行う(USE_IMGUI未定義時は何もしない)
//	DETAIL_PROFILE_CPU_SCOPE(name)									: frameカテゴリにスコープのCPU時間を記録
//	DETAIL_PROFILE_CPU_SCOPE_CATEGORY(kind, name)					: 指定カテゴリにスコープのCPU時間を記録
//	DETAIL_PROFILE_OBJECT_SCOPE(function, object)					: オブジェクト関数のCPU時間を記録(並列実行中でも可)
//	DETAIL_PROFILE_RENDER_JOB_SCOPE(owner, name)					: 描画ジョブのCPU時間を記録
//	DETAIL_PROFILE_BEGIN_FRAME() / END_FRAME()						: 集計フレームの開始/終了
//	DETAIL_PROFILE_GPU_BEGIN_FRAME() / GPU_END_FRAME()				: GPUスコープ計測フレームの開始/終了
//	DETAIL_PROFILE_GPU_SCOPE_BEGIN() / GPU_SCOPE_END(owner, name)	: GPUスコープ計測
//	DETAIL_PROFILE_SET_RENDERER_NAME(renderer)						: レンダラーへ表示名(型名)を設定

#if defined(USE_IMGUI)
#include "System/cpu_timer.h"
#include "Graphics/Debug/gpu_timer.h"
#include "System/universally_unique_identifier.h"

class game_object;

//	詳細プロファイラー
class detail_profiler
{
	SINGLETON(detail_profiler)

public:
	//	名前単位で集計する計測カテゴリ
	enum class category
	{
		frame,				//	フレーム内の主要処理
		renderer_submit,		//	レンダラーのrender()呼び出し(メインスレッドでのジョブ発行)
		renderer_job_cpu,	//	レンダラー単位の描画ジョブCPU時間
		renderer_gpu,		//	レンダラー単位のGPU時間
		render_job_cpu,		//	描画ジョブ単位のCPU時間
		gpu_scope,			//	描画ジョブ単位のGPU時間
	};

	//	履歴の保持フレーム数
	static constexpr int history_size = 120;

	//	計測値の履歴
	struct history
	{
		std::array<float, history_size> values{};
		int head = 0;
		int filled = 0;
		float current = 0.0f;
		float average = 0.0f;
		float peak = 0.0f;

		void push(float value);
	};

private:
	enum class display_value
	{
		current,
		average,
		peak,
	};

	struct frame_value
	{
		float time = 0.0f;
		UINT calls = 0;
	};

	struct flat_entry
	{
		history time;
		UINT calls = 0;
		int missing_frames = 0;
	};

	struct object_frame_value
	{
		std::string label;
		std::string type;
		std::array<float, magic_enum::enum_count<object_profile_function>()> times{};
	};

	struct object_entry
	{
		std::string label;
		std::string type;
		std::array<history, magic_enum::enum_count<object_profile_function>()> times;
		history total;
		UINT instance_count = 0;
		int missing_frames = 0;
	};

	struct object_thread_buffer
	{
		std::unordered_map<universally_unique_identifier, object_frame_value> values;
	};

	struct gpu_scope_frame
	{
		std::vector<std::unique_ptr<gpu_timer>> timers;
		std::vector<std::string> names;
		std::vector<std::string> owner_names;
		UINT used = 0;
		bool pending = false;
	};
	static constexpr UINT gpu_scope_latency = 4;

private:
	detail_profiler();
	~detail_profiler();

public:
	bool is_enabled() const { return enabled; }
	void set_enabled(bool flag) { enabled = flag; }

	//	フレームの開始/終了
	void begin_frame();
	void end_frame();

	//	名前単位の計測値を加算
	void add_time(category kind, std::string_view name, float time);

	//	オブジェクト関数の計測値を加算
	void add_object_time(object_profile_function function, const game_object* object, float time);

	//	常駐GPUスコープ計測
	void begin_gpu_frame();
	void end_gpu_frame();
	void begin_gpu_scope();
	void end_gpu_scope(std::string_view owner, std::string_view name);

	void imgui_render(bool& open);

	//	表示用の型名を取得("class "/"struct "を除去)
	static std::string get_display_type_name(const std::type_info& type);

private:
	object_thread_buffer& get_object_thread_buffer();
	void submit_gpu_times(const gpu_scope_frame& frame);

	void commit_flat(category kind);
	void commit_objects();

	float get_value(const history& data) const;

	void imgui_render_flat_table(const char* id, category kind, const char* time_label);
	void imgui_render_renderer_table();
	void imgui_render_texture_table();
	void imgui_render_object_table(const char* id, std::map<std::string, object_entry>& entries, bool type_mode);
	void imgui_render_history_tooltip(const char* label, const history& data);

private:
	bool enabled = true;
	bool paused = false;
	display_value value_mode = display_value::average;
	ImGuiTextFilter filter;

	cpu_timer frame_interval_timer;
	bool frame_interval_started = false;
	history frame_interval;

	std::mutex mutex;
	std::array<std::unordered_map<std::string, frame_value>, magic_enum::enum_count<category>()> frame_values;
	std::array<bool, magic_enum::enum_count<category>()> frame_submitted{};
	std::array<std::map<std::string, flat_entry>, magic_enum::enum_count<category>()> entries;

	std::unordered_map<universally_unique_identifier, object_frame_value> object_frame_values;
	std::map<std::string, object_entry> object_entries;
	std::map<std::string, object_entry> object_type_entries;

	//	スレッド毎のバッファ
	UINT instance_serial;
	std::vector<std::unique_ptr<object_thread_buffer>> object_thread_buffers;
	inline static std::atomic<UINT> instance_counter;

	gpu_scope_frame gpu_scope_frames[gpu_scope_latency];
	UINT gpu_scope_frame_index = 0;
	bool gpu_scope_recording = false;
};

//	CPUスコープ計測
class detail_profiler_cpu_scope
{
public:
	detail_profiler_cpu_scope(detail_profiler::category kind, std::string_view name);
	~detail_profiler_cpu_scope();

	detail_profiler_cpu_scope(const detail_profiler_cpu_scope&) = delete;
	detail_profiler_cpu_scope& operator=(const detail_profiler_cpu_scope&) = delete;

private:
	bool active;
	detail_profiler::category kind;
	std::string_view name;
	cpu_timer timer;
};

//	オブジェクト関数計測
class detail_profiler_object_scope
{
public:
	detail_profiler_object_scope(object_profile_function function, const game_object* object);
	~detail_profiler_object_scope();

	detail_profiler_object_scope(const detail_profiler_object_scope&) = delete;
	detail_profiler_object_scope& operator=(const detail_profiler_object_scope&) = delete;

private:
	bool active;
	object_profile_function function;
	const game_object* object;
	cpu_timer timer;
};

//	描画ジョブ計測
class detail_profiler_render_job_scope
{
public:
	detail_profiler_render_job_scope(std::string_view owner, std::string_view name);
	~detail_profiler_render_job_scope();

	detail_profiler_render_job_scope(const detail_profiler_render_job_scope&) = delete;
	detail_profiler_render_job_scope& operator=(const detail_profiler_render_job_scope&) = delete;

private:
	bool active;
	std::string_view owner;
	std::string_view name;
	cpu_timer timer;
};

#define	DETAIL_PROFILE_CONCAT_INNER(a, b)				a##b
#define	DETAIL_PROFILE_CONCAT(a, b)						DETAIL_PROFILE_CONCAT_INNER(a, b)
#define	DETAIL_PROFILE_UNIQUE_NAME						DETAIL_PROFILE_CONCAT(detail_profile_scope_, __LINE__)
#define	DETAIL_PROFILE_CALL(expression)					do { if (detail_profiler::is_valid()) { detail_profiler::instance()->expression; } } while (0)

#define	DETAIL_PROFILE_CPU_SCOPE(name)					detail_profiler_cpu_scope DETAIL_PROFILE_UNIQUE_NAME(detail_profiler::category::frame, name)
#define	DETAIL_PROFILE_CPU_SCOPE_CATEGORY(kind, name)	detail_profiler_cpu_scope DETAIL_PROFILE_UNIQUE_NAME(detail_profiler::category::kind, name)
#define	DETAIL_PROFILE_OBJECT_SCOPE(function, object)	detail_profiler_object_scope DETAIL_PROFILE_UNIQUE_NAME(function, object)
#define	DETAIL_PROFILE_RENDER_JOB_SCOPE(owner, name)	detail_profiler_render_job_scope DETAIL_PROFILE_UNIQUE_NAME(owner, name)
#define	DETAIL_PROFILE_BEGIN_FRAME()					DETAIL_PROFILE_CALL(begin_frame())
#define	DETAIL_PROFILE_END_FRAME()						DETAIL_PROFILE_CALL(end_frame())
#define	DETAIL_PROFILE_GPU_BEGIN_FRAME()				DETAIL_PROFILE_CALL(begin_gpu_frame())
#define	DETAIL_PROFILE_GPU_END_FRAME()					DETAIL_PROFILE_CALL(end_gpu_frame())
#define	DETAIL_PROFILE_GPU_SCOPE_BEGIN()				DETAIL_PROFILE_CALL(begin_gpu_scope())
#define	DETAIL_PROFILE_GPU_SCOPE_END(owner, name)		DETAIL_PROFILE_CALL(end_gpu_scope(owner, name))
#define	DETAIL_PROFILE_SET_RENDERER_NAME(target)		(target).set_profile_name(detail_profiler::get_display_type_name(typeid(target)))
#else	//	defined(USE_IMGUI)
#define	DETAIL_PROFILE_CPU_SCOPE(name)					((void)0)
#define	DETAIL_PROFILE_CPU_SCOPE_CATEGORY(kind, name)	((void)0)
#define	DETAIL_PROFILE_OBJECT_SCOPE(function, object)	((void)0)
#define	DETAIL_PROFILE_RENDER_JOB_SCOPE(owner, name)	((void)0)
#define	DETAIL_PROFILE_BEGIN_FRAME()					((void)0)
#define	DETAIL_PROFILE_END_FRAME()						((void)0)
#define	DETAIL_PROFILE_GPU_BEGIN_FRAME()				((void)0)
#define	DETAIL_PROFILE_GPU_END_FRAME()					((void)0)
#define	DETAIL_PROFILE_GPU_SCOPE_BEGIN()				((void)0)
#define	DETAIL_PROFILE_GPU_SCOPE_END(owner, name)		((void)0)
#define	DETAIL_PROFILE_SET_RENDERER_NAME(target)		((void)0)
#endif	//	defined(USE_IMGUI)
