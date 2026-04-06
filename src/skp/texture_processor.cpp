#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_FAILURE_STRINGS
#include "tinygltf/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "tinygltf/stb_image_resize2.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf/stb_image_write.h"

#include "texture_processor.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>

// 静态缓存，避免重复处理
static std::map<std::string, std::string> textureCache;

std::string TextureProcessor::ProcessTexture(const std::string& inputPath, int maxResolution) {
    if (textureCache.count(inputPath)) {
        return textureCache[inputPath];
    }

    int width, height, channels;
    unsigned char* data = stbi_load(inputPath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << inputPath << std::endl;
        return inputPath;
    }

    if (width <= maxResolution && height <= maxResolution) {
        stbi_image_free(data);
        textureCache[inputPath] = inputPath;
        return inputPath;
    }

    // 计算缩放比例
    float ratio = std::min((float)maxResolution / width, (float)maxResolution / height);
    int newWidth = (int)(width * ratio);
    int newHeight = (int)(height * ratio);

    std::vector<unsigned char> resizedData(newWidth * newHeight * channels);
    stbir_pixel_layout layout = (channels == 4) ? STBIR_RGBA : (channels == 3) ? STBIR_RGB : (channels == 2) ? STBIR_2CHANNEL : STBIR_1CHANNEL;
    stbir_resize_uint8_linear(data, width, height, 0, resizedData.data(), newWidth, newHeight, 0, layout);

    // 默认生成一个新的文件名，避免覆盖原始纹理（除非是在临时目录）
    std::string outputPath = inputPath;
    size_t dotPos = outputPath.find_last_of(".");
    if (dotPos != std::string::npos) {
        outputPath.insert(dotPos, "_resized");
    } else {
        outputPath += "_resized";
    }

    int success = 0;
    if (outputPath.find(".png") != std::string::npos || outputPath.find(".PNG") != std::string::npos) {
        success = stbi_write_png(outputPath.c_str(), newWidth, newHeight, channels, resizedData.data(), newWidth * channels);
    } else {
        // 其他格式默认写 JPG
        if (outputPath.find(".jpg") == std::string::npos && outputPath.find(".jpeg") == std::string::npos) {
            outputPath += ".jpg";
        }
        success = stbi_write_jpg(outputPath.c_str(), newWidth, newHeight, channels, resizedData.data(), 90);
    }

    stbi_image_free(data);

    if (success) {
        std::cout << "Resized texture: " << inputPath << " -> " << outputPath 
                  << " (" << width << "x" << height << " -> " << newWidth << "x" << newHeight << ")" << std::endl;
        textureCache[inputPath] = outputPath;
        return outputPath;
    } else {
        std::cerr << "Failed to write resized texture: " << outputPath << std::endl;
        return inputPath;
    }
}
