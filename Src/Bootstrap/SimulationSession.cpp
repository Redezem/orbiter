// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "SimulationSession.h"

#include "TerrainArchive.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>

namespace orbiter::bootstrap {

using runtime::BaseInstance;
using runtime::Color4;
using runtime::ControlInput;
using runtime::FrameScene;
using runtime::LaunchOptions;
using runtime::LoadedMesh;
using runtime::MeshAsset;
using runtime::MeshInstance;
using runtime::MeshMaterial;
using runtime::MeshSubmesh;
using runtime::MeshVertex;
using runtime::TerrainPatch;
using runtime::Vector3;
using runtime::VesselState;

namespace {

constexpr double kEarthRadius = 6378137.0;
constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;
constexpr double kGravity = 9.80665;
constexpr double kQuadcopterMassKg = 2.0;
constexpr double kQuadcopterMaxThrustNewton = 30.0;
constexpr double kMaxTiltRadians = 25.0 * kDegreesToRadians;
constexpr double kMaxYawRate = 1.4;
constexpr double kGroundPlaneExtent = 8000.0;
constexpr int kTerrainPatchLevel = 11;
constexpr int kTerrainPatchesPerSide = 3;
constexpr int kTerrainSamplesPerSide = 256; // 256^2 vertices fits uint16 index range

struct ScenarioShip {
	std::string name;
	std::string class_name;
	double latitude_degrees = 0.0;
	double longitude_degrees = 0.0;
	double altitude_meters = 0.0;
	double heading_degrees = 0.0;
};

struct ScenarioData {
	ScenarioShip ship;
	std::string focus_name;
	Vector3 camera_offset;
	float camera_fov_degrees = 40.0f;
};

struct BaseLocation {
	double latitude_degrees = 0.0;
	double longitude_degrees = 0.0;
};

struct ObjectConfig {
	std::string type;
	std::map<std::string, std::string> values;
};

std::string trim(const std::string &value)
{
	size_t start = 0;
	while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
		++start;
	}

	size_t end = value.size();
	while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
		--end;
	}

	return value.substr(start, end - start);
}

std::string uppercase(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::toupper(ch));
	});
	return value;
}

bool starts_with_ci(const std::string &value, const std::string &prefix)
{
	if (value.size() < prefix.size()) {
		return false;
	}
	return uppercase(value.substr(0, prefix.size())) == uppercase(prefix);
}

std::string strip_comment(const std::string &line)
{
	const size_t comment = line.find(';');
	return trim(comment == std::string::npos ? line : line.substr(0, comment));
}

Vector3 make_vector(double x, double y, double z)
{
	return {x, y, z};
}

