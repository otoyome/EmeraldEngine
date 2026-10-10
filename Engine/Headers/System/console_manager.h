#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#if defined(USE_IMGUI)
//	コンソールフラグ
namespace console_flags
{
	static constexpr uint32_t none = 0;
	static constexpr uint32_t persist = 1 << 0;		//	エディター設定に保存する
	static constexpr uint32_t read_only = 1 << 1;	//	コンソールからは変更できない
}

//	値と文字列の変換
namespace console_detail
{
	std::string to_string(bool value);
	std::string to_string(int value);
	std::string to_string(float value);
	std::string to_string(const std::string& value);
	bool from_string(std::string_view text, bool& value);
	bool from_string(std::string_view text, int& value);
	bool from_string(std::string_view text, float& value);
	bool from_string(std::string_view text, std::string& value);
}

//	コンソールオブジェクト
class console_object
{
public:
	console_object(std::string_view name, std::string_view help, uint32_t flags);
	virtual ~console_object();
	console_object(const console_object&) = delete;
	console_object& operator=(const console_object&) = delete;

	const std::string& get_name() const { return name; }
	const std::string& get_help() const { return help; }
	bool is_flag(uint32_t flag) const { return (flags & flag) != 0; }

	virtual bool is_variable() const = 0;

private:
	std::string name;
	std::string help;
	uint32_t flags;
};

//	コンソール変数
class console_variable_base : public console_object
{
public:
	using console_object::console_object;
	bool is_variable() const override { return true; }

	virtual std::string to_string() const = 0;
	virtual bool from_string(std::string_view text) = 0;
	virtual std::string default_to_string() const = 0;
	virtual void reset() = 0;
};

//	コンソール変数
template<class T>
class console_variable : public console_variable_base
{
public:
	console_variable(std::string_view name, const T& default_value, std::string_view help, uint32_t flags = console_flags::none)
	: console_variable_base(name, help, flags)
	, value(default_value)
	, default_value(default_value)
	{
	}

	const T& get() const { return value; }
	void set(const T& value) { this->value = value; }
	T& ref() { return value; }

	std::string to_string() const override { return console_detail::to_string(value); }
	bool from_string(std::string_view text) override { return console_detail::from_string(text, value); }
	std::string default_to_string() const override { return console_detail::to_string(default_value); }
	void reset() override { value = default_value; }

private:
	T value;
	T default_value;
};

//	コンソールコマンド
class console_command : public console_object
{
public:
	using arguments = std::vector<std::string>;
	using function = std::function<void(const arguments&)>;

public:
	console_command(std::string_view name, std::string_view help, function func, uint32_t flags = console_flags::none);
	bool is_variable() const override { return false; }
	void execute(const arguments& args) const;

private:
	function func;
};

//	コンソール管理
class console_manager
{
public:

public:
	static console_manager& instance();

	console_manager(const console_manager&) = delete;
	console_manager& operator=(const console_manager&) = delete;

public:
	//	登録/検索
	void register_object(console_object* object);
	void unregister_object(console_object* object);
	console_object* find(std::string_view name) const;
	console_variable_base* find_variable(std::string_view name) const;

	//	前方一致する要素を名前順に集める
	std::vector<const console_object*> collect(std::string_view prefix) const;

	//	1行を実行する
	//	「;」で区切って複数書ける
	void execute(std::string_view line);

	//	キー割り当て
	bool bind(std::string_view key, std::string_view command);
	bool unbind(std::string_view key);
	void clear_bindings();
	const std::map<std::string, std::string>& get_bindings() const { return bindings; }
	void update_bindings();

	void print(std::string_view text);
	const std::vector<std::string>& get_output() const { return output; }
	void clear_output() { output.clear(); }

	static std::vector<std::string> tokenize(std::string_view line);

private:
	console_manager();
	~console_manager();

	void execute_statement(std::string_view statement);

private:
	std::map<std::string, console_object*, std::less<>> objects;
	std::map<std::string, std::string> bindings;
	std::vector<std::string> output;
};

#endif	//	defined(USE_IMGUI)
