// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef ORBITER_RUNTIME_RUNTIME_H
#define ORBITER_RUNTIME_RUNTIME_H

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace orbiter::runtime {

enum class SessionBackend {
	Bootstrap,
	OrbiterCore,
};

struct Vector3 {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct Color4 {
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float a = 1.0f;
};

struct CameraState {
	Vector3 position;
	Vector3 target;
	float fov_degrees = 40.0f;
};

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

struct MeshAsset {
	std::string id;
	LoadedMesh mesh;
};

struct MeshInstance {
	size_t asset_index = 0;
	Vector3 position;
	Vector3 rotation_degrees;
	Vector3 scale = {1.0, 1.0, 1.0};
	Color4 color;
};

struct TerrainPatch {
	MeshInstance mesh;
};

struct BaseInstance {
	std::string kind;
	MeshInstance mesh;
};

struct FrameScene {
	CameraState camera;
	std::vector<MeshAsset> assets;
	std::vector<MeshInstance> vessel_instances;
	std::vector<TerrainPatch> terrain_patches;
	std::vector<BaseInstance> base_instances;
	bool using_procedural_ground = false;
	std::string focus_name;
};

struct VesselState {
	std::string name;
	std::string class_name;
	Vector3 position;
	Vector3 velocity;
	Vector3 rotation_degrees;
	double throttle = 0.0;
	double simulation_time = 0.0;
	bool paused = false;
};

struct ControlInput {
	double thrust = 0.0;
	double pitch = 0.0;
	double roll = 0.0;
	double yaw = 0.0;
	double camera_yaw_delta = 0.0;
	double camera_pitch_delta = 0.0;
	double camera_zoom_delta = 0.0;
	bool hover_hold = false;
	bool pause_toggle = false;
};

struct LaunchOptions {
	std::filesystem::path content_root;
	std::filesystem::path scenario_path;
	std::filesystem::path user_data_root;
	std::filesystem::path planet_tex_dir;
	std::filesystem::path snapshot_path;
	int max_frames = 300;
	bool max_frames_explicit = false;
	double initial_camera_distance = 0.0; // 0 = use scenario default
	bool show_help = false;
	SessionBackend backend = SessionBackend::Bootstrap;
	std::string error;
};

class Session {
public:
	virtual ~Session() = default;

	virtual void step(double dt_seconds, const ControlInput &input) = 0;
	virtual const FrameScene &frame_scene() const = 0;
	virtual const VesselState &focus_vessel() const = 0;
	virtual const std::vector<std::string> &warnings() const = 0;
	virtual double simulation_time() const = 0;
	virtual bool paused() const = 0;

	// Reload the session with a different scenario file. Backends that
	// cannot hot-swap return false and set error.
	virtual bool load_scenario(const std::filesystem::path &scenario_path, std::string &error)
	{
		error = "Scenario switching is not supported by this session backend";
		return false;
	}
};

LaunchOptions parse_launch_options(int argc, char **argv);
std::string help_text(const char *executable_name);
std::unique_ptr<Session> create_session(const LaunchOptions &options, std::string &error);
const char *session_backend_name(SessionBackend backend);

// All .scn files under content_root/Scenarios, sorted by display name
// ("<folder> / <file>"). Empty if the directory is absent.
std::vector<std::pair<std::string, std::filesystem::path>> discover_scenarios(
	const std::filesystem::path &content_root);

namespace detail {

inline std::string trim(std::string_view value)
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

inline std::string lowercase(std::string_view value)
{
	std::string out(value);
	std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return out;
}

inline std::vector<std::string> split_orbiter_path(std::string value)
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

inline std::filesystem::path resolve_case_insensitive(
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

} // namespace detail

inline std::filesystem::path resolve_orbiter_asset_path(
	const std::filesystem::path &root,
	const std::string &orbiter_relative_path)
{
	if (orbiter_relative_path.empty()) {
		return {};
	}

	const std::vector<std::string> parts = detail::split_orbiter_path(orbiter_relative_path);
	if (parts.empty()) {
		return {};
	}

	const std::filesystem::path resolved = detail::resolve_case_insensitive(root, parts);
	if (!resolved.empty()) {
		return resolved;
	}

	std::filesystem::path direct = root;
	for (const std::string &part : parts) {
		direct /= part;
	}
	return direct;
}

} // namespace orbiter::runtime

#endif
