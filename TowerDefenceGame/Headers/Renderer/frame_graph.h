#pragma once

#include "renderer_context.h"
#include "graphics/render_pass_begin_info.h"
#include "graphics/pooled_texture.h"

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class render_job_executor;
using render_job_function = void(render_job_context&);

//	フレームグラフ
class frame_graph
{
public:
	//	グラフで扱うテクスチャの番号
	struct texture_handle
	{
		UINT index = UINT_MAX;
		bool is_valid() const { return index != UINT_MAX; }
		bool operator==(const texture_handle& other) const { return index == other.index; }
	};

	//	テクスチャの使い方
	enum class texture_usage : uint8_t
	{
		undefined,			//	不定
		external,			//	グラフの外での使い方
		shader_resource,	//	シェーダーで読む
		render_target,		//	描画先
		depth_write,		//	深度・ステンシルの描画先
		unordered_access,	//	UAV で読み書きする
		copy_source,		//	コピー元
		copy_destination,	//	コピー先
	};

	//	パスの中での状態遷移
	struct texture_transition
	{
		UINT texture = 0;
		texture_usage before = texture_usage::undefined;
		texture_usage after = texture_usage::undefined;
		bool inside_pass = false;	//	パスの中で使い方が変わる
	};

	//	描画先
	struct color_attachment
	{
		texture_handle texture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> external_view;
		bool clear = false;
		std::array<FLOAT, 4> clear_value{ 0.0f, 0.0f, 0.0f, 0.0f };
	};

	//	描画先
	struct depth_attachment
	{
		texture_handle texture;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		UINT array_size = 0;	//	@tool 0 は配列でないビュー（1 以上は first_slice からその枚数の配列のビュー）
		UINT first_slice = 0;	//	@tool 配列のビューの先頭のスライス（シャドウマップアトラスなど）
		bool clear_depth = false;
		bool clear_stencil = false;
		FLOAT clear_depth_value = common_pipeline_states::depth_clear_value;
		UINT8 clear_stencil_value = 0;
	};

	//	パスの準備で使う宣言用のインターフェース
	class builder
	{
		friend class frame_graph;

	public:
		builder(frame_graph& graph, UINT pass_index);

		//	このフレームだけ使う一時テクスチャを要求
		texture_handle create(std::string_view name, const D3D11_TEXTURE2D_DESC& desc, DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN);

		//	読む（既定はシェーダーリソース。コピー元などは使い方を指定する）
		texture_handle read(texture_handle handle, texture_usage usage = texture_usage::shader_resource);

		//	@tool 書く（UAV・コピー先などの使い方を必ず指定する。描画先は set_render_targets で宣言する）
		texture_handle write(texture_handle handle, texture_usage usage);

		//	@tool 描画先の宣言（UE の RenderTargets に相当。パス1つにつき1組だけ）
		//	@tool グラフが execute の前に描画先の設定（とクリア）を行い、後で外す。描画先を切り替える処理はパスを分ける
		void set_render_targets(std::vector<color_attachment> colors, std::optional<depth_attachment> depth = std::nullopt);

		//	バックバッファ・GPU 側の状態などに結果を描き込むパス
		void side_effect();

		//	immediate context で実行する
		void use_immediate_context(bool wait_for_recordings = true);

	private:
		frame_graph& graph;
		UINT pass_index;
	};

	//	 実行時に参照するテクスチャの実体と描画先
	class resources
	{
		friend class frame_graph;
		struct frame_data;

	public:
		texture_ptr get_texture(texture_handle handle) const;

		//	テクスチャ全体を覆うビューポート
		D3D11_VIEWPORT get_viewport(texture_handle handle) const;

	private:
		resources(std::shared_ptr<const frame_data> frame, UINT pass_index);

	private:
		std::shared_ptr<const frame_data> frame;
		UINT pass_index;
	};

	using execute_function = std::function<void(const resources&, render_job_context&)>;

	//	プロファイラー表示用：追加するパスの所有者名
	class owner_scope
	{
	public:
		owner_scope(frame_graph& graph, std::string_view name) : graph(graph), previous(graph.owner_name) { graph.owner_name = name; }
		~owner_scope() { graph.owner_name = previous; }
		owner_scope(const owner_scope&) = delete;
		owner_scope& operator=(const owner_scope&) = delete;

