#ifndef TEXTURE_PROCESSOR_H
#define TEXTURE_PROCESSOR_H

#include <string>

class TextureProcessor {
public:
    static std::string ProcessTexture(const std::string& inputPath, int maxResolution = 1024);
};

#endif // TEXTURE_PROCESSOR_H
