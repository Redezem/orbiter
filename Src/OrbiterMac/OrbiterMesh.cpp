// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "OrbiterMesh.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>

namespace orbiter::macos {

namespace {

struct PendingGroup {
	std::vector<MeshVertex> vertices;
	std::vector<uint16_t> indices;
	int material_index = -1;
	int texture_index = -1;
};

std::string trim(std::string_view value)
{
	size_t start = 0;
	while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
		++start;
	}

	size_t end = value.size();
	while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
		--end;
	}

	return std::string(value.substr(start, end - start));
}

std::string strip_comment(const std::string &line)
{
	const size_t comment = line.find(';');
	return trim(comment == std::string::npos ? line : line.substr(0, comment));
}

bool starts_with_ci(std::string_view value, std::string_view prefix)
{
	if (value.size() < prefix.size()) {
		return false;
	}

	for (size_t i = 0; i < prefix.size(); ++i) {
		if (std::toupper(static_cast<unsigned char>(value[i])) !=
			std::toupper(static_cast<unsigned char>(prefix[i]))) {
			return false;
		}
	}

	return true;
}

std::string lowercase(std::string_view value)
{
	std::string out(value);
	std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return out;
}

std::filesystem::path resolve_case_insensitive(
	const std::filesystem::path &root,
	const std::vector<std::string> &components)
{
	std::filesystem::path current = root;
	for (const std::string &component : components) {
		if (!std::filesystem::exists(current) || !std::filesystem::is_directory(current)) {
			return {};
		}

		const std::string wanted = lowercase(component);
		bool matched = false;
		for (const auto &entry : std::filesystem::directory_iterator(current)) {
			if (lowercase(entry.path().filename().string()) == wanted) {
				current /= entry.path().filename();
				matched = true;
				break;
			}
		}

		if (!matched) {
			return {};
		}
	}

	return current;
}

std::vector<std::string> split_orbiter_path(std::string value)
{
	std::replace(value.begin(), value.end(), '\\', '/');
	std::vector<std::string> parts;
	std::stringstream stream(value);
	std::string part;
	while (std::getline(stream, part, '/')) {
		part = trim(part);
		if (!part.empty()) {
			parts.push_back(part);
		}
	}
	return parts;
}

bool parse_group(
	std::istream &stream,
	const std::string &header_line,
	PendingGroup &group,
	std::string &carry_line,
	std::string &error)
{
	int vertex_count = 0;
	int triangle_count = 0;
	std::stringstream header(header_line.substr(4));
	if (!(header >> vertex_count >> triangle_count) || vertex_count <= 0 || triangle_count <= 0) {
		error = "Invalid GEOM declaration";
		return false;
	}

	group.vertices.resize(static_cast<size_t>(vertex_count));
	for (int i = 0; i < vertex_count; ++i) {
		std::string line;
		if (!std::getline(stream, line)) {
			error = "Unexpected end of file while reading mesh vertices";
			return false;
		}

		std::stringstream row(strip_comment(line));
		MeshVertex vertex{};
		std::vector<float> values;
		float value = 0.0f;
		while (row >> value) {
			values.push_back(value);
		}

		if (values.size() != 5 && values.size() != 6 && values.size() != 8) {
			error = "Invalid mesh vertex row";
			return false;
		}

		vertex.position[0] = values[0];
		vertex.position[1] = values[1];
		vertex.position[2] = values[2];
		if (values.size() >= 6) {
			vertex.normal[0] = values[3];
			vertex.normal[1] = values[4];
			vertex.normal[2] = values[5];
		}
		if (values.size() == 8) {
			vertex.uv[0] = values[6];
			vertex.uv[1] = values[7];
		} else if (values.size() == 5) {
			vertex.uv[0] = values[3];
			vertex.uv[1] = values[4];
		}

		group.vertices[static_cast<size_t>(i)] = vertex;
	}

	group.indices.resize(static_cast<size_t>(triangle_count) * 3u);
	for (int i = 0; i < triangle_count; ++i) {
		std::string line;
		if (!std::getline(stream, line)) {
			error = "Unexpected end of file while reading mesh indices";
			return false;
		}

		unsigned int a = 0;
		unsigned int b = 0;
		unsigned int c = 0;
		std::stringstream row(strip_comment(line));
		if (!(row >> a >> b >> c)) {
			error = "Invalid mesh index row";
			return false;
		}

		group.indices[static_cast<size_t>(i) * 3u + 0u] = static_cast<uint16_t>(a);
		group.indices[static_cast<size_t>(i) * 3u + 1u] = static_cast<uint16_t>(b);
		group.indices[static_cast<size_t>(i) * 3u + 2u] = static_cast<uint16_t>(c);
	}

	carry_line.clear();
	return true;
}

} // namespace

