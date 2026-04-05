// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef ORBITER_BOOTSTRAP_ORBITER_MESH_H
#define ORBITER_BOOTSTRAP_ORBITER_MESH_H

#include "Runtime.h"

#include <filesystem>
#include <string>

namespace orbiter::bootstrap {

using runtime::LoadedMesh;
using runtime::MeshMaterial;
using runtime::MeshSubmesh;
using runtime::MeshVertex;

bool load_orbiter_mesh(
	const std::filesystem::path &mesh_path,
	LoadedMesh &mesh,
	std::string &error);

} // namespace orbiter::bootstrap

#endif
