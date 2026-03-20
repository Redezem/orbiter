// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "OrbiterMacApp.h"
#include "OrbiterMesh.h"

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
	const float3 albedo = texel.rgb * uniforms.base_color.rgb;
	const float3 normal = normalize(in.normal);
	const float3 light = normalize(uniforms.light_dir.xyz);
	const float diffuse = max(dot(normal, light), 0.0);
	const float lighting = 0.22 + diffuse * 0.78;
	return float4(albedo * lighting, 1.0);
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

id<MTLTexture> create_placeholder_texture(id<MTLDevice> device)
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

	const uint32_t pixels[4] = {
		0xffd030d0u,
		0xff101010u,
		0xff101010u,
		0xffd030d0u,
	};
	[texture replaceRegion:MTLRegionMake2D(0, 0, 2, 2)
			   mipmapLevel:0
				 withBytes:pixels
			   bytesPerRow:sizeof(uint32_t) * 2];
	return texture;
}

} // namespace

int run_app(const orbiter::bootstrap::LaunchOptions &options)
{
	@autoreleasepool {
		const std::filesystem::path mesh_path =
			options.content_root / "Meshes" / "DG" / "deltaglider.msh";
		const std::filesystem::path textures_root =
			options.content_root / "Textures";

		LoadedMesh mesh;
		std::string mesh_error;
		if (!load_orbiter_mesh(mesh_path, mesh, mesh_error)) {
			std::cerr << mesh_error << '\n';
			return 20;
		}

		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
			std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
			return 10;
		}

		SDL_Window *window = SDL_CreateWindow(
			"Orbiter macOS Bootstrap",
			1280,
			720,
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

		std::vector<ShaderVertex> gpu_vertices(mesh.vertices.size());
		for (size_t i = 0; i < mesh.vertices.size(); ++i) {
			gpu_vertices[i].position = {
				mesh.vertices[i].position[0],
				mesh.vertices[i].position[1],
				mesh.vertices[i].position[2],
			};
			gpu_vertices[i].normal = {
				mesh.vertices[i].normal[0],
				mesh.vertices[i].normal[1],
				mesh.vertices[i].normal[2],
			};
			gpu_vertices[i].uv = {
				mesh.vertices[i].uv[0],
				mesh.vertices[i].uv[1],
			};
		}

		id<MTLBuffer> vertex_buffer =
			[device newBufferWithBytes:gpu_vertices.data()
								length:sizeof(ShaderVertex) * gpu_vertices.size()
							   options:MTLResourceStorageModeShared];
		id<MTLBuffer> index_buffer =
			[device newBufferWithBytes:mesh.indices.data()
								length:sizeof(uint16_t) * mesh.indices.size()
							   options:MTLResourceStorageModeShared];
		if (!vertex_buffer || !index_buffer) {
			std::cerr << "Failed to create Metal mesh buffers\n";
			SDL_Metal_DestroyView(metal_view);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return 19;
		}

		MTKTextureLoader *texture_loader = [[MTKTextureLoader alloc] initWithDevice:device];
		id<MTLTexture> placeholder_texture = create_placeholder_texture(device);
		std::vector<id<MTLTexture>> textures(mesh.textures.size(), placeholder_texture);
		for (size_t i = 0; i < mesh.textures.size(); ++i) {
			const std::filesystem::path texture_path =
				resolve_orbiter_asset_path(textures_root, mesh.textures[i]);
			if (texture_path.empty() || !std::filesystem::exists(texture_path)) {
				std::cerr << "Texture not found: " << mesh.textures[i] << '\n';
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
				std::cerr << "Failed to load texture " << texture_path
						  << ": " << [[texture_error localizedDescription] UTF8String] << '\n';
				continue;
			}
			textures[i] = texture;
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
			<< "Orbiter_macOS bootstrap\n"
			<< "Content root: " << options.content_root << '\n'
			<< "Scenario: " << options.scenario_path << '\n'
			<< "Mesh: " << mesh_path << '\n'
			<< "User data: " << options.user_data_root << '\n';

		id<MTLTexture> depth_texture = nil;
		bool running = true;
		int frames = 0;
		while (running && (options.max_frames == 0 || frames < options.max_frames)) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_EVENT_QUIT) {
					running = false;
				}
			}

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
				pass.colorAttachments[0].clearColor = MTLClearColorMake(0.02, 0.03, 0.05, 1.0);
				pass.depthAttachment.texture = depth_texture;
				pass.depthAttachment.loadAction = MTLLoadActionClear;
				pass.depthAttachment.storeAction = MTLStoreActionDontCare;
				pass.depthAttachment.clearDepth = 1.0;

				id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
				id<MTLRenderCommandEncoder> encoder = [command_buffer renderCommandEncoderWithDescriptor:pass];
				[encoder setRenderPipelineState:opaque_pipeline_state];
				[encoder setDepthStencilState:opaque_depth_state];
				[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
				[encoder setCullMode:MTLCullModeBack];
				[encoder setVertexBuffer:vertex_buffer offset:0 atIndex:0];
				[encoder setFragmentSamplerState:sampler_state atIndex:0];

				const float aspect = std::max(width, 1) / static_cast<float>(std::max(height, 1));
				const float fov = 55.0f * kPi / 180.0f;
				const float distance = mesh.radius * 2.4f;
				const float seconds = SDL_GetTicks() * 0.001f;

				const matrix_float4x4 model =
					matrix_multiply(
						rotation_y_matrix(seconds * 0.35f),
						matrix_multiply(
							rotation_x_matrix(-0.22f),
							translation_matrix(-mesh.center[0], -mesh.center[1], -mesh.center[2])));
				const matrix_float4x4 view =
					look_at_matrix(
						(vector_float3){0.0f, mesh.radius * 0.2f, distance},
						(vector_float3){0.0f, 0.0f, 0.0f},
						(vector_float3){0.0f, 1.0f, 0.0f});
				const matrix_float4x4 projection =
					perspective_matrix(fov, aspect, 0.1f, std::max(50.0f, mesh.radius * 12.0f));
				const matrix_float4x4 mvp = matrix_multiply(projection, matrix_multiply(view, model));

				auto draw_submesh = ^(const MeshSubmesh &submesh) {
					FrameUniforms uniforms{};
					uniforms.mvp = mvp;
					uniforms.model = model;
					uniforms.light_dir = simd_normalize((vector_float4){0.35f, 0.8f, 0.45f, 0.0f});
					if (submesh.material_index >= 0 &&
						submesh.material_index < static_cast<int>(mesh.materials.size())) {
						const MeshMaterial &material = mesh.materials[static_cast<size_t>(submesh.material_index)];
						uniforms.base_color = {
							material.diffuse[0],
							material.diffuse[1],
							material.diffuse[2],
							material.diffuse[3],
						};
					} else {
						uniforms.base_color = {1.0f, 1.0f, 1.0f, 1.0f};
					}

					id<MTLTexture> texture = placeholder_texture;
					if (submesh.texture_index >= 0 &&
						submesh.texture_index < static_cast<int>(textures.size()) &&
						textures[static_cast<size_t>(submesh.texture_index)]) {
						texture = textures[static_cast<size_t>(submesh.texture_index)];
					}

					[encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
					[encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
					[encoder setFragmentTexture:texture atIndex:0];
					[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
										indexCount:submesh.index_count
										 indexType:MTLIndexTypeUInt16
									   indexBuffer:index_buffer
								 indexBufferOffset:submesh.index_offset * sizeof(uint16_t)];
				};

				for (const MeshSubmesh &submesh : mesh.submeshes) {
					if (!submesh.transparent) {
						[encoder setRenderPipelineState:opaque_pipeline_state];
						[encoder setDepthStencilState:opaque_depth_state];
						draw_submesh(submesh);
					}
				}

				for (const MeshSubmesh &submesh : mesh.submeshes) {
					if (submesh.transparent) {
						[encoder setRenderPipelineState:transparent_pipeline_state];
						[encoder setDepthStencilState:transparent_depth_state];
						draw_submesh(submesh);
					}
				}
				[encoder endEncoding];
				[command_buffer presentDrawable:drawable];
				[command_buffer commit];
			}

			SDL_Delay(16);
			++frames;
		}

		SDL_Metal_DestroyView(metal_view);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	return 0;
}

} // namespace orbiter::macos
