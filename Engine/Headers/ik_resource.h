#pragma once

#include <variant>

class ik_resource
{
public:
	static constexpr std::string_view default_exteision = "eik";
	static constexpr std::size_t invalid_id = -1;

public:
	enum class axis
	{
		x,
		y,
		z,

		reverse_x,
		reverse_y,
		reverse_z,
	};

	enum class constraint_mode
	{
		free,		// 拘束なし
		hinge,		// ヒンジ拘束
		ball_joint,	// 球関節拘束
		euler,		// オイラー角拘束
	};

	enum class solve_mode
	{
		limb,		//	2本IK
		ccd,		//	CCD-IK
		ccd_pmx,	//	PMX用CCD-IK
		aim,		//	指定方向へ向ける
	};

	enum class behavior_mode
	{
		foot,		//	足
		hand,		//	手
		lookat,		//	見る
	};

	struct foot_data
	{
		float ray_length = system_units::convert_cm_to_unit(30.0f);
		std::array<DirectX::XMFLOAT3, 2> ray_origin_offsets;
		float foot_offset = 0.0f;
		std::vector<size_t> adjust_node_indices;
		bool align_to_normal = true;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct hand_data
	{
		DirectX::XMFLOAT3 target_offset = {0, 0, 0}; // ターゲットからのオフセット

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct look_at_data
	{
		bool use_spine;
		bool use_eye;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

public:
	//	対応ノード情報
	struct node
	{
		std::string source_node_name; // ノード名
		constraint_mode mode = constraint_mode::free; // 拘束モード
		float limit_angles[6] = {}; // 回転制限値
		axis use_axis = axis::z; // 使用軸

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	//	計算情報
	struct solve_data
	{
		std::string name;
		solve_mode mode = solve_mode::ccd; // 計算方式
		std::vector<size_t> node_list; // 計算ノードリスト

		size_t target_node_index = invalid_id; // ターゲット位置へ到達させるボーン

		//	CCD
		UINT iteration; // ループ回数
		float constraint_angle; // ループ計算時の制限角度

		//	Limb
		bool use_pole_node_offset = false;
		DirectX::XMFLOAT3 pole_offset = {0, 0, 0};

		//	Aim
		DirectX::XMFLOAT3 aim_axis = {0, 0, 1};
		DirectX::XMFLOAT3 aim_offset = {0, 0, 0};

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	//	実行情報
	struct behavior_data
	{
		std::string name;
		behavior_mode mode = behavior_mode::foot; // 処理情報
		std::vector<std::size_t> solve_ids; // 処理するSolver情報

		std::variant<foot_data, hand_data, look_at_data> data;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

public:
	inline void set_file_path(std::string_view path) { file_path = path; }
	inline std::string_view get_file_path() const {return file_path;}
	const std::vector<node>& get_node_list() const { return nodes; }
	const std::vector<solve_data>& get_solve_data_list() const { return datas; }
	const std::vector<behavior_data>& get_behavior_data_list() const { return behavior_datas; }

#if defined(USE_IMGUI)
	std::vector<node>& get_node_list_for_editor() { return nodes; }
	std::vector<solve_data>& get_solve_data_list_for_editor() { return datas; }
	std::vector<behavior_data>& get_behavior_data_list_for_editor() { return behavior_datas; }
#endif	//	defined(USE_IMGUI)

public:
	ik_resource() {}
	~ik_resource() {}

	std::size_t search_index_by_name(std::string_view data_name);

public:
	bool serialize(std::string_view file_path);
	bool deserialize(std::string_view file_path);

private:
	std::string file_path;

	std::vector<node> nodes;
	std::vector<solve_data> datas;
	std::vector<behavior_data> behavior_datas;
};

using ik_resource_ptr = std::shared_ptr<ik_resource>;

class ik_resource_manager
{
	SINGLETON(ik_resource_manager)

private:
	ik_resource_manager();
	~ik_resource_manager();

public:
	bool load(ik_resource_ptr& resource, std::string_view file_path);
};
