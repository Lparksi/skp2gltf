#include "ktx2_encoder.h"
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <array>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

static std::string execCommand(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), (int)buffer.size(), pipe)) {
        result += buffer.data();
    }
    PCLOSE(pipe);
    return result;
}

bool Ktx2Encoder::IsKtx2Supported() {
    std::string result = execCommand("toktx --version 2>&1");
    return !result.empty() && result.find("toktx") != std::string::npos;
}

std::string Ktx2Encoder::EncodeToKtx2(
    const std::string& inputPath,
    const std::string& outputPath,
    const Options& options
) {
    if (!IsKtx2Supported()) {
        std::cerr << "toktx not found, skipping KTX2 encoding" << std::endl;
        return inputPath;
    }

    std::string formatFlag;
    std::string qualityFlag;
    
    if (options.format == Format::UASTC) {
        formatFlag = "--uastc 1";
        qualityFlag = "--uastc_quality " + std::to_string(std::min(4, options.quality / 32));
    } else {
        formatFlag = "--bcmp";
        qualityFlag = "--qlevel " + std::to_string(std::min(255, options.quality));
    }

    std::string mipmapFlag;
    if (options.generateMips) {
        mipmapFlag = "--genmipmap";
    }

    std::string cmd = "toktx " + formatFlag + " " + qualityFlag + " " + mipmapFlag + 
                      " --assign_oetf linear --convert_to_oetf srgb \"" + 
                      outputPath + "\" \"" + inputPath + "\" 2>&1";
    
    std::string result = execCommand(cmd);
    
    FILE* check = fopen(outputPath.c_str(), "rb");
    if (check) {
        fclose(check);
        std::cout << "KTX2 encoded: " << outputPath << std::endl;
        return outputPath;
    }
    
    std::cerr << "KTX2 encoding failed: " << result << std::endl;
    return inputPath;
}
