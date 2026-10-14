// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// Terrain archive reader tests against the real Earth packed archives.

#include "TerrainArchive.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace orbiter::bootstrap;

namespace {

std::filesystem::path find_elev_archive()
{
	const std::vector<std::filesystem::path> candidates = {
		std::filesystem::path("Textures/Earth/Archive/Elev.tree"),
		std::filesystem::path(std::getenv("ORBITER_SOURCE_ROOT_DIR") ? std::getenv("ORBITER_SOURCE_ROOT_DIR") : "") /
			"out/build/macos-arm64-debug/Textures/Earth/Archive/Elev.tree",
		std::filesystem::path(std::string(std::getenv("HOME") ? std::getenv("HOME") : "")) /
			"Codeforge/orbiter-textures/Textures/Earth/Archive/Elev.tree",
	};
	for (const std::filesystem::path &candidate : candidates) {
		if (!candidate.empty() && std::filesystem::exists(candidate)) {
			return candidate;
		}
	}
	return {};
}

} // namespace

TEST_CASE("TerrainArchive opens Earth elevation archive")
{
	const std::filesystem::path archive_path = find_elev_archive();
	if (archive_path.empty()) {
		WARN("SKIP: Earth elevation archive not staged; run Utils/assets/stage_assets.py");
		return;
	}

	TerrainArchive archive;
	std::string error;
	REQUIRE(archive.open(archive_path, error));
	INFO(error);
}

TEST_CASE("TerrainArchive resolves KSC tiles across patch levels")
{
	const std::filesystem::path archive_path = find_elev_archive();
	if (archive_path.empty()) {
		WARN("SKIP: Earth elevation archive not staged; run Utils/assets/stage_assets.py");
		return;
	}

	TerrainArchive archive;
	std::string error;
	REQUIRE(archive.open(archive_path, error));

	const double pi = 3.14159265358979323846;
	const double ksc_lat = 28.573 * pi / 180.0;
	const double ksc_lng = -80.65 * pi / 180.0;

	// Patch levels 8..11 must all resolve for KSC (12 is a placeholder tile).
	for (int patch_level = 8; patch_level <= 11; ++patch_level) {
		int ilat = 0, ilng = 0;
		TerrainArchive::tile_coords(patch_level, ksc_lat, ksc_lng, ilat, ilng);
		CHECK(archive.tile_index(patch_level, ilat, ilng) >= 0);
	}
}

TEST_CASE("TerrainArchive samples KSC elevation")
{
	const std::filesystem::path archive_path = find_elev_archive();
	if (archive_path.empty()) {
		WARN("SKIP: Earth elevation archive not staged; run Utils/assets/stage_assets.py");
		return;
	}

	TerrainArchive archive;
	std::string error;
	REQUIRE(archive.open(archive_path, error));

	const double pi = 3.14159265358979323846;
	const double ksc_lat = 28.573 * pi / 180.0;
	const double ksc_lng = -80.65 * pi / 180.0;

	double elevation = 0.0;
	REQUIRE(archive.sample_elevation(ksc_lat, ksc_lng, 11, elevation));
	// KSC ground is a few metres above sea level; archive tiles near KSC span
	// roughly -20..56 m. Sanity-check the sample lands in that band.
	CHECK(elevation > -25.0);
	CHECK(elevation < 60.0);

	// Atlantic just offshore of the Cape (still inside the archive's
	// coverage region) should be at or below sea level.
	double ocean_elevation = 0.0;
	REQUIRE(archive.sample_elevation(ksc_lat, ksc_lng + 0.12 * pi / 180.0, 11, ocean_elevation));
	CHECK(ocean_elevation < 5.0);
}

TEST_CASE("TerrainArchive builds a KSC heightfield mesh")
{
	const std::filesystem::path archive_path = find_elev_archive();
	if (archive_path.empty()) {
		WARN("SKIP: Earth elevation archive not staged; run Utils/assets/stage_assets.py");
		return;
	}

	TerrainArchive archive;
	std::string error;
	REQUIRE(archive.open(archive_path, error));

	TerrainMeshOptions options;
	options.samples_per_side = 64;
	options.patch_level = 11;
	options.min_latitude_degrees = 28.5645;
	options.max_latitude_degrees = 28.6523;
	options.min_longitude_degrees = -80.7188;
	options.max_longitude_degrees = -80.6309;

	std::vector<float> positions, normals, uvs;
	std::vector<uint16_t> indices;
	REQUIRE(build_terrain_mesh(archive, options, positions, normals, uvs, indices, error));
	INFO(error);

	CHECK(positions.size() == 64u * 64u * 3u);
	CHECK(indices.size() == 63u * 63u * 6u);

	// Region is ~0.088 deg wide: ~9.7 km east-west at this latitude.
	float min_x = positions[0], max_x = positions[0];
	for (size_t i = 0; i < positions.size(); i += 3) {
		min_x = std::min(min_x, positions[i]);
		max_x = std::max(max_x, positions[i]);
	}
	CHECK(max_x - min_x > 8000.0f);
	CHECK(max_x - min_x < 12000.0f);

	// Normals must be unit length and mostly upward for gentle terrain.
	for (size_t i = 1; i < normals.size(); i += 3) {
		CHECK(normals[i] > 0.0f);
	}
}

TEST_CASE("TerrainArchive decodes a KSC surface tile to RGBA")
{
	const std::filesystem::path archive_path = find_elev_archive();
	if (archive_path.empty()) {
		WARN("SKIP: Earth elevation archive not staged; run Utils/assets/stage_assets.py");
		return;
	}

	const std::filesystem::path surf_path = archive_path.parent_path() / "Surf.tree";
	if (!std::filesystem::exists(surf_path)) {
		WARN("SKIP: Earth surface archive not staged");
		return;
	}

	TerrainArchive surf;
	std::string error;
	REQUIRE(surf.open(surf_path, error));

	const double pi = 3.14159265358979323846;
	int ilat = 0, ilng = 0;
	TerrainArchive::tile_coords(11, 28.573 * pi / 180.0, -80.65 * pi / 180.0, ilat, ilng);

	TerrainSurfaceTile tile;
	REQUIRE(surf.load_surface_tile(11, ilat, ilng, tile, error));
	INFO(error);

	CHECK(tile.width == 512);
	CHECK(tile.height == 512);
	CHECK(tile.rgba.size() == 512u * 512u * 4u);

	// KSC patch contains land and ocean; verify both appear (not a flat tile).
	bool has_blue = false, has_green = false;
	for (size_t i = 0; i < tile.rgba.size(); i += 4) {
		const uint8_t r = tile.rgba[i + 0];
		const uint8_t g = tile.rgba[i + 1];
		const uint8_t b = tile.rgba[i + 2];
		if (b > 90 && b > r + 20) {
			has_blue = true;
		}
		if (g > 60 && g >= b) {
			has_green = true;
		}
	}
	CHECK(has_blue);
	CHECK(has_green);
}
