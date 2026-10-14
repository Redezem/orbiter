// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =======================================================================
// TerrainArchive.cpp
// Portable reader for Orbiter packed terrain archives (*.tree).
// Format cross-verified against the legacy Windows ZTreeMgr/elevmgr code:
//   header 44B: magic[4], size u32, flags u32, dataOfs u32,
//               dataLength i64, nodeCount u32, rootPos1..3 u32, rootPos4[2] u32
//   TOC: nodeCount * 32B stride: pos i64, sizeInflated u32, child[4] u32
//   node payload: zlib deflate; elevation tiles carry a 100-byte
//   ELEVFILEHEADER; surface tiles are DXT1 DDS images.
// =======================================================================

#include "TerrainArchive.h"

#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace orbiter::bootstrap {

namespace {

constexpr uint32_t kNoChild = 0xFFFFFFFFu;
constexpr int kTreeLevelOffset = 4; // patch level P <-> tree level P+4

#pragma pack(push, 1)
struct RawTreeFileHeader {
	uint8_t magic[4];
	uint32_t size;
	uint32_t flags;
	uint32_t data_ofs;
	int64_t data_length;
	uint32_t node_count;
	uint32_t root_pos1;
	uint32_t root_pos2;
	uint32_t root_pos3;
	uint32_t root_pos4[2];
};

struct RawElevFileHeader {
	uint8_t id[4];
	int32_t hdrsize;
	int32_t dtype;
	int32_t xgrid;
	int32_t ygrid;
	int32_t xpad;
	int32_t ypad;
	double scale;
	double offset;
	double latmin;
	double latmax;
	double lngmin;
	double lngmax;
	double emin;
	double emax;
	double emean;
};
#pragma pack(pop)

bool inflate_node(const uint8_t *input, size_t input_size, uint8_t *output, size_t output_size)
{
	z_stream stream{};
	if (inflateInit(&stream) != Z_OK) {
		return false;
	}
	stream.next_in = const_cast<Bytef *>(input);
	stream.avail_in = static_cast<uInt>(input_size);
	stream.next_out = output;
	stream.avail_out = static_cast<uInt>(output_size);
	const int result = inflate(&stream, Z_FINISH);
	const bool ok = (result == Z_STREAM_END);
	inflateEnd(&stream);
	return ok;
}

void unpack565(uint16_t value, uint8_t &r, uint8_t &g, uint8_t &b)
{
	const uint32_t rr = (value >> 11) & 0x1F;
	const uint32_t gg = (value >> 5) & 0x3F;
	const uint32_t bb = value & 0x1F;
	r = static_cast<uint8_t>(rr * 255 / 31);
	g = static_cast<uint8_t>(gg * 255 / 63);
	b = static_cast<uint8_t>(bb * 255 / 31);
}

// Decode one 8-byte DXT1 block into 16 RGBA pixels (row-major 4x4).
void decode_dxt1_block(const uint8_t *block, uint8_t out[16 * 4])
{
	uint16_t c0 = 0, c1 = 0;
	std::memcpy(&c0, block, 2);
	std::memcpy(&c1, block + 2, 2);

	uint8_t a[3], c[3];
	unpack565(c0, a[0], a[1], a[2]);
	unpack565(c1, c[0], c[1], c[2]);

	uint8_t palette[4][3];
	palette[0][0] = a[0]; palette[0][1] = a[1]; palette[0][2] = a[2];
	palette[1][0] = c[0]; palette[1][1] = c[1]; palette[1][2] = c[2];
	if (c0 > c1) {
		for (int k = 0; k < 3; ++k) {
			palette[2][k] = static_cast<uint8_t>((3 * a[k] + c[k]) / 4);
			palette[3][k] = static_cast<uint8_t>((a[k] + 3 * c[k]) / 4);
		}
	} else {
		for (int k = 0; k < 3; ++k) {
			palette[2][k] = static_cast<uint8_t>((a[k] + c[k]) / 2);
			palette[3][k] = 0;
		}
	}

	uint32_t bits = 0;
	std::memcpy(&bits, block + 4, 4);
	for (int i = 0; i < 16; ++i) {
		const uint32_t sel = (bits >> (2 * i)) & 3u;
		out[i * 4 + 0] = palette[sel][0];
		out[i * 4 + 1] = palette[sel][1];
		out[i * 4 + 2] = palette[sel][2];
		out[i * 4 + 3] = 255;
	}
}

bool decode_dds_dxt1(const std::vector<uint8_t> &dds, TerrainSurfaceTile &tile, std::string &error)
{
	if (dds.size() < 128) {
		error = "DDS tile too small";
		return false;
	}
	uint32_t height = 0, width = 0;
	std::memcpy(&height, dds.data() + 12, 4);
	std::memcpy(&width, dds.data() + 16, 4);
	const uint32_t fourcc = *reinterpret_cast<const uint32_t *>(dds.data() + 84);
	if (fourcc != 0x31545844u /* 'DXT1' */) {
		error = "Unsupported DDS pixel format (expected DXT1)";
		return false;
	}
	if (width == 0 || height == 0 || (width % 4) != 0 || (height % 4) != 0) {
		error = "Invalid DDS dimensions";
		return false;
	}
	const size_t blocks_x = width / 4;
	const size_t blocks_y = height / 4;
	const size_t needed = 128 + blocks_x * blocks_y * 8;
	if (dds.size() < needed) {
		error = "DDS tile data truncated";
		return false;
	}

	tile.width = static_cast<int>(width);
	tile.height = static_cast<int>(height);
	tile.rgba.assign(width * height * 4, 0);

	uint8_t block_pixels[16 * 4];
	for (size_t by = 0; by < blocks_y; ++by) {
		for (size_t bx = 0; bx < blocks_x; ++bx) {
			const uint8_t *block = dds.data() + 128 + (by * blocks_x + bx) * 8;
			decode_dxt1_block(block, block_pixels);
			for (int j = 0; j < 4; ++j) {
				const size_t row = (by * 4 + static_cast<size_t>(j)) * width + bx * 4;
				std::memcpy(tile.rgba.data() + row * 4, block_pixels + static_cast<size_t>(j) * 16, 16);
			}
		}
	}
	return true;
}

} // namespace