Vector3 operator+(const Vector3 &a, const Vector3 &b)
{
	return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 operator-(const Vector3 &a, const Vector3 &b)
{
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 operator*(const Vector3 &value, double scalar)
{
	return {value.x * scalar, value.y * scalar, value.z * scalar};
}

Vector3 operator/(const Vector3 &value, double scalar)
{
	return {value.x / scalar, value.y / scalar, value.z / scalar};
}

double dot(const Vector3 &a, const Vector3 &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

double length(const Vector3 &value)
{
	return std::sqrt(dot(value, value));
}

Vector3 normalize(const Vector3 &value)
{
	const double len = length(value);
	if (len <= 1e-9) {
		return {};
	}
	return value / len;
}

double clamp_angle(double angle)
{
	const double two_pi = 2.0 * 3.14159265358979323846;
	while (angle > 3.14159265358979323846) {
		angle -= two_pi;
	}
	while (angle < -3.14159265358979323846) {
		angle += two_pi;
	}
	return angle;
}

Vector3 rotate_euler_yxz(const Vector3 &value, const Vector3 &rotation_degrees)
{
	const double yaw = rotation_degrees.y * kDegreesToRadians;
	const double pitch = rotation_degrees.x * kDegreesToRadians;
	const double roll = rotation_degrees.z * kDegreesToRadians;

	const double cy = std::cos(yaw);
	const double sy = std::sin(yaw);
	const double cx = std::cos(pitch);
	const double sx = std::sin(pitch);
	const double cz = std::cos(roll);
	const double sz = std::sin(roll);

	Vector3 v = value;
	v = {cy * v.x + sy * v.z, v.y, -sy * v.x + cy * v.z};
	v = {v.x, cx * v.y - sx * v.z, sx * v.y + cx * v.z};
	v = {cz * v.x - sz * v.y, sz * v.x + cz * v.y, v.z};
	return v;
}

Vector3 body_up_vector(const Vector3 &rotation_degrees)
{
	return normalize(rotate_euler_yxz({0.0, 1.0, 0.0}, rotation_degrees));
}

Vector3 local_position_from_lon_lat(
	double origin_latitude_degrees,
	double origin_longitude_degrees,
	double latitude_degrees,
	double longitude_degrees,
	double altitude_meters)
{
	const double origin_latitude_radians = origin_latitude_degrees * kDegreesToRadians;
	const double latitude_radians = latitude_degrees * kDegreesToRadians;
	const double longitude_radians = longitude_degrees * kDegreesToRadians;
	const double origin_longitude_radians = origin_longitude_degrees * kDegreesToRadians;

	const double east =
		(longitude_radians - origin_longitude_radians) * std::cos(origin_latitude_radians) * kEarthRadius;
	const double north = (latitude_radians - origin_latitude_radians) * kEarthRadius;
	return {east, altitude_meters, north};
}

bool parse_vector3(const std::string &value, Vector3 &out)
{
	std::stringstream stream(value);
	return static_cast<bool>(stream >> out.x >> out.y >> out.z);
}

double parse_scalar(const std::map<std::string, std::string> &values, const std::string &key, double fallback)
{
	const auto it = values.find(key);
	if (it == values.end()) {
		return fallback;
	}
	std::stringstream stream(it->second);
	double result = fallback;
	stream >> result;
	return result;
}

bool load_lines(const std::filesystem::path &path, std::vector<std::string> &lines, std::string &error)
{
	std::ifstream stream(path);
	if (!stream) {
		error = "Unable to open " + path.string();
		return false;
	}

	std::string line;
	while (std::getline(stream, line)) {
		lines.push_back(line);
	}
	return true;
}

bool parse_scenario(const std::filesystem::path &path, ScenarioData &scenario, std::string &error)
{
	std::vector<std::string> lines;
	if (!load_lines(path, lines, error)) {
		return false;
	}

	bool in_focus = false;
	bool in_camera = false;
	bool in_ships = false;
	bool reading_ship = false;
	for (const std::string &raw_line : lines) {
		const std::string line = strip_comment(raw_line);
		if (line.empty()) {
			continue;
		}

		if (line == "BEGIN_FOCUS") {
			in_focus = true;
			continue;
		}
		if (line == "END_FOCUS") {
			in_focus = false;
			continue;
		}
		if (line == "BEGIN_CAMERA") {
			in_camera = true;
			continue;
		}
		if (line == "END_CAMERA") {
			in_camera = false;
			continue;
		}
		if (line == "BEGIN_SHIPS") {
			in_ships = true;
			continue;
		}
		if (line == "END_SHIPS") {
			in_ships = false;
			reading_ship = false;
			continue;
		}

		if (in_focus && starts_with_ci(line, "Ship ")) {
			scenario.focus_name = trim(line.substr(5));
			continue;
		}

		if (in_camera) {
			if (starts_with_ci(line, "POS ")) {
				parse_vector3(trim(line.substr(4)), scenario.camera_offset);
			} else if (starts_with_ci(line, "FOV ")) {
				std::stringstream stream(line.substr(4));
				stream >> scenario.camera_fov_degrees;
			}
			continue;
		}

		if (!in_ships) {
			continue;
		}

		if (!reading_ship && line.find(':') != std::string::npos) {
			const size_t separator = line.find(':');
			scenario.ship.name = trim(line.substr(0, separator));
			scenario.ship.class_name = trim(line.substr(separator + 1));
			reading_ship = true;
			continue;
		}

		if (reading_ship && line == "END") {
			reading_ship = false;
			continue;
		}

		if (!reading_ship) {
			continue;
		}

		if (starts_with_ci(line, "POS ")) {
			std::stringstream stream(line.substr(4));
			stream >> scenario.ship.longitude_degrees >> scenario.ship.latitude_degrees;
		} else if (starts_with_ci(line, "ALT ")) {
			std::stringstream stream(line.substr(4));
			stream >> scenario.ship.altitude_meters;
		} else if (starts_with_ci(line, "HEADING ")) {
			std::stringstream stream(line.substr(8));
			stream >> scenario.ship.heading_degrees;
		}
	}

	if (scenario.ship.name.empty() || scenario.ship.class_name.empty()) {
		error = "Scenario does not define a supported ship";
		return false;
	}

	if (!scenario.focus_name.empty() && scenario.focus_name != scenario.ship.name) {
		error = "Scenario focus ship is not available in the native bootstrap session";
		return false;
	}

	return true;
}

bool parse_base_config(
	const std::filesystem::path &path,
	BaseLocation &location,
	std::vector<ObjectConfig> &objects,
	std::string &error)
{
	std::vector<std::string> lines;
	if (!load_lines(path, lines, error)) {
		return false;
	}

	bool in_object_list = false;
	bool reading_object = false;
	ObjectConfig current;
	for (const std::string &raw_line : lines) {
		const std::string line = strip_comment(raw_line);
		if (line.empty()) {
			continue;
		}

		if (starts_with_ci(line, "Location =")) {
			std::stringstream stream(line.substr(line.find('=') + 1));
			stream >> location.longitude_degrees >> location.latitude_degrees;
			continue;
		}

		if (line == "BEGIN_OBJECTLIST") {
			in_object_list = true;
			continue;
		}
		if (line == "END_OBJECTLIST") {
			in_object_list = false;
			reading_object = false;
			continue;
		}
		if (!in_object_list) {
			continue;
		}

		if (!reading_object) {
			current = {};
			current.type = uppercase(line);
			reading_object = true;
			continue;
		}

		if (line == "END") {
			objects.push_back(current);
			reading_object = false;
			continue;
		}

		const size_t split = line.find_first_of(" \t");
		const std::string key = uppercase(split == std::string::npos ? line : line.substr(0, split));
		const std::string value = trim(split == std::string::npos ? "" : line.substr(split + 1));
		current.values[key] = value;
	}

	return !objects.empty();
}

LoadedMesh make_mesh_from_triangles(
	const std::vector<MeshVertex> &vertices,
	const std::vector<uint16_t> &indices,
	const std::vector<std::string> &textures)
{
	LoadedMesh mesh;
	mesh.vertices = vertices;
	mesh.indices = indices;
	mesh.textures = textures;
	mesh.materials.push_back({});
	mesh.submeshes.push_back({0, static_cast<uint32_t>(indices.size()), textures.empty() ? -1 : 0, 0, false});

	float min_x = std::numeric_limits<float>::max();
	float min_y = std::numeric_limits<float>::max();
	float min_z = std::numeric_limits<float>::max();
	float max_x = std::numeric_limits<float>::lowest();
	float max_y = std::numeric_limits<float>::lowest();
	float max_z = std::numeric_limits<float>::lowest();
	for (const MeshVertex &vertex : vertices) {
		min_x = std::min(min_x, vertex.position[0]);
		min_y = std::min(min_y, vertex.position[1]);
		min_z = std::min(min_z, vertex.position[2]);
		max_x = std::max(max_x, vertex.position[0]);
		max_y = std::max(max_y, vertex.position[1]);
		max_z = std::max(max_z, vertex.position[2]);
	}

	mesh.center = {
		(min_x + max_x) * 0.5f,
		(min_y + max_y) * 0.5f,
		(min_z + max_z) * 0.5f,
	};
	float radius_sq = 0.0f;
	for (const MeshVertex &vertex : vertices) {
		const float dx = vertex.position[0] - mesh.center[0];
		const float dy = vertex.position[1] - mesh.center[1];
		const float dz = vertex.position[2] - mesh.center[2];
		radius_sq = std::max(radius_sq, dx * dx + dy * dy + dz * dz);
	}
	mesh.radius = std::sqrt(radius_sq);
	return mesh;
}

LoadedMesh make_unit_box_mesh()
{
	std::vector<MeshVertex> vertices = {
		{{-0.5f, -0.5f, 0.5f}, {0, 0, 1}, {0, 1}},
		{{0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1, 1}},
		{{0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1, 0}},
		{{-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {0, 0}},
		{{0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 1}},
		{{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 1}},
		{{-0.5f, 0.5f, -0.5f}, {0, 0, -1}, {1, 0}},
		{{0.5f, 0.5f, -0.5f}, {0, 0, -1}, {0, 0}},
		{{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 1}},
		{{-0.5f, -0.5f, 0.5f}, {-1, 0, 0}, {1, 1}},
		{{-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {1, 0}},
		{{-0.5f, 0.5f, -0.5f}, {-1, 0, 0}, {0, 0}},
		{{0.5f, -0.5f, 0.5f}, {1, 0, 0}, {0, 1}},
		{{0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 1}},
		{{0.5f, 0.5f, -0.5f}, {1, 0, 0}, {1, 0}},
		{{0.5f, 0.5f, 0.5f}, {1, 0, 0}, {0, 0}},
		{{-0.5f, 0.5f, 0.5f}, {0, 1, 0}, {0, 1}},
		{{0.5f, 0.5f, 0.5f}, {0, 1, 0}, {1, 1}},
		{{0.5f, 0.5f, -0.5f}, {0, 1, 0}, {1, 0}},
		{{-0.5f, 0.5f, -0.5f}, {0, 1, 0}, {0, 0}},
		{{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 1}},
		{{0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 1}},
		{{0.5f, -0.5f, 0.5f}, {0, -1, 0}, {1, 0}},
		{{-0.5f, -0.5f, 0.5f}, {0, -1, 0}, {0, 0}},
	};
	std::vector<uint16_t> indices = {
		0, 1, 2, 0, 2, 3,
		4, 5, 6, 4, 6, 7,
		8, 9, 10, 8, 10, 11,
		12, 13, 14, 12, 14, 15,
		16, 17, 18, 16, 18, 19,
		20, 21, 22, 20, 22, 23,
	};
	return make_mesh_from_triangles(vertices, indices, {});
}

LoadedMesh make_unit_plane_mesh(const std::string &texture_name)
{
	std::vector<MeshVertex> vertices = {
		{{-0.5f, 0.0f, -0.5f}, {0, 1, 0}, {0, 1}},
		{{0.5f, 0.0f, -0.5f}, {0, 1, 0}, {1, 1}},
		{{0.5f, 0.0f, 0.5f}, {0, 1, 0}, {1, 0}},
		{{-0.5f, 0.0f, 0.5f}, {0, 1, 0}, {0, 0}},
	};
	std::vector<uint16_t> indices = {0, 3, 2, 0, 2, 1};
	return make_mesh_from_triangles(vertices, indices, texture_name.empty() ? std::vector<std::string>{} : std::vector<std::string>{texture_name});
}

// Stitch a rows x cols grid of decoded surface tiles into one RGBA image.
// Tiles are ordered north-to-south, west-to-east; each tile's uv [0,1] maps
// linearly into the stitched image.
bool stitch_surface_tiles(
	const std::vector<TerrainSurfaceTile> &tiles,
	int tiles_per_side,
	std::vector<uint8_t> &out_rgba,
	int &out_width,
	int &out_height,
	std::string &error)
{
	if (tiles_per_side <= 0 || tiles.size() != static_cast<size_t>(tiles_per_side) * tiles_per_side) {
		error = "Surface tile grid incomplete";
		return false;
	}
	int tile_w = 0, tile_h = 0;
	for (const TerrainSurfaceTile &tile : tiles) {
		if (tile.width <= 0 || tile.height <= 0) {
			error = "Surface tile has no image data";
			return false;
		}
		if (tile_w == 0) {
			tile_w = tile.width;
			tile_h = tile.height;
		} else if (tile.width != tile_w || tile.height != tile_h) {
			error = "Surface tiles have mixed sizes";
			return false;
		}
	}
	out_width = tile_w * tiles_per_side;
	out_height = tile_h * tiles_per_side;
	out_rgba.assign(static_cast<size_t>(out_width) * out_height * 4, 0);
	for (int ty = 0; ty < tiles_per_side; ++ty) {
		for (int tx = 0; tx < tiles_per_side; ++tx) {
			const TerrainSurfaceTile &tile = tiles[static_cast<size_t>(ty) * tiles_per_side + tx];
			for (int y = 0; y < tile_h; ++y) {
				std::memcpy(
					out_rgba.data() + (static_cast<size_t>(ty * tile_h + y) * out_width + tx * tile_w) * 4,
					tile.rgba.data() + static_cast<size_t>(y) * tile_w * 4,
					static_cast<size_t>(tile_w) * 4);
			}
		}
	}
	return true;
}

LoadedMesh make_unit_cylinder_mesh(int steps)
{
	std::vector<MeshVertex> vertices;
	std::vector<uint16_t> indices;
	steps = std::max(steps, 8);
	vertices.reserve(static_cast<size_t>(steps) * 4u + 2u);

	for (int i = 0; i < steps; ++i) {
		const double angle = static_cast<double>(i) / static_cast<double>(steps) * 2.0 * 3.14159265358979323846;
		const float x = static_cast<float>(std::cos(angle) * 0.5);
		const float z = static_cast<float>(std::sin(angle) * 0.5);
		const float u = static_cast<float>(i) / static_cast<float>(steps);
		vertices.push_back({{x, -0.5f, z}, {x * 2.0f, 0, z * 2.0f}, {u, 1}});
		vertices.push_back({{x, 0.5f, z}, {x * 2.0f, 0, z * 2.0f}, {u, 0}});
	}

	for (int i = 0; i < steps; ++i) {
		const int next = (i + 1) % steps;
		const uint16_t bottom0 = static_cast<uint16_t>(i * 2);
		const uint16_t top0 = static_cast<uint16_t>(i * 2 + 1);
		const uint16_t bottom1 = static_cast<uint16_t>(next * 2);
		const uint16_t top1 = static_cast<uint16_t>(next * 2 + 1);
		indices.insert(indices.end(), {bottom0, bottom1, top0, top0, bottom1, top1});
	}

	const uint16_t bottom_center = static_cast<uint16_t>(vertices.size());
	vertices.push_back({{0.0f, -0.5f, 0.0f}, {0, -1, 0}, {0.5f, 0.5f}});
	const uint16_t top_center = static_cast<uint16_t>(vertices.size());
	vertices.push_back({{0.0f, 0.5f, 0.0f}, {0, 1, 0}, {0.5f, 0.5f}});

	for (int i = 0; i < steps; ++i) {
		const int next = (i + 1) % steps;
		const uint16_t bottom0 = static_cast<uint16_t>(i * 2);
		const uint16_t bottom1 = static_cast<uint16_t>(next * 2);
		const uint16_t top0 = static_cast<uint16_t>(i * 2 + 1);
		const uint16_t top1 = static_cast<uint16_t>(next * 2 + 1);
		indices.insert(indices.end(), {bottom_center, bottom0, bottom1});
		indices.insert(indices.end(), {top_center, top1, top0});
	}

	return make_mesh_from_triangles(vertices, indices, {});
}

Color4 color_for_type(const std::string &type)
{
	if (type == "BLOCK") {
		return {0.72f, 0.74f, 0.76f, 1.0f};
	}
	if (type == "HANGAR" || type == "HANGAR2" || type == "HANGAR3") {
		return {0.70f, 0.72f, 0.69f, 1.0f};
	}
	if (type == "TANK") {
		return {0.82f, 0.84f, 0.87f, 1.0f};
	}
	if (type == "LPAD2A") {
		return {0.90f, 0.90f, 0.90f, 1.0f};
	}
	if (type == "RUNWAY") {
		return {1.0f, 1.0f, 1.0f, 1.0f};
	}
	return {1.0f, 1.0f, 1.0f, 1.0f};
}

bool parse_runtime_vector(const std::map<std::string, std::string> &values, const std::string &key, Vector3 &out)
{
	const auto it = values.find(key);
	if (it == values.end()) {
		return false;
	}
	return parse_vector3(it->second, out);
}

bool register_mesh_asset(
	const std::filesystem::path &content_root,
	const std::string &key,
	const std::string &mesh_name,
	const std::string &texture_override,
	std::unordered_map<std::string, size_t> &asset_lookup,
	std::vector<MeshAsset> &assets,
	size_t &asset_index,
	std::string &error)
{
	const auto existing = asset_lookup.find(key);
	if (existing != asset_lookup.end()) {
		asset_index = existing->second;
		return true;
	}

	const std::filesystem::path mesh_path =
		runtime::resolve_orbiter_asset_path(content_root / "Meshes", mesh_name + ".msh");
	LoadedMesh mesh;
	if (!load_orbiter_mesh(mesh_path, mesh, error)) {
		return false;
	}
	if (!texture_override.empty()) {
		if (mesh.textures.empty()) {
			mesh.textures.push_back(texture_override);
			for (MeshSubmesh &submesh : mesh.submeshes) {
				submesh.texture_index = 0;
			}
		} else {
			mesh.textures[0] = texture_override;
		}
	}

	asset_index = assets.size();
	assets.push_back({key, std::move(mesh)});
	asset_lookup[key] = asset_index;
	return true;
}

size_t register_generated_asset(
	const std::string &key,
	LoadedMesh mesh,
	std::unordered_map<std::string, size_t> &asset_lookup,
	std::vector<MeshAsset> &assets)
{
	const auto existing = asset_lookup.find(key);
	if (existing != asset_lookup.end()) {
		return existing->second;
	}

	const size_t asset_index = assets.size();
	assets.push_back({key, std::move(mesh)});
	asset_lookup[key] = asset_index;
	return asset_index;
}

} // namespace

bool SimulationSession::initialize(const LaunchOptions &options, std::string &error)
{
	options_ = options;
	frame_scene_ = {};
	vessel_state_ = {};
	warnings_.clear();
	simulation_time_ = 0.0;
	paused_ = false;
	target_throttle_ = 0.0;
	target_hover_altitude_ = 0.0;
	pitch_rate_ = 0.0;
	yaw_rate_ = 0.0;
	roll_rate_ = 0.0;

	ScenarioData scenario;
	if (!parse_scenario(options.scenario_path, scenario, error)) {
		return false;
	}

	const std::filesystem::path earth_base_path =
		options.content_root / "Config" / "Earth" / "Base" / "Canaveral.cfg";
	BaseLocation base_location;
	std::vector<ObjectConfig> base_objects;
	if (!parse_base_config(earth_base_path, base_location, base_objects, error)) {
		return false;
	}

	vessel_state_.name = scenario.ship.name;
	vessel_state_.class_name = scenario.ship.class_name;
	vessel_state_.position = local_position_from_lon_lat(
		base_location.latitude_degrees,
		base_location.longitude_degrees,
		scenario.ship.latitude_degrees,
		scenario.ship.longitude_degrees,
		scenario.ship.altitude_meters);
	vessel_state_.velocity = {};
	vessel_state_.rotation_degrees = {0.0, scenario.ship.heading_degrees, 0.0};
	vessel_state_.throttle = scenario.ship.altitude_meters > 0.01 ? 0.65 : 0.0;
	target_throttle_ = vessel_state_.throttle;
	target_hover_altitude_ = vessel_state_.position.y;
	frame_scene_.focus_name = vessel_state_.name;
	frame_scene_.camera.fov_degrees = scenario.camera_fov_degrees;

	const double camera_length = std::max(length(scenario.camera_offset), 1.0);
	camera_distance_ = camera_length;
	if (options.initial_camera_distance > 0.0) {
		camera_distance_ = options.initial_camera_distance;
	}
	camera_yaw_ = std::atan2(scenario.camera_offset.x, scenario.camera_offset.z);
	camera_pitch_ = std::asin(std::clamp(scenario.camera_offset.y / camera_length, -1.0, 1.0));

	std::unordered_map<std::string, size_t> asset_lookup;

	const size_t vessel_asset_index = [&]() -> size_t {
		const std::filesystem::path vessel_mesh_path =
			options.content_root / "Meshes" / "Quadcopter" / "quadcopter.msh";
		LoadedMesh mesh;
		std::string mesh_error;
		if (!load_orbiter_mesh(vessel_mesh_path, mesh, mesh_error)) {
			error = mesh_error;
			return static_cast<size_t>(-1);
		}
		const size_t asset_index = frame_scene_.assets.size();
		frame_scene_.assets.push_back({"mesh:Quadcopter/quadcopter", std::move(mesh)});
		asset_lookup[frame_scene_.assets.back().id] = asset_index;
		return asset_index;
	}();
	if (vessel_asset_index == static_cast<size_t>(-1)) {
		return false;
	}

	frame_scene_.vessel_instances.push_back({
		vessel_asset_index,
		vessel_state_.position,
		vessel_state_.rotation_degrees,
		{1.0, 1.0, 1.0},
		{0.95f, 0.95f, 0.95f, 1.0f},
	});

	const bool has_earth_surface_assets =
		std::filesystem::exists(options.content_root / "Textures" / "Earth") &&
		std::filesystem::exists(options.content_root / "Textures2" / "Earth");

	// Try real Earth terrain from the packed archives first.
	std::filesystem::path elev_archive_path;
	{
		const std::vector<std::filesystem::path> candidates = {
			options.planet_tex_dir.empty() ? std::filesystem::path{} : options.planet_tex_dir / "Textures" / "Earth" / "Archive" / "Elev.tree",
			options.content_root / "Textures" / "Earth" / "Archive" / "Elev.tree",
		};
		for (const std::filesystem::path &candidate : candidates) {
			if (!candidate.empty() && std::filesystem::exists(candidate)) {
				elev_archive_path = candidate;
				break;
			}
		}
	}

	bool terrain_ready = false;
	if (!elev_archive_path.empty()) {
		TerrainArchive elev_archive;
		std::string terrain_error;
		if (elev_archive.open(elev_archive_path, terrain_error)) {
			const double base_lat_radians = base_location.latitude_degrees * kDegreesToRadians;
			const double base_lng_radians = base_location.longitude_degrees * kDegreesToRadians;

			int center_ilat = 0, center_ilng = 0;
			TerrainArchive::tile_coords(kTerrainPatchLevel, base_lat_radians, base_lng_radians, center_ilat, center_ilng);

			const int nlat = 1 << kTerrainPatchLevel;
			const int nlng = 2 << kTerrainPatchLevel;
			const int ilat_min = std::max(center_ilat - kTerrainPatchesPerSide / 2, 0);
			const int ilat_max = std::min(center_ilat + kTerrainPatchesPerSide / 2, nlat - 1);
			const int ilng_min = std::max(center_ilng - kTerrainPatchesPerSide / 2, 0);
			const int ilng_max = std::min(center_ilng + kTerrainPatchesPerSide / 2, nlng - 1);
			const int patches_per_side = (ilat_max - ilat_min + 1);

			const double pi = 3.14159265358979323846;
			const double min_lat_radians = (0.5 - double(ilat_max + 1) / nlat) * pi;
			const double max_lat_radians = (0.5 - double(ilat_min) / nlat) * pi;
			const double min_lng_radians = double(ilng_min) / nlng * 2.0 * pi - pi;
			const double max_lng_radians = double(ilng_max + 1) / nlng * 2.0 * pi - pi;

			double reference_elevation = 0.0;
			elev_archive.sample_elevation(base_lat_radians, base_lng_radians, kTerrainPatchLevel, reference_elevation);

			TerrainMeshOptions mesh_options;
			mesh_options.samples_per_side = kTerrainSamplesPerSide;
			mesh_options.patch_level = kTerrainPatchLevel;
			mesh_options.min_latitude_degrees = min_lat_radians * kRadiansToDegrees;
			mesh_options.max_latitude_degrees = max_lat_radians * kRadiansToDegrees;
			mesh_options.min_longitude_degrees = min_lng_radians * kRadiansToDegrees;
			mesh_options.max_longitude_degrees = max_lng_radians * kRadiansToDegrees;
			mesh_options.reference_elevation = reference_elevation;
			mesh_options.use_reference_elevation = true;

			std::vector<float> positions, normals, uvs;
			std::vector<uint16_t> indices;
			if (build_terrain_mesh(elev_archive, mesh_options, positions, normals, uvs, indices, terrain_error)) {
				LoadedMesh terrain_mesh;
				const size_t vertex_count = positions.size() / 3;
				terrain_mesh.vertices.reserve(vertex_count);
				for (size_t i = 0; i < vertex_count; ++i) {
					terrain_mesh.vertices.push_back({
						{positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]},
						{normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]},
						{uvs[i * 2 + 0], uvs[i * 2 + 1]},
					});
				}
				terrain_mesh.indices = std::move(indices);
				terrain_mesh.submeshes.push_back({
					0,
					static_cast<uint32_t>(terrain_mesh.indices.size()),
					-1,
					0,
					false,
				});

				// Stitch the matching surface tiles into a PNG texture so the
				// Metal texture loader can consume it (it cannot read DDS).
				const std::filesystem::path surf_archive_path =
					elev_archive_path.parent_path() / "Surf.tree";
				std::string texture_relative;
				TerrainArchive surf_archive;
				std::string surf_error;
				if (std::filesystem::exists(surf_archive_path) && surf_archive.open(surf_archive_path, surf_error)) {
					// Surface archives do not carry every patch level everywhere
					// (ocean tiles are absent near KSC). Load each tile at the
					// finest available level, like the legacy ZTreeMgr does.
					std::vector<TerrainSurfaceTile> surf_tiles;
					bool all_tiles = true;
					for (int tile_ilat = ilat_min; tile_ilat <= ilat_max && all_tiles; ++tile_ilat) {
						for (int tile_ilng = ilng_min; tile_ilng <= ilng_max; ++tile_ilng) {
							TerrainSurfaceTile tile;
							bool loaded = false;
							for (int level = kTerrainPatchLevel; level >= 8 && !loaded; --level) {
								const int shift = kTerrainPatchLevel - level;
								const int parent_ilat = tile_ilat >> shift;
								const int parent_ilng = tile_ilng >> shift;
								if (surf_archive.load_surface_tile(level, parent_ilat, parent_ilng, tile, surf_error)) {
									loaded = true;
								}
							}
							if (!loaded) {
								all_tiles = false;
								break;
							}
							surf_tiles.push_back(std::move(tile));
						}
					}
					if (all_tiles) {
						std::vector<uint8_t> rgba;
						int image_width = 0, image_height = 0;
						if (stitch_surface_tiles(surf_tiles, patches_per_side, rgba, image_width, image_height, surf_error)) {
							const std::filesystem::path generated_dir =
								options.content_root / "Textures" / "Earth" / "Generated";
							std::filesystem::create_directories(generated_dir);
							const std::string image_name =
								"Surf_P" + std::to_string(kTerrainPatchLevel) + "_" +
								std::to_string(ilat_min) + "_" + std::to_string(ilng_min) + ".png";
							const std::filesystem::path image_path = generated_dir / image_name;
							if (write_png_rgba(image_path, image_width, image_height, rgba.data(), surf_error)) {
								texture_relative = "Earth/Generated/" + image_name;
							} else {
								warnings_.push_back("Terrain surface PNG generation failed: " + surf_error);
							}
						} else {
							warnings_.push_back("Terrain surface stitching failed: " + surf_error);
						}
					} else {
						warnings_.push_back("Terrain surface tiles unavailable: " + surf_error);
					}
				} else {
					warnings_.push_back("Earth surface archive unavailable: " + surf_archive_path.string());
				}

				if (!texture_relative.empty()) {
					terrain_mesh.textures.push_back(texture_relative);
				}

				const size_t terrain_asset_index = register_generated_asset(
					"terrain:earth:p" + std::to_string(kTerrainPatchLevel),
					std::move(terrain_mesh),
					asset_lookup,
					frame_scene_.assets);

				// The mesh is built centred on the region centre; place it at the
				// local-frame position of that centre so it aligns with the base origin.
				const double region_center_lat_radians = (min_lat_radians + max_lat_radians) * 0.5;
				const double region_center_lng_radians = (min_lng_radians + max_lng_radians) * 0.5;
				const Vector3 terrain_offset = local_position_from_lon_lat(
					base_location.latitude_degrees,
					base_location.longitude_degrees,
					region_center_lat_radians * kRadiansToDegrees,
					region_center_lng_radians * kRadiansToDegrees,
					0.0);

				frame_scene_.terrain_patches.push_back({
					{
						terrain_asset_index,
						terrain_offset,
						{0.0, 0.0, 0.0},
						{1.0, 1.0, 1.0},
						{1.0f, 1.0f, 1.0f, 1.0f},
					}
				});
				terrain_ready = true;
				frame_scene_.using_procedural_ground = false;
			} else {
				warnings_.push_back("Earth terrain mesh build failed: " + terrain_error);
			}
		} else {
			warnings_.push_back("Earth elevation archive failed to open: " + terrain_error);
		}
	}

	if (!terrain_ready) {
		frame_scene_.using_procedural_ground = true;
		if (!has_earth_surface_assets) {
			warnings_.push_back("Earth terrain/elevation assets were not found in the runtime tree; using procedural ground.");
		} else {
			warnings_.push_back("Earth terrain archives could not be used; using procedural ground over the KSC scene.");
		}

		const size_t ground_asset_index =
			register_generated_asset("primitive:ground", make_unit_plane_mesh(""), asset_lookup, frame_scene_.assets);
		frame_scene_.terrain_patches.push_back({
			{
				ground_asset_index,
				{0.0, -0.01, 0.0},
				{0.0, 0.0, 0.0},
				{kGroundPlaneExtent, 1.0, kGroundPlaneExtent},
				{0.16f, 0.28f, 0.12f, 1.0f},
			}
		});
	}

	const size_t runway_asset_index =
		register_generated_asset("primitive:runway", make_unit_plane_mesh("Runway2"), asset_lookup, frame_scene_.assets);
	const size_t pad_asset_index =
		register_generated_asset("primitive:pad", make_unit_plane_mesh("Lpad02a"), asset_lookup, frame_scene_.assets);
	const size_t box_asset_index =
		register_generated_asset("primitive:box", make_unit_box_mesh(), asset_lookup, frame_scene_.assets);
	const size_t tank_asset_index =
		register_generated_asset("primitive:tank", make_unit_cylinder_mesh(16), asset_lookup, frame_scene_.assets);

	for (const ObjectConfig &object : base_objects) {
		const std::string type = object.type;
		if (type == "RUNWAYLIGHTS") {
			continue;
		}

		Vector3 pos{};
		parse_runtime_vector(object.values, "POS", pos);
		const double rotation = parse_scalar(object.values, "ROT", 0.0);
		Vector3 scale = {1.0, 1.0, 1.0};
		parse_runtime_vector(object.values, "SCALE", scale);
		Color4 color = color_for_type(type);

		BaseInstance instance;
		instance.kind = type;
		instance.mesh.position = pos;
		instance.mesh.rotation_degrees = {0.0, rotation, 0.0};
		instance.mesh.scale = scale;
		instance.mesh.color = color;

		if (type == "BLOCK" || type == "HANGAR" || type == "HANGAR2" || type == "HANGAR3") {
			instance.mesh.asset_index = box_asset_index;
			frame_scene_.base_instances.push_back(instance);
			continue;
		}

		if (type == "TANK") {
			instance.mesh.asset_index = tank_asset_index;
			frame_scene_.base_instances.push_back(instance);
			continue;
		}

		if (type == "LPAD2A") {
			const double uniform_scale = parse_scalar(object.values, "SCALE", 1.0);
			instance.mesh.asset_index = pad_asset_index;
			instance.mesh.scale = {45.0 * uniform_scale, 1.0, 45.0 * uniform_scale};
			frame_scene_.base_instances.push_back(instance);
			continue;
		}

		if (type == "RUNWAY") {
			Vector3 end1{};
			Vector3 end2{};
			if (!parse_runtime_vector(object.values, "END1", end1) ||
				!parse_runtime_vector(object.values, "END2", end2)) {
				continue;
			}
			const double width = parse_scalar(object.values, "WIDTH", 90.0);
			const Vector3 center = (end1 + end2) * 0.5;
			const Vector3 delta = end2 - end1;
			const double heading = std::atan2(delta.x, delta.z) * kRadiansToDegrees;
			instance.mesh.asset_index = runway_asset_index;
			instance.mesh.position = center;
			instance.mesh.rotation_degrees = {0.0, heading, 0.0};
			instance.mesh.scale = {width, 1.0, std::max(length(delta), 1.0)};
			frame_scene_.base_instances.push_back(instance);
			continue;
		}

		if (type == "MESH") {
			const auto file_it = object.values.find("FILE");
			if (file_it == object.values.end()) {
				continue;
			}
			const std::string texture_override =
				object.values.count("TEX") ? object.values.at("TEX") : std::string{};
			size_t asset_index = 0;
			std::string mesh_error;
			if (!register_mesh_asset(
					options.content_root,
					"mesh:" + file_it->second + ":" + texture_override,
					file_it->second,
					texture_override,
					asset_lookup,
					frame_scene_.assets,
					asset_index,
					mesh_error)) {
				warnings_.push_back(mesh_error);
				continue;
			}
			instance.mesh.asset_index = asset_index;
			instance.mesh.scale = {1.0, 1.0, 1.0};
			frame_scene_.base_instances.push_back(instance);
			continue;
		}
	}

	update_camera({});
	update_focus_instance();
	return true;
}

void SimulationSession::step(double dt_seconds, const ControlInput &input)
{
	if (input.pause_toggle) {
		paused_ = !paused_;
	}

	update_camera(input);

	if (paused_ || dt_seconds <= 0.0) {
		vessel_state_.paused = paused_;
		return;
	}

	const double dt = std::min(dt_seconds, 0.05);
	simulation_time_ += dt;

	if (input.hover_hold) {
		target_hover_altitude_ = vessel_state_.position.y;
	}

	if (std::abs(input.thrust) > 1e-4) {
		target_throttle_ = std::clamp(target_throttle_ + input.thrust * dt * 0.85, 0.0, 1.0);
		target_hover_altitude_ = vessel_state_.position.y;
	}

	double throttle = target_throttle_;
	if (input.hover_hold) {
		const double altitude_error = target_hover_altitude_ - vessel_state_.position.y;
		const double vertical_speed_error = -vessel_state_.velocity.y;
		const double hover = (kQuadcopterMassKg * kGravity) / kQuadcopterMaxThrustNewton;
		throttle = std::clamp(hover + altitude_error * 0.08 + vertical_speed_error * 0.05, 0.0, 1.0);
	}
	vessel_state_.throttle = throttle;

	const double target_pitch = std::clamp(input.pitch, -1.0, 1.0) * kMaxTiltRadians;
	const double target_roll = std::clamp(input.roll, -1.0, 1.0) * kMaxTiltRadians;
	const double current_pitch = vessel_state_.rotation_degrees.x * kDegreesToRadians;
	const double current_roll = vessel_state_.rotation_degrees.z * kDegreesToRadians;

	pitch_rate_ += ((target_pitch - current_pitch) * 5.0 - pitch_rate_ * 2.4) * dt;
	roll_rate_ += ((target_roll - current_roll) * 5.0 - roll_rate_ * 2.4) * dt;
	yaw_rate_ += ((std::clamp(input.yaw, -1.0, 1.0) * kMaxYawRate) - yaw_rate_) * 3.5 * dt;

	vessel_state_.rotation_degrees.x =
		std::clamp(vessel_state_.rotation_degrees.x + pitch_rate_ * dt * kRadiansToDegrees, -30.0, 30.0);
	vessel_state_.rotation_degrees.z =
		std::clamp(vessel_state_.rotation_degrees.z + roll_rate_ * dt * kRadiansToDegrees, -30.0, 30.0);
	vessel_state_.rotation_degrees.y =
		std::fmod(vessel_state_.rotation_degrees.y + yaw_rate_ * dt * kRadiansToDegrees + 360.0, 360.0);

	const Vector3 thrust_direction = body_up_vector(vessel_state_.rotation_degrees);
	const Vector3 thrust_acceleration = thrust_direction * (throttle * kQuadcopterMaxThrustNewton / kQuadcopterMassKg);
	const Vector3 gravity_acceleration = {0.0, -kGravity, 0.0};
	const Vector3 drag_acceleration = vessel_state_.velocity * -0.35;
	const Vector3 acceleration = thrust_acceleration + gravity_acceleration + drag_acceleration;

	vessel_state_.velocity = vessel_state_.velocity + acceleration * dt;
	vessel_state_.position = vessel_state_.position + vessel_state_.velocity * dt;

	if (vessel_state_.position.y < 0.255) {
		vessel_state_.position.y = 0.255;
		if (vessel_state_.velocity.y < 0.0) {
			vessel_state_.velocity.y = 0.0;
		}
		vessel_state_.velocity.x *= 0.92;
		vessel_state_.velocity.z *= 0.92;
	}

	vessel_state_.simulation_time = simulation_time_;
	vessel_state_.paused = paused_;
	update_focus_instance();
}

void SimulationSession::update_camera(const ControlInput &input)
{
	camera_yaw_ = clamp_angle(camera_yaw_ + input.camera_yaw_delta);
	camera_pitch_ = std::clamp(camera_pitch_ + input.camera_pitch_delta, -1.3, 1.3);
	camera_distance_ = std::clamp(camera_distance_ + input.camera_zoom_delta, 8.0, 400.0);

	const Vector3 target = vessel_state_.position + Vector3{0.0, 0.6, 0.0};
	const double cos_pitch = std::cos(camera_pitch_);
	const Vector3 offset = {
		std::sin(camera_yaw_) * cos_pitch * camera_distance_,
		std::sin(camera_pitch_) * camera_distance_,
		std::cos(camera_yaw_) * cos_pitch * camera_distance_,
	};

	frame_scene_.camera.target = target;
	frame_scene_.camera.position = target + offset;
}

void SimulationSession::update_focus_instance()
{
	if (frame_scene_.vessel_instances.empty()) {
		return;
	}
	frame_scene_.vessel_instances[0].position = vessel_state_.position;
	frame_scene_.vessel_instances[0].rotation_degrees = vessel_state_.rotation_degrees;
}

} // namespace orbiter::bootstrap
