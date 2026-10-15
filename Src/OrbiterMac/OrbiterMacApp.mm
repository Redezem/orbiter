// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "OrbiterMacApp.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <simd/simd.h>

#include <zlib.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_metal.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace orbiter::macos {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct ShaderVertex {
	vector_float3 position;
	vector_float3 normal;
	vector_float2 uv;
};

struct FrameUniforms {
	matrix_float4x4 mvp;
	matrix_float4x4 model;
	vector_float4 base_color;
	vector_float4 light_dir;
};

struct GpuMeshAsset {
	id<MTLBuffer> vertex_buffer = nil;
	id<MTLBuffer> index_buffer = nil;
	std::vector<id<MTLTexture>> textures;
};

static NSString *const kShaderSource = @R"(
#include <metal_stdlib>
using namespace metal;

struct ShaderVertex {
	float3 position;
	float3 normal;
	float2 uv;
};

struct FrameUniforms {
	float4x4 mvp;
	float4x4 model;
	float4 base_color;
	float4 light_dir;
};

struct VertexOut {
	float4 position [[position]];
	float3 normal;
	float2 uv;
};

vertex VertexOut orbiter_vertex(
	uint vertex_id [[vertex_id]],
	const device ShaderVertex *vertices [[buffer(0)]],
	constant FrameUniforms &uniforms [[buffer(1)]])
{
	VertexOut out;
	const ShaderVertex v = vertices[vertex_id];
	out.position = uniforms.mvp * float4(v.position, 1.0);
	out.normal = normalize((uniforms.model * float4(v.normal, 0.0)).xyz);
	out.uv = float2(v.uv.x, 1.0 - v.uv.y);
	return out;
}

fragment float4 orbiter_fragment(
	VertexOut in [[stage_in]],
	texture2d<float> diffuse_map [[texture(0)]],
	sampler diffuse_sampler [[sampler(0)]],
	constant FrameUniforms &uniforms [[buffer(0)]])
{
	const float4 texel = diffuse_map.sample(diffuse_sampler, in.uv);
	const float alpha = texel.a * uniforms.base_color.a;
	const float3 albedo = texel.rgb * uniforms.base_color.rgb;
	const float3 normal = normalize(in.normal);
	const float3 light = normalize(uniforms.light_dir.xyz);
	const float diffuse = max(dot(normal, light), 0.0);
	const float lighting = 0.22 + diffuse * 0.78;
	return float4(albedo * lighting, alpha);
}
)";

matrix_float4x4 identity_matrix()
{
	return matrix_identity_float4x4;
}

matrix_float4x4 translation_matrix(float x, float y, float z)
{
	matrix_float4x4 matrix = identity_matrix();
	matrix.columns[3] = {x, y, z, 1.0f};
	return matrix;
}

matrix_float4x4 scale_matrix(float x, float y, float z)
{
	matrix_float4x4 matrix = identity_matrix();
	matrix.columns[0][0] = x;
	matrix.columns[1][1] = y;
	matrix.columns[2][2] = z;
	return matrix;
}

matrix_float4x4 rotation_x_matrix(float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return {{
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, c, s, 0.0f},
		{0.0f, -s, c, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	}};
}

