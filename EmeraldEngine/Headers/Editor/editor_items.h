#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <string_view>
#include "Material/material_resource.h"
#include "Animation/animation_resource.h"
#include "Animation/ik_resource.h"
#include "Model/model_instance.h"
#include "Effect/effect_instance.h"

#include "ImGuizmo/ImCurveEdit.h"
#include "ImGuizmo/ImSequencer.h"
#include "im-neo-sequencer/imgui_neo_sequencer.h"

class engine_context;

template<class T>
class console_variable;

//	エディターアイテム関係
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
class editor_items
{
public:
	//	取り扱いテクスチャの読み込み用
	static constexpr const TCHAR* texture_file_dialog_filter =
		L"テクスチャ (*.bmp, *.png, *.jpg, *.tga, *.dds, *.hdr)\0*.bmp;*.png;*.jpg;*.tga;*.dds;*.hdr\0\
		すべてのファイル (*.*)\0*.*\0";

	//	取り扱いテクスチャの読み込み用
	static constexpr const TCHAR* model_file_dialog_filter =
		L"テクスチャ (*.emo)\0*.emo\0\
		すべてのファイル (*.*)\0*.*\0";

	//	cubic_sdfファイルの読み込み用
	static constexpr const TCHAR* sdf_file_dialog_filter =
		L"cubic SDF (*.esdf)\0*.esdf\0\
		すべてのファイル (*.*)\0*.*\0";

	//	アイコンの基本サイズ
	static constexpr ImVec2 icon_size_srv = { 32, 32 };

public:
	//	エディター用コンテキスト
	class ibase_editor;
	struct context
	{
		friend class editor_items;

		int get_item_id() {return inner_item_id++;}

		void set_window_id(std::string value) { window_id = std::move(value); }

	private:
		int inner_item_id; // 内部でItemIdで使っているだけの白物
		int reset_item_id; // 内部でItemIdで使っているだけの白物
		texture_ptr blank_texture; // 空テクスチャ
		texture_resource_ptr cross_texture; // ×テクスチャ
		model_resource_ptr reserve_model_resource; // 内部で一時的に保持しておきたい
		std::unordered_map<std::string, bool> flag_maps; // フラグ管理
		cache_string down_allow = "↓";
		cache_string right_allow = "→";
		std::string window_id;	//	窓の名前に付ける ID
		char new_shader_name[128] = {}; // マテリアルシェーダー新規作成の名前

	private:	//	アニメーション関係
		std::unique_ptr<ibase_editor> interpolation_editor;
		std::unique_ptr<ibase_editor> timeline_editor;
		std::unique_ptr<ibase_editor> ik_editor;

		//	アニメーション編集時に表示したくないやつを指定するための物
		inline static const std::string EditAnimationHideName = "__EditAnimationHideName__";
		inline static const std::string EditAnimationHideRootMotion = "__EditAnimationHideRootMotion__";
		inline static const std::string EditAnimationHideIK = "__EditAnimationHideIK__";
		inline static const std::string EditAnimationHideCurve = "__EditAnimationHideCurve__";

	private:	//	編集用ウィンドウ
		//	ツールウィンドウ
		struct tool_window
		{
			std::unique_ptr<ibase_editor> editor;
			console_variable<bool>* visible = nullptr;
			bool was_visible = false;
		};
		//	キーごとに開くウィンドウ
		struct opened_window
		{
			std::string key;
			std::unique_ptr<ibase_editor> editor;
			bool open = true;
			bool focus = true;
		};
		std::vector<tool_window> tool_windows;
		std::vector<opened_window> opened_windows;
		std::string closing_window_key;	//	閉じる確認中のウィンドウ
		bool open_close_popup = false;
	};

	//	内部で利用するエディターアイテムのインターフェース
	class ibase_editor
	{
	public:
		ibase_editor() {}
		virtual ~ibase_editor() {}

		virtual void render(std::shared_ptr<context>& /*context*/) {}

	public:	//	編集用ウィンドウとして開いた時に使う
		virtual std::string get_label() const { return {}; }
		virtual void render_window(std::shared_ptr<context>& context, bool& /*open*/) { render(context); }
		virtual void update() {}
		virtual void on_closed() {}
		virtual bool is_dirty() const { return false; }
		virtual bool save() { return true; }
		virtual void on_file_moved(std::string_view /*new_file_path*/) {}
	};

	//	関数で描く編集用ウィンドウ
	class function_window : public ibase_editor
	{
	public:
		function_window(std::string label, std::function<void(bool& open)> function)
			: label(std::move(label)), function(std::move(function))
		{
		}

		std::string get_label() const override { return label; }
		void render_window(std::shared_ptr<context>& /*context*/, bool& open) override { function(open); }

	private:
		std::string label;
		std::function<void(bool& open)> function;
	};

public:
	static void initialize();
	static void terminate();

	static std::shared_ptr<context> generate_context();

	//	窓を開く
	static bool begin_window(std::shared_ptr<context>& context, std::string_view name, bool* open = nullptr, int flags = 0);
	static void begin_contexts();

public:	//	編集用ウィンドウ
	static void register_tool_window(std::shared_ptr<context>& context, std::unique_ptr<ibase_editor> editor, console_variable<bool>& visible);

	//	キーごとにウィンドウを開く
	template<class Editor, class... Args>
	static Editor* open_window(std::shared_ptr<context>& context, std::string_view key, Args&&... args)
	{
		if (ibase_editor* found = find_window(context, key))
		{
			focus_window(context, key);
			return dynamic_cast<Editor*>(found);
		}
		auto editor = std::make_unique<Editor>(key, std::forward<Args>(args)...);
		Editor* result = editor.get();
		add_window(context, key, std::move(editor));
		return result;
	}

