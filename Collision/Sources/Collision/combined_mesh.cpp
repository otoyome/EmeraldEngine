#include "collision/combined_mesh.h"

#include "Collision/mesh.h"

#include <fstream>
#include <functional>

std::filesystem::path collision::combined_mesh::make_mesh_path(std::string_view model_path, size_t combined_index)
{
	std::filesystem::path path{ std::string(model_path) };
	path.replace_extension("combined" + std::to_string(combined_index) + "." + std::string(bounding_mesh::extension));
	return path;
}

std::filesystem::path collision::combined_mesh::make_info_path(std::string_view model_path)
{
	std::filesystem::path path{ std::string(model_path) };
	path.replace_extension(std::string(extension));
	return path;
}

bool collision::combined_mesh::serialize(std::string_view file_path) const
{
	std::ofstream ostream(std::string(file_path), std::ios::binary);
	if (!ostream.is_open())
		return false;

	const std::uint32_t count = static_cast<std::uint32_t>(material_indices.size());
	ostream.write(reinterpret_cast<const char*>(&count), sizeof(count));
	ostream.write(reinterpret_cast<const char*>(material_indices.data()), sizeof(std::uint32_t) * count);
	return ostream.good();
}

bool collision::combined_mesh::deserialize(std::string_view file_path)
{
	std::ifstream istream(std::string(file_path), std::ios::binary);
	if (!istream.is_open())
		return false;

	std::uint32_t count = 0;
	istream.read(reinterpret_cast<char*>(&count), sizeof(count));
	if (!istream.good())
		return false;

	material_indices.resize(count);
	if (count > 0)
		istream.read(reinterpret_cast<char*>(material_indices.data()), sizeof(std::uint32_t) * count);
	return !istream.fail();
}