matrix_float4x4 rotation_y_matrix(float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return {{
		{c, 0.0f, -s, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f},
		{s, 0.0f, c, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	}};
}

matrix_float4x4 rotation_z_matrix(float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return {{
		{c, s, 0.0f, 0.0f},
		{-s, c, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	}};
}

matrix_float4x4 perspective_matrix(float fov_y_radians, float aspect, float near_z, float far_z)
{
	const float ys = 1.0f / std::tan(fov_y_radians * 0.5f);
	const float xs = ys / aspect;
	const float zs = far_z / (near_z - far_z);
	return {{
		{xs, 0.0f, 0.0f, 0.0f},
		{0.0f, ys, 0.0f, 0.0f},
		{0.0f, 0.0f, zs, -1.0f},
		{0.0f, 0.0f, zs * near_z, 0.0f},
	}};
}

matrix_float4x4 look_at_matrix(vector_float3 eye, vector_float3 target, vector_float3 up)
{
	const vector_float3 z_axis = simd_normalize(eye - target);
	const vector_float3 x_axis = simd_normalize(simd_cross(up, z_axis));
	const vector_float3 y_axis = simd_cross(z_axis, x_axis);

	return {{
		{x_axis.x, y_axis.x, z_axis.x, 0.0f},
		{x_axis.y, y_axis.y, z_axis.y, 0.0f},
		{x_axis.z, y_axis.z, z_axis.z, 0.0f},
		{-simd_dot(x_axis, eye), -simd_dot(y_axis, eye), -simd_dot(z_axis, eye), 1.0f},
	}};
}

id<MTLTexture> create_solid_texture(id<MTLDevice> device, uint32_t rgba)
{
	MTLTextureDescriptor *descriptor =
		[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
														 width:2
														height:2
													 mipmapped:NO];
	id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
	if (!texture) {
		return nil;
	}

	const uint32_t pixels[4] = {rgba, rgba, rgba, rgba};
	[texture replaceRegion:MTLRegionMake2D(0, 0, 2, 2)
			   mipmapLevel:0
				 withBytes:pixels
			   bytesPerRow:sizeof(uint32_t) * 2];
	return texture;
}

matrix_float4x4 model_matrix_for_instance(const orbiter::runtime::MeshInstance &instance)
{
	const matrix_float4x4 translation = translation_matrix(
		static_cast<float>(instance.position.x),
		static_cast<float>(instance.position.y),
		static_cast<float>(instance.position.z));
	// Must match SimulationSession's rotate_euler_yxz: yaw first, then
	// pitch, then roll (R = Rz(roll) * Rx(pitch) * Ry(yaw)). Composing in a
	// different order renders the vessel at an attitude the physics never
	// had, which reads as "flying diagonally" while tilting.
	const matrix_float4x4 rotation =
		matrix_multiply(
			rotation_z_matrix(static_cast<float>(instance.rotation_degrees.z * kPi / 180.0)),
			matrix_multiply(
				rotation_x_matrix(static_cast<float>(instance.rotation_degrees.x * kPi / 180.0)),
				rotation_y_matrix(static_cast<float>(instance.rotation_degrees.y * kPi / 180.0))));
	const matrix_float4x4 scale = scale_matrix(
		static_cast<float>(instance.scale.x),
		static_cast<float>(instance.scale.y),
		static_cast<float>(instance.scale.z));
	return matrix_multiply(translation, matrix_multiply(rotation, scale));
}

bool build_gpu_mesh_assets(
	id<MTLDevice> device,
	MTKTextureLoader *texture_loader,
	const std::filesystem::path &textures_root,
	const orbiter::runtime::FrameScene &scene,
	std::vector<GpuMeshAsset> &gpu_assets,
	std::string &error)
{
	id<MTLTexture> placeholder_texture = create_solid_texture(device, 0xffffffffu);
	gpu_assets.clear();
	gpu_assets.resize(scene.assets.size());

	for (size_t asset_index = 0; asset_index < scene.assets.size(); ++asset_index) {
		const orbiter::runtime::MeshAsset &asset = scene.assets[asset_index];
		GpuMeshAsset &gpu_asset = gpu_assets[asset_index];
		std::vector<ShaderVertex> gpu_vertices(asset.mesh.vertices.size());
		for (size_t i = 0; i < asset.mesh.vertices.size(); ++i) {
			gpu_vertices[i].position = {
				asset.mesh.vertices[i].position[0],
				asset.mesh.vertices[i].position[1],
				asset.mesh.vertices[i].position[2],
			};
			gpu_vertices[i].normal = {
				asset.mesh.vertices[i].normal[0],
				asset.mesh.vertices[i].normal[1],
				asset.mesh.vertices[i].normal[2],
			};
			gpu_vertices[i].uv = {
				asset.mesh.vertices[i].uv[0],
				asset.mesh.vertices[i].uv[1],
			};
		}

		gpu_asset.vertex_buffer =
			[device newBufferWithBytes:gpu_vertices.data()
								length:sizeof(ShaderVertex) * gpu_vertices.size()
							   options:MTLResourceStorageModeShared];
		gpu_asset.index_buffer =
			[device newBufferWithBytes:asset.mesh.indices.data()
								length:sizeof(uint16_t) * asset.mesh.indices.size()
							   options:MTLResourceStorageModeShared];
		if (!gpu_asset.vertex_buffer || !gpu_asset.index_buffer) {
			error = "Failed to create Metal mesh buffers for " + asset.id;
			return false;
		}

		gpu_asset.textures.resize(std::max<size_t>(asset.mesh.textures.size(), 1u), placeholder_texture);
		for (size_t i = 0; i < asset.mesh.textures.size(); ++i) {
			const std::filesystem::path texture_path =
				orbiter::runtime::resolve_orbiter_asset_path(textures_root, asset.mesh.textures[i]);
			if (texture_path.empty() || !std::filesystem::exists(texture_path)) {
				continue;
			}

			NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:texture_path.string().c_str()]];
			NSDictionary *options_dict = @{
				MTKTextureLoaderOptionSRGB : @NO,
				MTKTextureLoaderOptionGenerateMipmaps : @YES
			};
			NSError *texture_error = nil;
			id<MTLTexture> texture = [texture_loader newTextureWithContentsOfURL:url
																	 options:options_dict
																	   error:&texture_error];
			if (!texture) {
				continue;
			}
			gpu_asset.textures[i] = texture;
		}
	}

	return true;
}

