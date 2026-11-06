#pragma once

#if defined(USE_IMGUI)
#include "material_resource.h"
#include "animation_resource.h"
#include "ik_resource.h"
#include "model_instance.h"
#include "effect_instance.h"

#include "ImGuizmo/ImCurveEdit.h"
#include "ImGuizmo/ImSequencer.h"
#include "im-neo-sequencer/imgui_neo_sequencer.h"

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

	private:
		int inner_item_id; // 内部でItemIdで使っているだけの白物
		int reset_item_id; // 内部でItemIdで使っているだけの白物
		texture_ptr blank_texture; // 空テクスチャ
		texture_ptr cross_texture; // ×テクスチャ
		model_resource_ptr reserve_model_resource; // 内部で一時的に保持しておきたい
		std::unordered_map<std::string, bool> flag_maps; // フラグ管理
		cache_string down_allow = "↓";
		cache_string right_allow = "→";
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
		struct node_ui_flags {
			bool node = false;
			bool model = false;
			bool particle = false;
			bool decal = false;
		};
		std::unordered_map<std::string, node_ui_flags> animation_ui_flag_maps; // フラグ管理
	};

	//	内部で利用するエディターアイテムのインターフェース
	class ibase_editor
	{
	public:
		ibase_editor() {}
		virtual ~ibase_editor() {}

		virtual void render(std::shared_ptr<context>& context) = 0;
	};

public:
	static void initialize();
	static void terminate();

	static std::shared_ptr<context> generate_context();
	static void begin_contexts();

public:	//	表示系
	static void render_model_bones(const model_instance& model_instance, const std::vector<std::string_view>& selected_bones);
	static void render_model_bones(const model_instance& model_instance, const std::vector<std::string>& selected_bones);
	static void render_model_bones(const model_instance& model_instance, const char** selected_bones, std::size_t selected_bone_size);

	static void render_model_select_bones(const model_instance& model_instance, const std::vector<std::string_view>& selected_bones);
	static void render_model_select_bones(const model_instance& model_instance, const std::vector<std::string>& selected_bones);
	static void render_model_select_bones(const model_instance& model_instance, const char** selected_bones, std::size_t selected_bone_size);

public:	//	編集アイテム系
	static bool edit_material(std::shared_ptr<context>& context, material_resource& material);

	//	マテリアルリソース
	static bool edit_shared_material(std::shared_ptr<context>& context, const material_resource_ptr& material);

	//	アセットブラウザからマテリアル欄にマテリアルリソースをドロップ処理
	static bool edit_material_slot(std::shared_ptr<context>& context, material_resource_ptr& material, material_resource::domains domain);
	static bool edit_texture_item(std::shared_ptr<context>& context, texture_ptr& resource, bool* selected_file = nullptr);
	//	マテリアルのテクスチャ欄にテクスチャリソースをドロップ処理
	static bool edit_material_texture_slot(std::shared_ptr<context>& context, material_resource::shader_resource::resource& slot);
	static bool edit_model_resource_item(std::shared_ptr<context>& context, model_resource_ptr& resource, std::string* selected_file_path = nullptr);

	//	アニメーション
	static bool edit_animation_data(std::shared_ptr<context>& context, animation::data& animation_data);
	static bool edit_animation_data_timeline(std::shared_ptr<context>& context, animation::data& animation_data);
	static bool edit_ik(std::shared_ptr<context>& context, model_instance_ptr& model_instance, ik_resource_ptr& ik_resource);

	//	エフェクト
	static bool edit_effect_noeds(std::shared_ptr<context>& context, effect_instance_ptr& effect_instance);
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
	std::vector<std::weak_ptr<context>> context_ptrs;
};

#endif	//	defined(USE_IMGUI)
