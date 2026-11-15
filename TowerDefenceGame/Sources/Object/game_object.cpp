#include "Object/game_object.h"
#include "Object/game_object_registry.h"
#include "Object/engine_context.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"


std::string_view game_object::get_name() const
{
	return name.view();
}

void game_object::set_name(std::string_view name)
{
	this->name = name;
}

engine_context& game_object::get_context() const
{
	assert(context && "登録前のオブジェクトからワールドを参照した");
	return *context;
}

bool game_object::is_active() const
{
	return active;
}

void game_object::set_active(bool active)
{
	if (this->active != active)
	{
		this->active = active;
		if (this->active)
			on_active();
		else
			on_deactive();
	}
}

game_object::game_object(std::string_view name)
: name(name)
, active(true)
, uuid(universally_unique_identifier::generate())
{
}

game_object::~game_object()
{
}

void game_object::serialize(cereal::JSONOutputArchive& archive) const
{
	std::string uuid_text = uuid.to_string();
	std::string name_text(name.view());
	bool active_value = active;
	archive(cereal::make_nvp("uuid", uuid_text),
		cereal::make_nvp("name", name_text),
		cereal::make_nvp("active", active_value));
}

void game_object::deserialize(cereal::JSONInputArchive& archive)
{
	std::string uuid_text;
	std::string name_text;
	bool active_value = true;
	archive(cereal::make_nvp("uuid", uuid_text),
		cereal::make_nvp("name", name_text),
		cereal::make_nvp("active", active_value));

	universally_unique_identifier value;
	if (universally_unique_identifier::from_string(uuid_text, value))
		uuid = value;
	set_name(name_text);

	//	登録前なので on_active/on_deactive は呼ばない
	active = active_value;
}

void game_object::fixed_update(float elapsed_time)
{
}

void game_object::update(float elapsed_time)
{
}

void game_object::post_update(float elapsed_time)
{
}

void game_object::pre_render()
{
}

void game_object::post_render()
{
}

#if defined(USE_IMGUI)
void game_object::imgui_render_outliner(const std::function<void()>& render_children, bool has_children, const std::function<void(game_object& child)>& on_drop)
{
	if (!display_inspector)
	{
		if (render_children)
			render_children();
		return;
	}

	ImGui::PushID(static_cast<int>(std::hash<universally_unique_identifier>()(uuid)));
	//	一時オブジェクトは名前の後ろに印を付けて薄く表示する
	if (transient)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	//	@tool 選んでいるオブジェクトは強調し、名前をクリックしたら選ぶ（開閉は矢印かダブルクリック。子が無ければ矢印を出さない）
	const bool selected = context && context->get_selected_object().get() == this;
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (selected)
		flags |= ImGuiTreeNodeFlags_Selected;
	if (!has_children)
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	const bool opened = ImGui::TreeNodeEx("##object", flags, "%s%s", name.get<utf8_cache>().data(), transient ? " (transient)" : "");
	if (context && ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		context->set_selected_object(shared_from_this());
	//	@tool 行をドラッグして別の行に落とすと、落とした先を親にする
	if (on_drop)
	{
		if (ImGui::BeginDragDropSource())
		{
			game_object* self = this;
			ImGui::SetDragDropPayload(outliner_payload_type, &self, sizeof(self));
			ImGui::TextUnformatted(name.get<utf8_cache>().data());
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(outliner_payload_type))
			{
				game_object* child = *static_cast<game_object* const*>(payload->Data);
				if (child && child != this)
					on_drop(*child);
			}
			ImGui::EndDragDropTarget();
		}
	}
	if (transient)
		ImGui::PopStyleColor();
	if (opened && has_children)
	{
		if (render_children)
			render_children();

		ImGui::TreePop();
	}
	ImGui::PopID();
}

void game_object::imgui_render_inspector()
{
	ImGui::PushID(static_cast<int>(std::hash<universally_unique_identifier>()(uuid)));
	ImGui::TextUnformatted(name.get<utf8_cache>().data());
	if (transient)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(transient)");
	}
	ImGui::Separator();

	if (ImGui::Checkbox("active", &active))
	{
		if (active)
			on_active();
		else
			on_deactive();
	}

	imgui_render_core();
	ImGui::PopID();
}

void game_object::imgui_render_core()
{
}
#endif	//	defined(USE_IMGUI)

void game_object::on_initialize()
{
}

void game_object::on_finalize()
{
}

void game_object::on_active()
{
}

void game_object::on_deactive()
{
}

GAME_OBJECT_REGISTER(game_object);