void encode_scene_frame(
	id<MTLRenderCommandEncoder> encoder,
	const orbiter::runtime::FrameScene &scene,
	const std::vector<GpuMeshAsset> &gpu_assets,
	id<MTLRenderPipelineState> opaque_pipeline_state,
	id<MTLRenderPipelineState> transparent_pipeline_state,
	id<MTLDepthStencilState> opaque_depth_state,
	id<MTLDepthStencilState> transparent_depth_state,
	id<MTLSamplerState> sampler_state,
	int width,
	int height)
{
	[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
	[encoder setCullMode:MTLCullModeBack];
	[encoder setFragmentSamplerState:sampler_state atIndex:0];

	const float aspect = std::max(width, 1) / static_cast<float>(std::max(height, 1));
	const float fov = scene.camera.fov_degrees * kPi / 180.0f;
	const matrix_float4x4 view = look_at_matrix(
		(vector_float3){
			static_cast<float>(scene.camera.position.x),
			static_cast<float>(scene.camera.position.y),
			static_cast<float>(scene.camera.position.z)},
		(vector_float3){
			static_cast<float>(scene.camera.target.x),
			static_cast<float>(scene.camera.target.y),
			static_cast<float>(scene.camera.target.z)},
		(vector_float3){0.0f, 1.0f, 0.0f});
	const matrix_float4x4 projection = perspective_matrix(fov, aspect, 0.1f, 30000.0f);

	auto draw_instance = ^(const orbiter::runtime::MeshInstance &instance) {
		if (instance.asset_index >= scene.assets.size() || instance.asset_index >= gpu_assets.size()) {
			return;
		}

		const orbiter::runtime::MeshAsset &asset = scene.assets[instance.asset_index];
		const GpuMeshAsset &gpu_asset = gpu_assets[instance.asset_index];
		const matrix_float4x4 model = model_matrix_for_instance(instance);
		const matrix_float4x4 mvp = matrix_multiply(projection, matrix_multiply(view, model));

		[encoder setVertexBuffer:gpu_asset.vertex_buffer offset:0 atIndex:0];
		for (const orbiter::runtime::MeshSubmesh &submesh : asset.mesh.submeshes) {
			FrameUniforms uniforms{};
			uniforms.mvp = mvp;
			uniforms.model = model;
			uniforms.base_color = {
				instance.color.r,
				instance.color.g,
				instance.color.b,
				instance.color.a,
			};
			if (submesh.material_index >= 0 &&
				submesh.material_index < static_cast<int>(asset.mesh.materials.size())) {
				const orbiter::runtime::MeshMaterial &material =
					asset.mesh.materials[static_cast<size_t>(submesh.material_index)];
				uniforms.base_color.x *= material.diffuse[0];
				uniforms.base_color.y *= material.diffuse[1];
				uniforms.base_color.z *= material.diffuse[2];
				uniforms.base_color.w *= material.diffuse[3];
			}
			uniforms.light_dir = simd_normalize((vector_float4){0.45f, 0.85f, 0.25f, 0.0f});

			id<MTLTexture> texture = gpu_asset.textures.empty() ? nil : gpu_asset.textures.front();
			if (submesh.texture_index >= 0 &&
				submesh.texture_index < static_cast<int>(gpu_asset.textures.size())) {
				texture = gpu_asset.textures[static_cast<size_t>(submesh.texture_index)];
			}

			[encoder setRenderPipelineState:submesh.transparent ? transparent_pipeline_state : opaque_pipeline_state];
			[encoder setDepthStencilState:submesh.transparent ? transparent_depth_state : opaque_depth_state];
			[encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
			[encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
			[encoder setFragmentTexture:texture atIndex:0];
			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
					indexCount:submesh.index_count
					 indexType:MTLIndexTypeUInt16
					indexBuffer:gpu_asset.index_buffer
			   indexBufferOffset:submesh.index_offset * sizeof(uint16_t)];
		}
	};

	for (const orbiter::runtime::TerrainPatch &patch : scene.terrain_patches) {
		draw_instance(patch.mesh);
	}
	for (const orbiter::runtime::BaseInstance &base : scene.base_instances) {
		draw_instance(base.mesh);
	}
	for (const orbiter::runtime::MeshInstance &vessel : scene.vessel_instances) {
		draw_instance(vessel);
	}
}

// Write an RGBA byte buffer to a PNG file using zlib deflate.
bool write_snapshot_png(const std::filesystem::path &path, int width, int height, const uint8_t *rgba)
{
	auto be32 = [](uint32_t v) {
		uint8_t b[4] = {
			static_cast<uint8_t>((v >> 24) & 0xff),
			static_cast<uint8_t>((v >> 16) & 0xff),
			static_cast<uint8_t>((v >> 8) & 0xff),
			static_cast<uint8_t>(v & 0xff),
		};
		return std::vector<uint8_t>(b, b + 4);
	};
	auto chunk = [&](const char *type, const std::vector<uint8_t> &payload) {
		std::vector<uint8_t> out;
		const std::vector<uint8_t> length_field = be32(static_cast<uint32_t>(payload.size()));
		out.insert(out.end(), length_field.begin(), length_field.end());
		const size_t type_start = out.size();
		out.insert(out.end(), type, type + 4);
		out.insert(out.end(), payload.begin(), payload.end());
		const std::vector<uint8_t> crc_input(out.begin() + type_start, out.end());
		const uint32_t crc = static_cast<uint32_t>(crc32(0, crc_input.data(), static_cast<uInt>(crc_input.size())));
		const std::vector<uint8_t> crc_field = be32(crc);
		out.insert(out.end(), crc_field.begin(), crc_field.end());
		return out;
	};

	std::vector<uint8_t> raw;
	raw.reserve(static_cast<size_t>(height) * (1 + static_cast<size_t>(width) * 4));
	for (int y = 0; y < height; ++y) {
		raw.push_back(0); // filter: none
		const size_t row_start = static_cast<size_t>(y) * static_cast<size_t>(width) * 4;
		raw.insert(raw.end(), rgba + row_start, rgba + row_start + static_cast<size_t>(width) * 4);
	}

	uLong compressed_bound = compressBound(static_cast<uLong>(raw.size()));
	std::vector<uint8_t> compressed(compressed_bound);
	if (compress2(compressed.data(), &compressed_bound, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK) {
		return false;
	}
	compressed.resize(compressed_bound);

	std::vector<uint8_t> png;
	png.insert(png.end(), {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a});
	std::vector<uint8_t> ihdr;
	const std::vector<uint8_t> w_field = be32(static_cast<uint32_t>(width));
	const std::vector<uint8_t> h_field = be32(static_cast<uint32_t>(height));
	ihdr.insert(ihdr.end(), w_field.begin(), w_field.end());
	ihdr.insert(ihdr.end(), h_field.begin(), h_field.end());
	ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0}); // bit depth 8, color type RGBA
	const std::vector<uint8_t> ihdr_chunk = chunk("IHDR", ihdr);
	png.insert(png.end(), ihdr_chunk.begin(), ihdr_chunk.end());
	const std::vector<uint8_t> idat_chunk = chunk("IDAT", compressed);
	png.insert(png.end(), idat_chunk.begin(), idat_chunk.end());
	const std::vector<uint8_t> iend_chunk = chunk("IEND", {});
	png.insert(png.end(), iend_chunk.begin(), iend_chunk.end());

	std::ofstream out(path, std::ios::binary);
	if (!out) {
		return false;
	}
	out.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
	return out.good();
}

} // namespace

