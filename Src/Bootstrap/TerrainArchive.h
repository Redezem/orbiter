// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =======================================================================
// TerrainArchive.h
// Portable reader for Orbiter packed terrain archives (*.tree).
// Format cross-verified against the legacy Windows ZTreeMgr/elevmgr code.
// =======================================================================

#ifndef ORBITER_BOOTSTRAP_TERRAIN_ARCHIVE_H
#define ORBITER_BOOTSTRAP_TERRAIN_ARCHIVE_H

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace orbiter::bootstrap {

struct TerrainTileHeader {
	std::string id;
	int hdrsize = 0;
	int dtype = 0;
	int xgrid = 0;
	int ygrid = 0;
	int xpad = 0;
	int ypad = 0;
	double scale = 1.0;
	double offset = 0.0;
	double latmin = 0.0;
	double latmax = 0.0;
	double lngmin = 0.0;
	double lngmax = 0.0;
	double emin = 0.0;
	double emax = 0.0;
	double emean = 0.0;
};

// Decoded elevation tile: grid values with padding ring stripped.
struct TerrainElevationTile {
	TerrainTileHeader header;
	int grid = 0;      // usable grid size (xgrid - xpad*2 == ygrid - ypad*2 expected)
	int stride = 0;    // row stride including padding (grid + 1)
	std::vector<double> values; // row-major, padding stripped, scale/offset applied
};

// Decoded surface tile: RGBA8 rows, top row = northern edge.
struct TerrainSurfaceTile {
	TerrainTileHeader header;
	int width = 0;
	int height = 0;
	std::vector<uint8_t> rgba;
};

class TerrainArchive {
public:
	TerrainArchive() = default;
	~TerrainArchive()
	{
		if (file_) {
			std::fclose(file_);
			file_ = nullptr;
		}
	}
	TerrainArchive(const TerrainArchive &) = delete;
	TerrainArchive &operator=(const TerrainArchive &) = delete;

	bool open(const std::filesystem::path &archive_path, std::string &error);
	bool is_open() const { return !nodes_.empty(); }

	// Patch level P addressing (matches ElevationManager::TileIdx):
	// ilat = int((pi/2 - lat)/pi * 2^P), ilng = int((lng+pi)/2pi * 2^(P+1)).
	// Internally maps to tree level P+4 like ZTreeMgr::Idx.
	int tile_index(int patch_level, int ilat, int ilng) const;

	// Convert geographic coordinates (radians) to patch-level tile indices.
	static void tile_coords(int patch_level, double lat_radians, double lng_radians, int &ilat, int &ilng);

	// Bilinear elevation sample in radians; returns false when no tile covers the point.
	bool sample_elevation(double lat_radians, double lng_radians, int patch_level, double &elevation_meters) const;

	bool load_elevation_tile(int patch_level, int ilat, int ilng, TerrainElevationTile &out, std::string &error);
	bool load_surface_tile(int patch_level, int ilat, int ilng, TerrainSurfaceTile &out, std::string &error);

private:
	struct Node {
		int64_t pos = 0;
		uint32_t size = 0;
		uint32_t child[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
	};

	uint32_t idx(int tree_level, int ilat, int ilng) const;
	bool read_node(uint32_t node_index, std::vector<uint8_t> &out, std::string &error) const;

	mutable std::FILE *file_ = nullptr;
	std::filesystem::path path_;
	uint64_t data_ofs_ = 0;
	uint64_t data_length_ = 0;
	std::vector<Node> nodes_;
	uint32_t root_pos1_ = 0xFFFFFFFFu;
	uint32_t root_pos2_ = 0xFFFFFFFFu;
	uint32_t root_pos3_ = 0xFFFFFFFFu;
	uint32_t root_pos4_[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};

	mutable std::map<std::string, TerrainElevationTile> elev_cache_;
};

// Minimal RGBA8 -> PNG writer (zlib deflate). Used to publish stitched
// terrain surface tiles where the GPU texture loader cannot read DDS.
bool write_png_rgba(const std::filesystem::path &path, int width, int height,
	const uint8_t *rgba, std::string &error);

// Build a heightfield mesh covering the geographic rectangle
// [min_lat,max_lat] x [min_lng,max_lng] (degrees). Passing exactly one
// patch's bounds makes the mesh uv [0,1] align with that patch's surface tile.
// Elevation is sampled from the archive.
struct TerrainMeshOptions {
	int samples_per_side = 193;
	int patch_level = 11;
	double min_latitude_degrees = 0.0;
	double max_latitude_degrees = 0.0;
	double min_longitude_degrees = 0.0;
	double max_longitude_degrees = 0.0;
	double vertical_exaggeration = 1.0;
	double reference_elevation = 0.0;      // subtracted from samples when enabled
	bool use_reference_elevation = false;  // local relief, not geoid height
};

bool build_terrain_mesh(
	TerrainArchive &archive,
	const TerrainMeshOptions &options,
	std::vector<float> &positions,   // row-major x,y,z triples
	std::vector<float> &normals,
	std::vector<float> &uvs,
	std::vector<uint16_t> &indices,
	std::string &error);

} // namespace orbiter::bootstrap

#endif // ORBITER_BOOTSTRAP_TERRAIN_ARCHIVE_H