bool TerrainArchive::open(const std::filesystem::path &archive_path, std::string &error)
{
	if (file_) {
		std::fclose(file_);
		file_ = nullptr;
	}
	nodes_.clear();
	elev_cache_.clear();

	std::FILE *f = std::fopen(archive_path.string().c_str(), "rb");
	if (!f) {
		error = "Failed to open terrain archive: " + archive_path.string();
		return false;
	}

	RawTreeFileHeader header{};
	if (std::fread(&header, sizeof(header), 1, f) != 1) {
		std::fclose(f);
		error = "Failed to read terrain archive header: " + archive_path.string();
		return false;
	}
	if (std::memcmp(header.magic, "TREE", 4) != 0 && header.size < 44) {
		// Magic is version-specific; the monolith does not validate it.
		// Require the header size field to be sane instead.
		if (header.size != 44) {
			std::fclose(f);
			error = "Unexpected terrain archive header size: " + archive_path.string();
			return false;
		}
	}

	// TOC entries are 32-byte stride (TreeNode: i64 + 3*u32 + padding).
	const uint64_t toc_bytes = header.data_ofs - 44;
	if (header.node_count == 0 || toc_bytes < static_cast<uint64_t>(header.node_count) * 28) {
		std::fclose(f);
		error = "Invalid terrain archive TOC: " + archive_path.string();
		return false;
	}
	const uint32_t stride = static_cast<uint32_t>(toc_bytes / header.node_count);
	if (stride < 28) {
		std::fclose(f);
		error = "Invalid terrain archive TOC stride: " + archive_path.string();
		return false;
	}

	std::vector<uint8_t> toc(static_cast<size_t>(stride) * header.node_count);
	if (std::fread(toc.data(), toc.size(), 1, f) != 1) {
		std::fclose(f);
		error = "Failed to read terrain archive TOC: " + archive_path.string();
		return false;
	}

	nodes_.resize(header.node_count);
	for (uint32_t i = 0; i < header.node_count; ++i) {
		const uint8_t *entry = toc.data() + static_cast<size_t>(i) * stride;
		Node &node = nodes_[i];
		std::memcpy(&node.pos, entry, 8);
		std::memcpy(&node.size, entry + 8, 4);
		for (int k = 0; k < 4; ++k) {
			std::memcpy(&node.child[k], entry + 12 + static_cast<size_t>(k) * 4, 4);
		}
	}

	data_ofs_ = header.data_ofs;
	data_length_ = static_cast<uint64_t>(header.data_length);
	root_pos1_ = header.root_pos1;
	root_pos2_ = header.root_pos2;
	root_pos3_ = header.root_pos3;
	root_pos4_[0] = header.root_pos4[0];
	root_pos4_[1] = header.root_pos4[1];
	path_ = archive_path;
	file_ = f;
	return true;
}