std::filesystem::path resolve_orbiter_asset_path(
	const std::filesystem::path &root,
	const std::string &orbiter_relative_path)
{
	if (orbiter_relative_path.empty()) {
		return {};
	}

	const std::vector<std::string> parts = split_orbiter_path(orbiter_relative_path);
	if (parts.empty()) {
		return {};
	}

	const std::filesystem::path resolved = resolve_case_insensitive(root, parts);
	if (!resolved.empty()) {
		return resolved;
	}

	std::filesystem::path direct = root;
	for (const std::string &part : parts) {
		direct /= part;
	}
	return direct;
}

bool load_orbiter_mesh(
	const std::filesystem::path &mesh_path,
	LoadedMesh &mesh,
	std::string &error)
{
	mesh = LoadedMesh{};

	std::ifstream stream(mesh_path);
	if (!stream) {
		error = "Unable to open mesh: " + mesh_path.string();
		return false;
	}

	std::string line;
	if (!std::getline(stream, line) || trim(line) != "MSHX1") {
		error = "Unsupported mesh format in " + mesh_path.string();
		return false;
	}

	int group_count = -1;
	bool static_mesh = false;
	while (std::getline(stream, line)) {
		const std::string entry = strip_comment(line);
		if (entry.empty()) {
			continue;
		}
		if (starts_with_ci(entry, "STATICMESH")) {
			static_mesh = true;
			continue;
		}
		if (starts_with_ci(entry, "GROUPS")) {
			std::stringstream row(entry.substr(6));
			row >> group_count;
			break;
		}
	}

	if (group_count <= 0) {
		error = "Mesh does not define any groups";
		return false;
	}

	std::vector<PendingGroup> groups;
	groups.reserve(static_cast<size_t>(group_count));

	for (int g = 0; g < group_count; ++g) {
		PendingGroup group;
		if (static_mesh) {
			(void)static_mesh;
		}

		while (std::getline(stream, line)) {
			const std::string entry = strip_comment(line);
			if (entry.empty()) {
				continue;
			}

			if (starts_with_ci(entry, "MATERIAL")) {
				std::stringstream row(entry.substr(8));
				int index = 0;
				if (row >> index) {
					group.material_index = index - 1;
				}
				continue;
			}

			if (starts_with_ci(entry, "TEXTURE")) {
				std::stringstream row(entry.substr(7));
				int index = 0;
				if (row >> index) {
					group.texture_index = index - 1;
				}
				continue;
			}

			if (starts_with_ci(entry, "GEOM")) {
				std::string carry_line;
				if (!parse_group(stream, entry, group, carry_line, error)) {
					return false;
				}
				break;
			}
		}

		if (group.vertices.empty() || group.indices.empty()) {
			error = "Failed to parse mesh group geometry";
			return false;
		}

		groups.push_back(std::move(group));
	}

	int material_count = 0;
	if (!std::getline(stream, line)) {
		error = "Missing material list";
		return false;
	}
	line = strip_comment(line);
	if (!starts_with_ci(line, "MATERIALS")) {
		error = "Missing MATERIALS section";
		return false;
	}
	{
		std::stringstream row(line.substr(9));
		row >> material_count;
	}

	if (material_count < 0) {
		error = "Invalid material count";
		return false;
	}

	for (int i = 0; i < material_count; ++i) {
		if (!std::getline(stream, line)) {
			error = "Unexpected end of file while reading material names";
			return false;
		}
	}

	mesh.materials.resize(static_cast<size_t>(material_count));
	for (int i = 0; i < material_count; ++i) {
		MeshMaterial material;
		if (!std::getline(stream, line) || !starts_with_ci(strip_comment(line), "MATERIAL")) {
			error = "Invalid material declaration";
			return false;
		}

		for (int row_index = 0; row_index < 4; ++row_index) {
			if (!std::getline(stream, line)) {
				error = "Unexpected end of file while reading material data";
				return false;
			}

			std::stringstream row(strip_comment(line));
			float a = 0.0f;
			float b = 0.0f;
			float c = 0.0f;
			float d = 1.0f;
			row >> a >> b >> c >> d;
			if (row_index == 0) {
				material.diffuse[0] = a;
				material.diffuse[1] = b;
				material.diffuse[2] = c;
				material.diffuse[3] = d;
			}
		}

		mesh.materials[static_cast<size_t>(i)] = material;
	}

	int texture_count = 0;
	if (!std::getline(stream, line) || !starts_with_ci(strip_comment(line), "TEXTURES")) {
		error = "Missing TEXTURES section";
		return false;
	}
	{
		std::stringstream row(strip_comment(line).substr(8));
		row >> texture_count;
	}

	mesh.textures.resize(static_cast<size_t>(std::max(texture_count, 0)));
	for (int i = 0; i < texture_count; ++i) {
		if (!std::getline(stream, line)) {
			error = "Unexpected end of file while reading texture list";
			return false;
		}
		mesh.textures[static_cast<size_t>(i)] = strip_comment(line);
	}

	float min_x = std::numeric_limits<float>::max();
	float min_y = std::numeric_limits<float>::max();
	float min_z = std::numeric_limits<float>::max();
	float max_x = std::numeric_limits<float>::lowest();
	float max_y = std::numeric_limits<float>::lowest();
	float max_z = std::numeric_limits<float>::lowest();

	for (const PendingGroup &group : groups) {
		const uint32_t vertex_offset = static_cast<uint32_t>(mesh.vertices.size());
		const uint32_t index_offset = static_cast<uint32_t>(mesh.indices.size());

		mesh.vertices.insert(mesh.vertices.end(), group.vertices.begin(), group.vertices.end());
		for (uint16_t index : group.indices) {
			mesh.indices.push_back(static_cast<uint16_t>(vertex_offset + index));
		}

		for (const MeshVertex &vertex : group.vertices) {
			min_x = std::min(min_x, vertex.position[0]);
			min_y = std::min(min_y, vertex.position[1]);
			min_z = std::min(min_z, vertex.position[2]);
			max_x = std::max(max_x, vertex.position[0]);
			max_y = std::max(max_y, vertex.position[1]);
			max_z = std::max(max_z, vertex.position[2]);
		}

		const bool transparent =
			group.material_index >= 0 &&
			group.material_index < static_cast<int>(mesh.materials.size()) &&
			mesh.materials[static_cast<size_t>(group.material_index)].diffuse[3] < 0.999f;

		mesh.submeshes.push_back({
			index_offset,
			static_cast<uint32_t>(group.indices.size()),
			group.texture_index,
			group.material_index,
			transparent,
		});
	}

	if (mesh.vertices.empty() || mesh.indices.empty()) {
		error = "Mesh contains no renderable geometry";
		return false;
	}

	mesh.center = {
		(min_x + max_x) * 0.5f,
		(min_y + max_y) * 0.5f,
		(min_z + max_z) * 0.5f,
	};

	float radius_sq = 0.0f;
	for (const MeshVertex &vertex : mesh.vertices) {
		const float dx = vertex.position[0] - mesh.center[0];
		const float dy = vertex.position[1] - mesh.center[1];
		const float dz = vertex.position[2] - mesh.center[2];
		radius_sq = std::max(radius_sq, dx * dx + dy * dy + dz * dz);
	}
	mesh.radius = std::max(1.0f, std::sqrt(radius_sq));

	return true;
}

} // namespace orbiter::macos
