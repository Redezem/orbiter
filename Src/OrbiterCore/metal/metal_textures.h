#pragma once
#import <Metal/Metal.h>
#include "texture_store.h"
namespace core_metal {
using MetalTextureStore=TextureStore<id<MTLTexture>>;
inline std::unique_ptr<MetalTextureStore> makeMetalTextures(id<MTLDevice> device,
    const char *highRoot,const char *root) {
    return std::make_unique<MetalTextureStore>(std::vector<std::string>{highRoot,root},
        [device](const orbiter::dds::Image &image)->id<MTLTexture> {
            auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                width:image.width height:image.height mipmapped:NO];
            descriptor.storageMode=MTLStorageModeShared;
            descriptor.usage=MTLTextureUsageShaderRead;
            id<MTLTexture> texture=[device newTextureWithDescriptor:descriptor];
            if(texture) [texture replaceRegion:MTLRegionMake2D(0,0,image.width,image.height) mipmapLevel:0
                withBytes:image.rgba.data() bytesPerRow:image.width*4];
            return texture;
        },256u*1024*1024,[](const id<MTLTexture> &texture) { return size_t(texture.allocatedSize); });
}
}