void TerrainArchive::tile_coords(int patch_level, double lat_radians, double lng_radians, int &ilat, int &ilng)
{
	const double pi = 3.14159265358979323846;
	const int nlat = 1 << patch_level;
	const int nlng = 2 << patch_level;
	ilat = static_cast<int>((pi / 2 - lat_radians) / pi * nlat);
	ilng = static_cast<int>((lng_radians + pi) / (2 * pi) * nlng);
	ilat = std::clamp(ilat, 0, nlat - 1);
	ilng = std::clamp(ilng, 0, nlng - 1);
}

uint32_t TerrainArchive::idx(int tree_level, int ilat, int ilng) const
{
	if (tree_level <= 4) {
		if (tree_level == 1) return root_pos1_;
		if (tree_level == 2) return root_pos2_;
		if (tree_level == 3) return root_pos3_;
		if (ilng < 0 || ilng > 1) return kNoChild;
		return root_pos4_[ilng];
	}
	const uint32_t parent = idx(tree_level - 1, ilat / 2, ilng / 2);
	if (parent == kNoChild || parent >= nodes_.size()) {
		return kNoChild;
	}
	const int cidx = ((ilat & 1) << 1) + (ilng & 1);
	return nodes_[parent].child[cidx];
}

int TerrainArchive::tile_index(int patch_level, int ilat, int ilng) const
{
	if (!is_open() || patch_level < 0) {
		return -1;
	}
	const uint32_t result = idx(patch_level + kTreeLevelOffset, ilat, ilng);
	if (result == kNoChild || result >= nodes_.size()) {
		return -1;
	}
	return static_cast<int>(result);
}

bool TerrainArchive::read_node(uint32_t node_index, std::vector<uint8_t> &out, std::string &error) const
{
	if (!file_ || node_index >= nodes_.size()) {
		error = "Invalid terrain archive node index";
		return false;
	}
	const Node &node = nodes_[node_index];
	if (node.size == 0) {
		error = "Terrain archive node has no data";
		return false;
	}
	const int64_t end_pos =
		(node_index + 1 < nodes_.size()) ? nodes_[node_index + 1].pos : static_cast<int64_t>(data_length_);
	const int64_t deflated_size = end_pos - node.pos;
	if (deflated_size <= 0) {
		error = "Invalid terrain archive node size";
		return false;
	}

	std::vector<uint8_t> deflated(static_cast<size_t>(deflated_size));
	if (std::fseek(file_, static_cast<long>(data_ofs_ + static_cast<uint64_t>(node.pos)), SEEK_SET) != 0) {
		error = "Failed to seek terrain archive node";
		return false;
	}
	if (std::fread(deflated.data(), deflated.size(), 1, file_) != 1) {
		error = "Failed to read terrain archive node data";
		return false;
	}

	out.assign(node.size, 0);
	if (!inflate_node(deflated.data(), deflated.size(), out.data(), out.size())) {
		error = "Failed to inflate terrain archive node";
		return false;
	}
	return true;
}