int run_app(const orbiter::runtime::LaunchOptions &options)
{
	@autoreleasepool {
		std::string error;
		std::unique_ptr<orbiter::runtime::Session> session = orbiter::runtime::create_session(options, error);
		if (!session) {
			std::cerr << error << '\n';
			return 20;
		}

		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
			std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
			return 10;
		}

		SDL_Window *window = SDL_CreateWindow(
			"Orbiter macOS",
			1440,
			900,
			SDL_WINDOW_RESIZABLE | SDL_WINDOW_METAL);
		if (!window) {
			std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
			SDL_Quit();
			return 11;
		}

		SDL_MetalView metal_view = SDL_Metal_CreateView(window);
		if (!metal_view) {
			std::cerr << "SDL_Metal_CreateView failed: " << SDL_GetError() << '\n';
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 12;
		}

		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		if (!device) {
			std::cerr << "Metal is not available on this system\n";
			SDL_Metal_DestroyView(metal_view);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 13;
		}

		CAMetalLayer *layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(metal_view);
		layer.device = device;
		layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		layer.framebufferOnly = YES;

		NSError *shader_error = nil;
		id<MTLLibrary> shader_library = [device newLibraryWithSource:kShaderSource
															 options:nil
															   error:&shader_error];
		if (!shader_library) {
			std::cerr << "Failed to compile Metal shaders: "
					  << [[shader_error localizedDescription] UTF8String] << '\n';
			SDL_Metal_DestroyView(metal_view);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 15;
		}

		id<MTLFunction> vertex_function = [shader_library newFunctionWithName:@"orbiter_vertex"];
		id<MTLFunction> fragment_function = [shader_library newFunctionWithName:@"orbiter_fragment"];
		if (!vertex_function || !fragment_function) {
			std::cerr << "Failed to look up Metal shader entry points\n";
			SDL_Metal_DestroyView(metal_view);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 16;
		}

		MTLRenderPipelineDescriptor *opaque_pipeline_descriptor = [[MTLRenderPipelineDescriptor alloc] init];
		opaque_pipeline_descriptor.vertexFunction = vertex_function;
		opaque_pipeline_descriptor.fragmentFunction = fragment_function;
		opaque_pipeline_descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		opaque_pipeline_descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

		id<MTLRenderPipelineState> opaque_pipeline_state =
			[device newRenderPipelineStateWithDescriptor:opaque_pipeline_descriptor error:&shader_error];
		if (!opaque_pipeline_state) {
			std::cerr << "Failed to create opaque Metal pipeline: "
					  << [[shader_error localizedDescription] UTF8String] << '\n';
			SDL_Metal_DestroyView(metal_view);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 17;
		}

		MTLRenderPipelineDescriptor *transparent_pipeline_descriptor = [[MTLRenderPipelineDescriptor alloc] init];
		transparent_pipeline_descriptor.vertexFunction = vertex_function;
		transparent_pipeline_descriptor.fragmentFunction = fragment_function;
		transparent_pipeline_descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		transparent_pipeline_descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		transparent_pipeline_descriptor.colorAttachments[0].blendingEnabled = YES;
		transparent_pipeline_descriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
		transparent_pipeline_descriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
		transparent_pipeline_descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
		transparent_pipeline_descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
		transparent_pipeline_descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		transparent_pipeline_descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

		id<MTLRenderPipelineState> transparent_pipeline_state =
			[device newRenderPipelineStateWithDescriptor:transparent_pipeline_descriptor error:&shader_error];
		if (!transparent_pipeline_state) {
			std::cerr << "Failed to create transparent Metal pipeline: "
					  << [[shader_error localizedDescription] UTF8String] << '\n';
			SDL_Metal_DestroyView(metal_view);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 18;
		}

		MTLDepthStencilDescriptor *depth_descriptor = [[MTLDepthStencilDescriptor alloc] init];
		depth_descriptor.depthCompareFunction = MTLCompareFunctionLess;
		depth_descriptor.depthWriteEnabled = YES;
		id<MTLDepthStencilState> opaque_depth_state = [device newDepthStencilStateWithDescriptor:depth_descriptor];

		MTLDepthStencilDescriptor *transparent_depth_descriptor = [[MTLDepthStencilDescriptor alloc] init];
		transparent_depth_descriptor.depthCompareFunction = MTLCompareFunctionLess;
		transparent_depth_descriptor.depthWriteEnabled = NO;
		id<MTLDepthStencilState> transparent_depth_state =
			[device newDepthStencilStateWithDescriptor:transparent_depth_descriptor];

		MTLSamplerDescriptor *sampler_descriptor = [[MTLSamplerDescriptor alloc] init];
		sampler_descriptor.minFilter = MTLSamplerMinMagFilterLinear;
		sampler_descriptor.magFilter = MTLSamplerMinMagFilterLinear;
		sampler_descriptor.mipFilter = MTLSamplerMipFilterLinear;
		sampler_descriptor.sAddressMode = MTLSamplerAddressModeRepeat;
		sampler_descriptor.tAddressMode = MTLSamplerAddressModeRepeat;
		id<MTLSamplerState> sampler_state = [device newSamplerStateWithDescriptor:sampler_descriptor];

		// HUD (ImGui) state for the live window only.
		bool hud_ready = false;
		bool hud_visible = true;
		bool launcher_visible = false;
		std::string imgui_ini_path;
		char scenario_filter_buf[128] = {0};
		std::vector<std::pair<std::string, std::filesystem::path>> scenario_list =
			orbiter::runtime::discover_scenarios(options.content_root);
		std::filesystem::path current_scenario = options.scenario_path;
		bool scenario_switch_requested = false;
		std::filesystem::path pending_scenario;

		MTKTextureLoader *texture_loader = [[MTKTextureLoader alloc] initWithDevice:device];
		std::vector<GpuMeshAsset> gpu_assets;
		if (!build_gpu_mesh_assets(
				device,
				texture_loader,
				options.content_root / "Textures",
				session->frame_scene(),
				gpu_assets,
				error)) {
			std::cerr << error << '\n';
			SDL_Metal_DestroyView(metal_view);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 19;
		}

		id<MTLCommandQueue> command_queue = [device newCommandQueue];
		if (!command_queue) {
			std::cerr << "Failed to create Metal command queue\n";
			SDL_Metal_DestroyView(metal_view);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 14;
		}

		std::cout
			<< "Orbiter_macOS session\n"
			<< "Content root: " << options.content_root << '\n'
			<< "Scenario: " << options.scenario_path << '\n'
			<< "User data: " << options.user_data_root << '\n';
		for (const std::string &warning : session->warnings()) {
			std::cout << "Warning: " << warning << '\n';
		}

		if (options.snapshot_path.empty()) {
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO &imgui_io = ImGui::GetIO();
			imgui_io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			const std::filesystem::path imgui_ini = options.user_data_root / "imgui.ini";
			imgui_ini_path = imgui_ini.string();
			imgui_io.IniFilename = imgui_ini_path.c_str();
			ImGui::StyleColorsDark();
			if (ImGui_ImplSDL3_InitForMetal(window) && ImGui_ImplMetal_Init(device)) {
				hud_ready = true;
			} else {
				std::cerr << "ImGui init failed; running without HUD\n";
			}
		}

		if (!options.snapshot_path.empty()) {
			// Headless snapshot: render one frame into an offscreen texture and
			// write it out as PNG. Works without a visible window surface.
			const int snapshot_width = 1280;
			const int snapshot_height = 800;

			session->step(0.016, orbiter::runtime::ControlInput{});
			const orbiter::runtime::FrameScene &scene = session->frame_scene();

			MTLTextureDescriptor *color_descriptor =
				[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
					width:snapshot_width
					height:snapshot_height
					mipmapped:NO];
			color_descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
			color_descriptor.storageMode = MTLStorageModeShared;
			id<MTLTexture> color_texture = [device newTextureWithDescriptor:color_descriptor];

			MTLTextureDescriptor *snapshot_depth_descriptor =
				[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
					width:snapshot_width
					height:snapshot_height
					mipmapped:NO];
			snapshot_depth_descriptor.storageMode = MTLStorageModePrivate;
			snapshot_depth_descriptor.usage = MTLTextureUsageRenderTarget;
			id<MTLTexture> snapshot_depth = [device newTextureWithDescriptor:snapshot_depth_descriptor];

			if (!color_texture || !snapshot_depth) {
				std::cerr << "Failed to create snapshot render targets\n";
				SDL_Metal_DestroyView(metal_view);
				SDL_DestroyWindow(window);
				SDL_Quit();
				return 21;
			}

			MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
			pass.colorAttachments[0].texture = color_texture;
			pass.colorAttachments[0].loadAction = MTLLoadActionClear;
			pass.colorAttachments[0].storeAction = MTLStoreActionStore;
			pass.colorAttachments[0].clearColor = MTLClearColorMake(0.47, 0.62, 0.82, 1.0);
			pass.depthAttachment.texture = snapshot_depth;
			pass.depthAttachment.loadAction = MTLLoadActionClear;
			pass.depthAttachment.storeAction = MTLStoreActionDontCare;
			pass.depthAttachment.clearDepth = 1.0;

			id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
			id<MTLRenderCommandEncoder> encoder = [command_buffer renderCommandEncoderWithDescriptor:pass];
			encode_scene_frame(
				encoder,
				scene,
				gpu_assets,
				opaque_pipeline_state,
				transparent_pipeline_state,
				opaque_depth_state,
				transparent_depth_state,
				sampler_state,
				snapshot_width,
				snapshot_height);
			[encoder endEncoding];
			[command_buffer commit];
			[command_buffer waitUntilCompleted];

			std::vector<uint8_t> pixels(static_cast<size_t>(snapshot_width) * snapshot_height * 4);
			MTLRegion region = MTLRegionMake2D(0, 0, snapshot_width, snapshot_height);
			[color_texture getBytes:pixels.data()
				bytesPerRow:static_cast<NSUInteger>(snapshot_width) * 4
				fromRegion:region
				mipmapLevel:0];

			// BGRA -> RGBA swap for the PNG writer.
			for (size_t i = 0; i < pixels.size(); i += 4) {
				std::swap(pixels[i + 0], pixels[i + 2]);
			}

			if (!write_snapshot_png(options.snapshot_path, snapshot_width, snapshot_height, pixels.data())) {
				std::cerr << "Failed to write snapshot: " << options.snapshot_path << '\n';
				SDL_Metal_DestroyView(metal_view);
				SDL_DestroyWindow(window);
				SDL_Quit();
				return 22;
			}

			std::cout << "Snapshot written: " << options.snapshot_path << '\n';
			SDL_Metal_DestroyView(metal_view);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 0;
		}

		id<MTLTexture> depth_texture = nil;
		bool running = true;
		bool dragging = false;
		double camera_yaw_delta = 0.0;
		double camera_pitch_delta = 0.0;
		double camera_zoom_delta = 0.0;
		int frames = 0;
		uint64_t previous_ticks = SDL_GetTicks();

		while (running && (options.max_frames == 0 || frames < options.max_frames)) {
			orbiter::runtime::ControlInput input{};
			bool imgui_want_mouse = false;
			bool imgui_want_keyboard = false;

			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				if (hud_ready) {
					ImGui_ImplSDL3_ProcessEvent(&event);
				}
				if (hud_ready) {
					const ImGuiIO &io = ImGui::GetIO();
					imgui_want_mouse = io.WantCaptureMouse;
					imgui_want_keyboard = io.WantCaptureKeyboard;
				}
				if (event.type == SDL_EVENT_QUIT) {
					running = false;
				} else if (event.type == SDL_EVENT_KEY_DOWN &&
					   !event.key.repeat &&
					   event.key.scancode == SDL_SCANCODE_P) {
					input.pause_toggle = true;
				} else if (event.type == SDL_EVENT_KEY_DOWN &&
					   !event.key.repeat &&
					   event.key.scancode == SDL_SCANCODE_H) {
					hud_visible = !hud_visible;
				} else if (event.type == SDL_EVENT_KEY_DOWN &&
					   !event.key.repeat &&
					   event.key.scancode == SDL_SCANCODE_L) {
					launcher_visible = !launcher_visible;
				} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
					   event.button.button == SDL_BUTTON_LEFT &&
					   !imgui_want_mouse) {
					dragging = true;
				} else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
					   event.button.button == SDL_BUTTON_LEFT) {
					dragging = false;
				} else if (event.type == SDL_EVENT_MOUSE_MOTION && dragging && !imgui_want_mouse) {
					camera_yaw_delta += -event.motion.xrel * 0.004;
					camera_pitch_delta += -event.motion.yrel * 0.004;
				} else if (event.type == SDL_EVENT_MOUSE_WHEEL && !imgui_want_mouse) {
					camera_zoom_delta += -event.wheel.y * 4.0;
				}
			}

			const bool *keyboard = SDL_GetKeyboardState(nullptr);
			input.thrust = 0.0;
			if (!imgui_want_keyboard) {
			if (keyboard[SDL_SCANCODE_W]) {
				input.thrust += 1.0;
			}
			if (keyboard[SDL_SCANCODE_S]) {
				input.thrust -= 1.0;
			}
			if (keyboard[SDL_SCANCODE_UP]) {
				input.pitch -= 1.0;
			}
			if (keyboard[SDL_SCANCODE_DOWN]) {
				input.pitch += 1.0;
			}
			if (keyboard[SDL_SCANCODE_LEFT]) {
				input.roll -= 1.0;
			}
			if (keyboard[SDL_SCANCODE_RIGHT]) {
				input.roll += 1.0;
			}
			if (keyboard[SDL_SCANCODE_A]) {
				input.yaw -= 1.0;
			}
			if (keyboard[SDL_SCANCODE_D]) {
				input.yaw += 1.0;
			}
			input.hover_hold = keyboard[SDL_SCANCODE_SPACE];
			}
			input.camera_yaw_delta = camera_yaw_delta;
			input.camera_pitch_delta = camera_pitch_delta;
			input.camera_zoom_delta = camera_zoom_delta;
			camera_yaw_delta = 0.0;
			camera_pitch_delta = 0.0;
			camera_zoom_delta = 0.0;

			const uint64_t now_ticks = SDL_GetTicks();
			const double dt = std::max(0.001, static_cast<double>(now_ticks - previous_ticks) / 1000.0);
			previous_ticks = now_ticks;
			session->step(dt, input);

			if (scenario_switch_requested) {
				scenario_switch_requested = false;
				std::string switch_error;
				if (session->load_scenario(pending_scenario, switch_error)) {
					current_scenario = pending_scenario;
					std::string rebuild_error;
					if (!build_gpu_mesh_assets(
							device,
							texture_loader,
							options.content_root / "Textures",
							session->frame_scene(),
							gpu_assets,
							rebuild_error)) {
						std::cerr << "Scenario switch mesh rebuild failed: " << rebuild_error << '\n';
					}
				} else {
					std::cerr << "Scenario switch failed: " << switch_error << '\n';
				}
			}

			const orbiter::runtime::FrameScene &scene = session->frame_scene();
			const orbiter::runtime::VesselState &vessel = session->focus_vessel();

			int width = 0;
			int height = 0;
			SDL_GetWindowSizeInPixels(window, &width, &height);
			layer.drawableSize = CGSizeMake(width, height);
			if (!depth_texture ||
				depth_texture.width != static_cast<NSUInteger>(width) ||
				depth_texture.height != static_cast<NSUInteger>(height)) {
				MTLTextureDescriptor *depth_texture_descriptor =
					[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
							width:std::max(width, 1)
							height:std::max(height, 1)
						mipmapped:NO];
				depth_texture_descriptor.storageMode = MTLStorageModePrivate;
				depth_texture_descriptor.usage = MTLTextureUsageRenderTarget;
				depth_texture = [device newTextureWithDescriptor:depth_texture_descriptor];
			}

			id<CAMetalDrawable> drawable = [layer nextDrawable];
			if (drawable) {
				MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
				pass.colorAttachments[0].texture = drawable.texture;
				pass.colorAttachments[0].loadAction = MTLLoadActionClear;
				pass.colorAttachments[0].storeAction = MTLStoreActionStore;
				pass.colorAttachments[0].clearColor = MTLClearColorMake(0.47, 0.62, 0.82, 1.0);
				pass.depthAttachment.texture = depth_texture;
				pass.depthAttachment.loadAction = MTLLoadActionClear;
				pass.depthAttachment.storeAction = MTLStoreActionDontCare;
				pass.depthAttachment.clearDepth = 1.0;

				id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
				id<MTLRenderCommandEncoder> encoder = [command_buffer renderCommandEncoderWithDescriptor:pass];
				encode_scene_frame(
					encoder,
					scene,
					gpu_assets,
					opaque_pipeline_state,
					transparent_pipeline_state,
					opaque_depth_state,
					transparent_depth_state,
					sampler_state,
					width,
					height);

				if (hud_ready && hud_visible) {
					ImGui_ImplMetal_NewFrame(pass);
					ImGui_ImplSDL3_NewFrame();
					ImGui::NewFrame();

					ImGui::Begin("Orbiter");
					ImGui::Text("%s (%s)", vessel.name.c_str(), vessel.class_name.c_str());
					ImGui::Text("Altitude  %.1f m", vessel.position.y);
					const double speed = std::sqrt(
						vessel.velocity.x * vessel.velocity.x +
						vessel.velocity.y * vessel.velocity.y +
						vessel.velocity.z * vessel.velocity.z);
					ImGui::Text("Speed     %.1f m/s", speed);
					ImGui::Text("Throttle  %.0f%%", vessel.throttle * 100.0);
					ImGui::Text("Time      %.1f s", vessel.simulation_time);
					ImGui::Separator();
					ImGui::TextUnformatted("W/S thrust  A/D yaw  arrows pitch/roll");
					ImGui::TextUnformatted("Space hover-hold  P pause  H toggle HUD");
					ImGui::TextUnformatted("Drag mouse orbit camera  wheel zoom");
					ImGui::TextUnformatted("L scenario launcher");
					ImGui::End();

					if (launcher_visible) {
						ImGui::Begin("Scenarios");
						ImGui::InputTextWithHint("Filter", "search scenarios...", scenario_filter_buf,
						sizeof(scenario_filter_buf));
						ImGui::Separator();
						ImGui::BeginChild("scenario_list", ImVec2(0, 380), true);
						const std::string lower_filter = [&]() {
							std::string out = scenario_filter_buf;
							std::transform(out.begin(), out.end(), out.begin(),
								[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
							return out;
						}();
						for (const auto &entry : scenario_list) {
							if (!lower_filter.empty()) {
								std::string lower_name = entry.first;
								std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
									[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
								if (lower_name.find(lower_filter) == std::string::npos) {
									continue;
								}
							}
							const bool is_current = entry.second == current_scenario;
							if (is_current) {
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.9f, 1.0f, 1.0f));
							}
							if (ImGui::Selectable(entry.first.c_str(), is_current)) {
								pending_scenario = entry.second;
								scenario_switch_requested = true;
							}
							if (is_current) {
								ImGui::PopStyleColor();
							}
						}
						ImGui::EndChild();
						ImGui::Text("%zu scenarios", scenario_list.size());
						ImGui::End();
					}

					ImGui::Render();
					ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), command_buffer, encoder);
				}

				[encoder endEncoding];
				[command_buffer presentDrawable:drawable];
				[command_buffer commit];
			}

			SDL_Delay(1);
			++frames;
		}

		SDL_Metal_DestroyView(metal_view);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	return 0;
}

} // namespace orbiter::macos