	private:
		frame_graph& graph;
		std::string previous;
	};

public:
	frame_graph();
	~frame_graph();

	//	フレームの開始
	void begin_frame();

	//	外部のテクスチャをグラフに取り込む
	texture_handle import_texture(std::string_view name, const texture_ptr& texture);

	//	@tool このフレームだけ使う一時テクスチャを作る（複数のパスで使う物をパスの外で作る。UE の GraphBuilder.CreateTexture）
	//	@tool srv_format はシェーダーリソースビューの形式（UNKNOWN はテクスチャと同じ。深度など型の無い形式で指定する）
	texture_handle create_texture(std::string_view name, const D3D11_TEXTURE2D_DESC& desc, DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN);

	//	@tool 前のフレームで抜き出したテクスチャを取り込む（UE の RegisterExternalTexture）
	//	@tool 無い・中身が入っていない・形式が expected_desc と違う場合は無効な番号を返す
	texture_handle register_external_texture(std::string_view name, const pooled_texture_ref_ptr& texture, const D3D11_TEXTURE2D_DESC& expected_desc);

	//	@tool 実行後にテクスチャの実体を受け取る予約（UE の QueueTextureExtraction）
	//	@tool 戻り値の中身は execute() で入る。戻り値を持っている間は、プールが別の用途に使い回さない
	pooled_texture_ref_ptr queue_extraction(texture_handle handle);

	//	パスを追加
	template<class Data, class Setup, class Execute>
	const Data& add_pass(std::string_view name, const renderer_context& context, Setup&& setup, Execute&& execute)
	{
		auto data = std::make_shared<Data>();
		const UINT pass_index = begin_pass(name, context);
		builder pass_builder(*this, pass_index);
		setup(pass_builder, *data);
		end_pass(pass_index, [data, execute = std::forward<Execute>(execute)](const resources& pass_resources, render_job_context& graphics)
		{
			execute(static_cast<const Data&>(*data), pass_resources, graphics);
		});
		return *data;
	}

	//	使われないパスを除き、一時テクスチャと描画先を用意してから、追加した順に実行者へ渡す
	void execute(render_job_executor& executor);

#if defined(USE_IMGUI)
	//	@tool 直前に実行したフレームのパス一覧を表示する
	void imgui_render() const;
#endif	//	defined(USE_IMGUI)

	//	@tool 直前に実行したフレームのパス一覧と一時テクスチャの割り当てをファイルに書く（自動キャプチャの確認用）
	void write_report(const std::string& file_path) const;

private:
	//	@tool テクスチャ（取り込み・一時）
	struct texture_entry
	{
		std::string name;
		texture_ptr texture;
		D3D11_TEXTURE2D_DESC desc{};
		DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN;	//	@tool 一時テクスチャのシェーダーリソースビューの形式
		bool transient = false;
		int pool_index = -1;			//	@tool 割り当てた一時テクスチャのプールの番号
		UINT version = 0;				//	@tool 追加中の最新版
		pooled_texture_ref_ptr external;	//	@tool 取り込んだ抜き出し済みのテクスチャ（このフレームの間はプールが使い回さないように持つ）
		pooled_texture_ref_ptr extraction;	//	@tool 実行後に実体を渡す予約
	};

	//	@tool テクスチャの版（書き込みで作られ、読まれた数を数える）
	struct version_node
	{
		UINT texture = 0;
		UINT version = 0;
		int producer = -1;				//	@tool この版を書いたパス（-1 は取り込んだ時点の内容）
		UINT read_count = 0;
	};

	//	@tool パスの描画先の宣言
	struct render_targets_desc
	{
		std::vector<color_attachment> colors;
		std::optional<depth_attachment> depth;
	};

	struct pass
	{
		std::string name;
		std::string owner_name;
		renderer_context context;
		execute_function execute;
		bool use_immediate_context = false;
		bool wait_for_recordings = true;
		bool side_effect = false;
		bool culled = false;
		std::vector<UINT> reads;
		std::vector<UINT> writes;
		std::optional<render_targets_desc> render_targets;
		std::vector<std::pair<UINT, texture_usage>> accesses;
		std::vector<texture_transition> transitions;
	};