bool TerrainArchive::load_elevation_tile(
	int patch_level,
	int ilat,
	int ilng,
	TerrainElevationTile &out,
	std::string &error)
{
	const int node_index = tile_index(patch_level, ilat, ilng);
	if (node_index < 0) {
		error = "Elevation tile not present";
		return false;
	}

	std::vector<uint8_t> data;
	if (!read_node(static_cast<uint32_t>(node_index), data, error)) {
		return false;
	}
	if (data.size() < sizeof(RawElevFileHeader)) {
		error = "Elevation tile truncated";
		return false;
	}

	RawElevFileHeader raw{};
	std::memcpy(&raw, data.data(), sizeof(raw));
	if (raw.hdrsize < static_cast<int32_t>(sizeof(RawElevFileHeader)) || raw.hdrsize > static_cast<int32_t>(data.size())) {
		error = "Invalid elevation tile header size";
		return false;
	}
	if (raw.xgrid <= 0 || raw.ygrid <= 0) {
		error = "Invalid elevation tile grid";
		return false;
	}

	out.header.id.assign(reinterpret_cast<const char *>(raw.id), 4);
	out.header.hdrsize = raw.hdrsize;
	out.header.dtype = raw.dtype;
	out.header.xgrid = raw.xgrid;
	out.header.ygrid = raw.ygrid;
	out.header.xpad = raw.xpad;
	out.header.ypad = raw.ypad;
	out.header.scale = raw.scale;
	out.header.offset = raw.offset;
	out.header.latmin = raw.latmin;
	out.header.latmax = raw.latmax;
	out.header.lngmin = raw.lngmin;
	out.header.lngmax = raw.lngmax;
	out.header.emin = raw.emin;
	out.header.emax = raw.emax;
	out.header.emean = raw.emean;

	const int stride = raw.xgrid; // full stored row including padding ring
	const int usable_x = raw.xgrid - 2 * raw.xpad;
	const int usable_y = raw.ygrid - 2 * raw.ypad;
	if (usable_x <= 1 || usable_y <= 1) {
		error = "Elevation tile usable grid too small";
		return false;
	}

	const size_t sample_count = static_cast<size_t>(raw.xgrid) * raw.ygrid;
	const size_t payload_bytes =
		(raw.dtype == -16) ? sample_count * 2 : (raw.dtype == 8 ? sample_count : 0);
	if (raw.dtype != 0 && payload_bytes == 0) {
		error = "Unsupported elevation tile dtype";
		return false;
	}
	if (raw.hdrsize + payload_bytes > data.size()) {
		error = "Elevation tile payload truncated";
		return false;
	}

	out.grid = usable_x;
	out.stride = stride;
	out.values.assign(static_cast<size_t>(usable_x) * usable_y, 0.0);

	const uint8_t *payload = data.data() + raw.hdrsize;
	for (int row = 0; row < usable_y; ++row) {
		const int stored_row = row + raw.ypad;
		for (int col = 0; col < usable_x; ++col) {
			const int stored_col = col + raw.xpad;
			double value = 0.0;
			if (raw.dtype == -16) {
				int16_t raw_value = 0;
				std::memcpy(&raw_value, payload + (static_cast<size_t>(stored_row) * stride + stored_col) * 2, 2);
				value = raw_value;
			} else if (raw.dtype == 8) {
				value = payload[static_cast<size_t>(stored_row) * stride + stored_col];
			} else {
				value = 0.0; // dtype 0: flat tile defined by offset
			}
			out.values[static_cast<size_t>(row) * usable_x + col] = value * raw.scale + raw.offset;
		}
	}

	return true;
}

bool TerrainArchive::load_surface_tile(
	int patch_level,
	int ilat,
	int ilng,
	TerrainSurfaceTile &out,
	std::string &error)
{
	const int node_index = tile_index(patch_level, ilat, ilng);
	if (node_index < 0) {
		error = "Surface tile not present";
		return false;
	}

	std::vector<uint8_t> data;
	if (!read_node(static_cast<uint32_t>(node_index), data, error)) {
		return false;
	}
	if (data.size() < 4 || std::memcmp(data.data(), "DDS ", 4) != 0) {
		error = "Surface tile is not a DDS image";
		return false;
	}
	return decode_dds_dxt1(data, out, error);
}

