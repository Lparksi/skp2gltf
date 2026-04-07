#ifndef KTX2_ENCODER_H
#define KTX2_ENCODER_H

#include <string>
#include <vector>

class Ktx2Encoder {
public:
    enum class Format {
        UASTC,
        ETC1S
    };
    
    struct Options {
        Format format = Format::UASTC;
        int quality = 128;
        int mipmaps = -1;
        bool generateMips = true;
    };
    
    static std::string EncodeToKtx2(
        const std::string& inputPath,
        const std::string& outputPath,
        const Options& options = Options()
    );
    
    static bool IsKtx2Supported();
    
    static std::string GetKtx2Extension() { return ".ktx2"; }
};

#endif
