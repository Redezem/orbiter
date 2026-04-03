// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef ORBITER_BOOTSTRAP_ORBITER_MESH_H
#define ORBITER_BOOTSTRAP_ORBITER_MESH_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace orbiter::bootstrap {

struct MeshVertex {
	float position[3];
	float normal[3];
	float uv[2];
};

struct MeshSubmesh {
	uint32_t index_offset = 0;
	uint32_t index_count = 0;
	int texture_index = -1;
	int material_index = -1;
	bool transparent = false;
};

struct MeshMaterial {
	float diffuse[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct LoadedMesh {
	std::vector<MeshVertex> vertices;
	std::vector<uint16_t> indices;
	std::vector<MeshSubmesh> submeshes;
	std::vector<MeshMaterial> materials;
	std::vector<std::string> textures;
	std::array<float, 3> center = {0.0f, 0.0f, 0.0f};
	float radius = 1.0f;
};

bool load_orbiter_mesh(
	const std::filesystem::path &mesh_path,
	LoadedMesh &mesh,
	std::string &error);

std::filesystem::path resolve_orbiter_asset_path(
	const std::filesystem::path &root,
	const std::string &orbiter_relative_path);

} // namespace orbiter::bootstrap

#endif