bool TerrainArchive::sample_elevation(
	double lat_radians,
	double lng_radians,
	int patch_level,
	double &elevation_meters) const
{
	// Like the legacy ElevationManager, fall back to coarser patch levels
	// when the requested level has no tile for this location.
	for (int level = patch_level; level >= 0; --level) {
		int ilat = 0, ilng = 0;
		tile_coords(level, lat_radians, lng_radians, ilat, ilng);

		const std::string cache_key =
			std::to_string(level) + ":" + std::to_string(ilat) + ":" + std::to_string(ilng);
		auto cached = elev_cache_.find(cache_key);
		if (cached == elev_cache_.end()) {
			TerrainElevationTile tile;
			std::string error;
			if (!const_cast<TerrainArchive *>(this)->load_elevation_tile(level, ilat, ilng, tile, error)) {
				continue;
			}
			cached = elev_cache_.emplace(cache_key, std::move(tile)).first;
		}
		const TerrainElevationTile &tile = cached->second;

		if (tile.header.latmax <= tile.header.latmin || tile.header.lngmax <= tile.header.lngmin) {
			continue;
		}
		if (lat_radians < tile.header.latmin || lat_radians > tile.header.latmax ||
			lng_radians < tile.header.lngmin || lng_radians > tile.header.lngmax) {
			// Tile exists but does not cover this point (placeholder tiles can
			// carry degenerate bounds); try a coarser level.
			continue;
		}

		// Bilinear sample in tile-local grid coordinates (legacy elevmgr convention:
		// stored row 0 corresponds to latmin, column 0 to lngmin).
		const double latidx = (lat_radians - tile.header.latmin) / (tile.header.latmax - tile.header.latmin) * (tile.grid - 1);
		const double lngidx = (lng_radians - tile.header.lngmin) / (tile.header.lngmax - tile.header.lngmin) * (tile.grid - 1);
		int lat0 = static_cast<int>(latidx);
		int lng0 = static_cast<int>(lngidx);
		lat0 = std::clamp(lat0, 0, tile.grid - 2);
		lng0 = std::clamp(lng0, 0, tile.grid - 2);
		const double w_lat = std::clamp(latidx - lat0, 0.0, 1.0);
		const double w_lng = std::clamp(lngidx - lng0, 0.0, 1.0);

		const double e00 = tile.values[static_cast<size_t>(lat0) * tile.grid + lng0];
		const double e01 = tile.values[static_cast<size_t>(lat0) * tile.grid + lng0 + 1];
		const double e10 = tile.values[static_cast<size_t>(lat0 + 1) * tile.grid + lng0];
		const double e11 = tile.values[static_cast<size_t>(lat0 + 1) * tile.grid + lng0 + 1];

		const double e0 = e00 * (1.0 - w_lng) + e01 * w_lng;
		const double e1 = e10 * (1.0 - w_lng) + e11 * w_lng;
		elevation_meters = e0 * (1.0 - w_lat) + e1 * w_lat;
		return true;
	}
	return false;
}