	static ibase_editor* find_window(const std::shared_ptr<context>& context, std::string_view key);
	static void focus_window(std::shared_ptr<context>& context, std::string_view key);

	//	パスのウィンドウを保存せずに閉じる
	static void discard_windows(std::shared_ptr<context>& context, std::string_view path);
	//	パスの移動にウィンドウを合わせる
	static void move_windows(std::shared_ptr<context>& context, std::string_view old_path, std::string_view new_path);
	//	全てのウィンドウを破棄する
	static void close_all_windows(std::shared_ptr<context>& context);

	//	メニューバーと開いているウィンドウを描く（毎フレーム）
	static void render_windows(std::shared_ptr<context>& context);

private:
	static void add_window(std::shared_ptr<context>& context, std::string_view key, std::unique_ptr<ibase_editor> editor);
	static context::opened_window* find_opened_window(const std::shared_ptr<context>& context, std::string_view key);
	static void render_window_menu_bar(std::shared_ptr<context>& context);

public:
	//	エディターのメニューバー（メインのウィンドウは隠すので「editor」窓に出す。同じフレームで何度呼んでも続けて足される）
	static bool begin_editor_menu_bar();
	static void end_editor_menu_bar();
	//	「editor」窓の右上の閉じるボタンが押されたか
	static bool consume_editor_close_request();
	//	「editor」窓の中のドッキングの場所
	static void editor_dock_space(const char* default_window);

private:
	static void render_window_close_popup(std::shared_ptr<context>& context);

public:	//	表示系
	static void render_model_bones(engine_context& engine, const model_instance& model_instance, const std::vector<std::string_view>& selected_bones);
	static void render_model_bones(engine_context& engine, const model_instance& model_instance, const std::vector<std::string>& selected_bones);
	static void render_model_bones(engine_context& engine, const model_instance& model_instance, const char** selected_bones, std::size_t selected_bone_size);

	static void render_model_select_bones(engine_context& engine, const model_instance& model_instance, const std::vector<std::string_view>& selected_bones);
	static void render_model_select_bones(engine_context& engine, const model_instance& model_instance, const std::vector<std::string>& selected_bones);
	static void render_model_select_bones(engine_context& engine, const model_instance& model_instance, const char** selected_bones, std::size_t selected_bone_size);

public:	//	編集アイテム系
	static bool edit_material(std::shared_ptr<context>& context, material_resource& material);

	//	マテリアルリソース
	static bool edit_shared_material(std::shared_ptr<context>& context, const material_resource_ptr& material);

	//	アセットブラウザからマテリアル欄にマテリアルリソースをドロップ処理
	static bool edit_material_slot(std::shared_ptr<context>& context, material_resource_ptr& material, material_resource::domains domain);
	static bool edit_material_slot(std::shared_ptr<context>& context, resource::asset_handle<material_resource>& material, material_resource::domains domain);
	static bool edit_texture_item(std::shared_ptr<context>& context, resource::asset_handle<texture_resource>& resource, bool* selected_file = nullptr);
	//	マテリアルのテクスチャ欄にテクスチャリソースをドロップ処理
	static bool edit_material_texture_slot(std::shared_ptr<context>& context, material_resource::shader_resource::resource& slot);
	static bool edit_model_resource_item(std::shared_ptr<context>& context, model_resource_ptr& resource, std::string* selected_file_path = nullptr);

	//	アニメーション
	static bool edit_animation_data(std::shared_ptr<context>& context, animation::data& animation_data);
	static bool edit_animation_data_timeline(std::shared_ptr<context>& context, animation::data& animation_data);
	//	窓を作らず、今の領域にタイムラインを描く
	static bool edit_animation_data_timeline_embedded(std::shared_ptr<context>& context, animation::data& animation_data);
	//	窓を作らず、今の領域にタイムラインで選んだキーの詳細を描く
	static void edit_animation_timeline_inspector(std::shared_ptr<context>& context);
	static bool edit_ik(std::shared_ptr<context>& context, engine_context& engine, model_instance_ptr& model_instance, ik_resource_ptr& ik_resource);

	//	エフェクト
	//	ノードのツリー。選んだノードの番号を selected_node_index に入れる
	static bool edit_effect_node_tree(std::shared_ptr<context>& context, engine_context& engine, effect_instance_ptr& effect_instance, size_t& selected_node_index);
	//	選んだノードの詳細（名前・変形・モデル・パーティクル・デカール）
	static bool edit_effect_node_detail(std::shared_ptr<context>& context, engine_context& engine, effect_instance_ptr& effect_instance, size_t selected_node_index);
	static bool edit_effect_animation(std::shared_ptr<context>& context, effect_instance_ptr& effect_instance);

private:	//	内部編集用関数
	static bool edit_effect_model(std::shared_ptr<context>& context, effect_resource_ptr& effect_resource, effect_resource::model& model);
	static bool edit_effect_particle(std::shared_ptr<context>& context, effect_resource_ptr& effect_resource, effect_resource::particle& particle);
	static bool edit_effect_decal(std::shared_ptr<context>& context, effect_resource_ptr& effect_resource, effect_resource::decal& decal);

public:	//	汎用
	static bool open_file_dialog(const TCHAR* text, std::string& out_selected_file_path);
	static bool open_file_dialog_multi(const TCHAR* text, std::vector<std::string>& out_selected_file_paths);
	static bool save_file_dialog(const TCHAR* text, std::string& out_selected_file_path, const TCHAR* default_extension = nullptr);
	static bool copy_to_clipboard(const char* text);

private:
	inline static std::unique_ptr<editor_items> instance;
	inline static bool editor_window_open = true;
	std::vector<std::weak_ptr<context>> context_ptrs;
};

#endif	//	defined(USE_IMGUI)