	UINT begin_pass(std::string_view name, const renderer_context& context);
	void end_pass(UINT pass_index, execute_function execute);
	UINT read_texture(UINT pass_index, texture_handle handle);
	UINT write_texture(UINT pass_index, texture_handle handle);
	void record_access(UINT pass_index, texture_handle handle, texture_usage usage);

	//	@tool 結果が使われないパスを除く
	void cull_passes();
	//	@tool 除かなかったパスを順にたどり、テクスチャごとの使い方の変化（状態遷移）を求める
	void build_transitions();
	//	@tool 一時テクスチャをプールから割り当てる（使用期間が重ならない物同士は同じ実体を使う）
	void allocate_transient_textures();
	//	@tool 描画先のビューを用意する（取り込んだテクスチャのビューは作り置きを使う）
	void build_render_targets();

	//	@tool 描画先ビューの作り置き（テクスチャとビューの形式ごと）
	ID3D11RenderTargetView* acquire_render_target_view(const texture_ptr& texture);
	ID3D11DepthStencilView* acquire_depth_stencil_view(const texture_ptr& texture, DXGI_FORMAT format, UINT first_slice, UINT array_size);

private:
	std::vector<texture_entry> textures;
	std::vector<version_node> nodes;
	std::vector<pass> passes;
	std::shared_ptr<resources::frame_data> frame;
	std::string owner_name = "framework";

	//	@tool 一時テクスチャのプール（描画スレッドでのみ使う）
	struct pooled_texture
	{
		D3D11_TEXTURE2D_DESC desc{};
		DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN;
		texture_ptr texture;
		std::uint64_t last_used_frame = 0;
		std::weak_ptr<const pooled_texture_ref> owner;	//	@tool 抜き出した先（生きている間は使い回さない・破棄しない）
	};
	std::vector<pooled_texture> texture_pool;

	//	@tool 描画先ビューの作り置き（描画スレッドでのみ使う。テクスチャを保持して、使われなくなったら破棄する）
	struct cached_view
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil_view;
		std::uint64_t last_used_frame = 0;
	};
	struct view_key
	{
		ID3D11Resource* resource = nullptr;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;	//	@tool UNKNOWN は描画先（色）
		UINT array_size = 0;						//	@tool 深度の配列のビューの枚数（0 は配列でない）
		UINT first_slice = 0;						//	@tool 深度の配列のビューの先頭のスライス
		bool operator==(const view_key& other) const { return resource == other.resource && format == other.format && array_size == other.array_size && first_slice == other.first_slice; }
	};
	struct view_key_hash
	{
		std::size_t operator()(const view_key& key) const { return std::hash<const void*>()(key.resource) ^ (std::hash<int>()(static_cast<int>(key.format)) << 1) ^ (std::hash<UINT>()(key.array_size) << 2) ^ (std::hash<UINT>()(key.first_slice) << 3); }
	};
	std::unordered_map<view_key, cached_view, view_key_hash> view_cache;

	std::uint64_t frame_count = 0;

	//	@tool 表示用（直前に実行したフレームの情報。表示はメインスレッド、実行は描画スレッドなので排他する）
	struct report_pass
	{
		std::string name;
		std::string owner_name;
		bool immediate = false;
		bool culled = false;
		std::vector<std::string> transitions;	//	@tool 表示用の状態遷移（「テクスチャ名: 前 -> 後」）
	};
	std::vector<report_pass> last_passes;
	std::vector<std::pair<std::string, int>> last_transient_textures;
	UINT last_pool_size = 0;
	mutable std::mutex report_mutex;
};

//	@tool 実行時に参照するフレームの情報（パスの記録処理が共有して持つ）
struct frame_graph::resources::frame_data
{
	struct texture_data
	{
		texture_ptr texture;
		D3D11_TEXTURE2D_DESC desc{};
	};
	std::vector<texture_data> textures;
	std::vector<std::optional<render_pass_begin_info>> render_targets;	//	@tool パスごとの描画先（グラフが execute の前後で開始・終了する。無いパスは nullopt）
};