bool build_terrain_mesh(
	TerrainArchive &archive,
	const TerrainMeshOptions &options,
	std::vector<float> &positions,
	std::vector<float> &normals,
	std::vector<float> &uvs,
	std::vector<uint16_t> &indices,
	std::string &error)
{
	const double pi = 3.14159265358979323846;
	const double earth_radius_m = 6371000.0;

	const int samples = std::max(3, options.samples_per_side);
	const double min_lat = options.min_latitude_degrees * pi / 180.0;
	const double max_lat = options.max_latitude_degrees * pi / 180.0;
	const double min_lng = options.min_longitude_degrees * pi / 180.0;
	const double max_lng = options.max_longitude_degrees * pi / 180.0;
	if (max_lat <= min_lat || max_lng <= min_lng) {
		error = "Invalid terrain mesh bounds";
		return false;
	}

	const double mid_lat = (min_lat + max_lat) * 0.5;
	const double east_extent = (max_lng - min_lng) * std::cos(mid_lat) * earth_radius_m;
	const double north_extent = (max_lat - min_lat) * earth_radius_m;

	std::vector<double> heights(static_cast<size_t>(samples) * samples, 0.0);
	bool any_sample = false;
	for (int row = 0; row < samples; ++row) {
		const double t_row = static_cast<double>(row) / (samples - 1); // 0 = north edge
		const double lat = max_lat - (max_lat - min_lat) * t_row;
		for (int col = 0; col < samples; ++col) {
			const double t_col = static_cast<double>(col) / (samples - 1); // 0 = west edge
			const double lng = min_lng + (max_lng - min_lng) * t_col;
			double elevation = 0.0;
			if (archive.sample_elevation(lat, lng, options.patch_level, elevation)) {
				any_sample = true;
			}
			heights[static_cast<size_t>(row) * samples + col] = elevation;
		}
	}
	if (!any_sample) {
		error = "Terrain archive produced no elevation samples";
		return false;
	}
	const double reference = options.use_reference_elevation ? options.reference_elevation : 0.0;

	const double exaggeration = options.vertical_exaggeration;
	positions.assign(static_cast<size_t>(samples) * samples * 3, 0.0f);
	for (int row = 0; row < samples; ++row) {
		const double t_row = static_cast<double>(row) / (samples - 1);
		for (int col = 0; col < samples; ++col) {
			const double t_col = static_cast<double>(col) / (samples - 1);
			const double x = (t_col - 0.5) * east_extent;
			const double z = (0.5 - t_row) * north_extent;
			const double y = (heights[static_cast<size_t>(row) * samples + col] - reference) * exaggeration;
			const size_t o = (static_cast<size_t>(row) * samples + col) * 3;
			positions[o + 0] = static_cast<float>(x);
			positions[o + 1] = static_cast<float>(y);
			positions[o + 2] = static_cast<float>(z);
		}
	}

	uvs.assign(static_cast<size_t>(samples) * samples * 2, 0.0f);
	for (int row = 0; row < samples; ++row) {
		for (int col = 0; col < samples; ++col) {
			const size_t o = (static_cast<size_t>(row) * samples + col) * 2;
			uvs[o + 0] = static_cast<float>(options.uv_offset_u + options.uv_scale_u * static_cast<double>(col) / (samples - 1));
			uvs[o + 1] = static_cast<float>(options.uv_offset_v + options.uv_scale_v * (1.0 - static_cast<double>(row) / (samples - 1)));
		}
	}

	indices.clear();
	indices.reserve(static_cast<size_t>(samples - 1) * (samples - 1) * 6);
	for (int row = 0; row + 1 < samples; ++row) {
		for (int col = 0; col + 1 < samples; ++col) {
			const uint16_t v00 = static_cast<uint16_t>(row * samples + col);
			const uint16_t v01 = static_cast<uint16_t>(row * samples + col + 1);
			const uint16_t v10 = static_cast<uint16_t>((row + 1) * samples + col);
			const uint16_t v11 = static_cast<uint16_t>((row + 1) * samples + col + 1);
			// Counter-clockwise when viewed from above (+Y), matching the Metal
			// renderer's front-facing winding: (v00, v01, v11) has cross product
			// (v01-v00) x (v11-v00) pointing +Y.
			indices.insert(indices.end(), {v00, v01, v11});
			indices.insert(indices.end(), {v00, v11, v10});
		}
	}

	normals.assign(static_cast<size_t>(samples) * samples * 3, 0.0f);
	const double cell_east = east_extent / (samples - 1);
	const double cell_north = north_extent / (samples - 1);
	for (int row = 0; row < samples; ++row) {
		for (int col = 0; col < samples; ++col) {
			const int col_l = std::max(col - 1, 0);
			const int col_r = std::min(col + 1, samples - 1);
			const int row_n = std::max(row - 1, 0);
			const int row_s = std::min(row + 1, samples - 1);
			const double dhdx =
				(heights[static_cast<size_t>(row) * samples + col_r] -
					heights[static_cast<size_t>(row) * samples + col_l]) *
				exaggeration / ((col_r - col_l) * cell_east);
			const double dhdz =
				(heights[static_cast<size_t>(row_s) * samples + col] -
					heights[static_cast<size_t>(row_n) * samples + col]) *
				exaggeration / ((row_s - row_n) * cell_north);
			double nx = -dhdx;
			double ny = 1.0;
			double nz = -dhdz;
			const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
			nx /= len;
			ny /= len;
			nz /= len;
			const size_t o = (static_cast<size_t>(row) * samples + col) * 3;
			normals[o + 0] = static_cast<float>(nx);
			normals[o + 1] = static_cast<float>(ny);
			normals[o + 2] = static_cast<float>(nz);
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// Minimal PNG writer (RGBA8, zlib deflate, CRC32 from zlib).
// ---------------------------------------------------------------------------

namespace {

uint32_t png_crc(uint32_t crc, const uint8_t *data, size_t length)
{
	return static_cast<uint32_t>(crc32(static_cast<uInt>(crc), data, static_cast<uInt>(length)));
}

void png_be32(uint8_t *p, uint32_t value)
{
	p[0] = static_cast<uint8_t>(value >> 24);
	p[1] = static_cast<uint8_t>(value >> 16);
	p[2] = static_cast<uint8_t>(value >> 8);
	p[3] = static_cast<uint8_t>(value);
}

void png_chunk(std::vector<uint8_t> &out, const char *type, const uint8_t *data, size_t length)
{
	const size_t header = out.size();
	out.resize(header + 12 + length);
	png_be32(out.data() + header, static_cast<uint32_t>(length));
	std::memcpy(out.data() + header + 4, type, 4);
	if (length) {
		std::memcpy(out.data() + header + 8, data, length);
	}
	const uint32_t crc = png_crc(0, out.data() + header + 4, 4 + length);
	png_be32(out.data() + header + 8 + length, crc);
}

} // namespace

bool write_png_rgba(const std::filesystem::path &path, int width, int height,
	const uint8_t *rgba, std::string &error)
{
	if (width <= 0 || height <= 0 || !rgba) {
		error = "Invalid PNG input";
		return false;
	}

	// Filtered scanlines: filter byte 0 + RGBA row.
	std::vector<uint8_t> raw(static_cast<size_t>(height) * (1 + width * 4));
	for (int y = 0; y < height; ++y) {
		uint8_t *row = raw.data() + static_cast<size_t>(y) * (1 + width * 4);
		row[0] = 0;
		std::memcpy(row + 1, rgba + static_cast<size_t>(y) * width * 4, static_cast<size_t>(width) * 4);
	}

	uLongf compressed_size = compressBound(static_cast<uLong>(raw.size()));
	std::vector<uint8_t> compressed(compressed_size);
	if (compress(compressed.data(), &compressed_size, raw.data(), static_cast<uLong>(raw.size())) != Z_OK) {
		error = "PNG compression failed";
		return false;
	}
	compressed.resize(compressed_size);

	std::vector<uint8_t> png;
	png.reserve(8 + 25 + compressed_size + 40);
	const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
	png.insert(png.end(), signature, signature + 8);

	uint8_t ihdr[13];
	png_be32(ihdr, static_cast<uint32_t>(width));
	png_be32(ihdr + 4, static_cast<uint32_t>(height));
	ihdr[8] = 8;  // bit depth
	ihdr[9] = 6;  // color type RGBA
	ihdr[10] = 0; // compression
	ihdr[11] = 0; // filter
	ihdr[12] = 0; // interlace
	png_chunk(png, "IHDR", ihdr, sizeof(ihdr));
	png_chunk(png, "IDAT", compressed.data(), compressed.size());
	png_chunk(png, "IEND", nullptr, 0);

	std::FILE *f = std::fopen(path.string().c_str(), "wb");
	if (!f) {
		error = "Failed to open PNG output: " + path.string();
		return false;
	}
	const bool wrote = std::fwrite(png.data(), png.size(), 1, f) == 1;
	std::fclose(f);
	if (!wrote) {
		error = "Failed to write PNG output: " + path.string();
		return false;
	}
	return true;
}

} // namespace orbiter::bootstrap
