#ifndef NOMINMAX
#define NOMINMAX
#endif
/*
 * @Author: yaol 
 * @Date: 2025-02-18 17:26:05 
 * @Last Modified by:   yaol 
 * @Last Modified time: 2025-02-18 17:26:05 
 */

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <SketchUpAPI/sketchup.h>

#include <memory>
#include <vector>
#include <cstdarg>
#include <string>
#include <algorithm>
#include <cctype>
#include "skp/skp_exporter.h"

static std::string ToLowerCopy(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

static bool EndsWithIgnoreCase(const std::string &value, const std::string &suffix)
{
    if (value.size() < suffix.size())
    {
        return false;
    }

    const size_t offset = value.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(value[offset + i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i])))
        {
            return false;
        }
    }
    return true;
}

double getRatio(SUModelUnits units)
{
    switch (units)
    {
        case SUModelUnits_Inches:
            return 0.0254;
        case SUModelUnits_Feet:
            return 0.3048;
        case SUModelUnits_Millimeters:
            return 0.001;
        case SUModelUnits_Centimeters:
            return 0.01;
        case SUModelUnits_Meters:
            return 1;
        default:
            return 1;
    }
}
int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "Usage: skp2gltf.exe <input.skp> <output_dir> <output_name_or_path> [options]" << std::endl;
        std::cerr << "Options:" << std::endl;
        std::cerr << "  format:<gltf|glb>      Output format (default: glb)" << std::endl;
        std::cerr << "  draco                  Enable Draco compression" << std::endl;
        std::cerr << "  draco-speed:<N>        Draco encoding speed 0-10 (default: 5, lower=smaller)" << std::endl;
        std::cerr << "  draco-pos:<N>          Draco position quantization bits (default: 14)" << std::endl;
        std::cerr << "  draco-tex:<N>          Draco texcoord quantization bits (default: 12)" << std::endl;
        std::cerr << "  draco-norm:<N>         Draco normal quantization bits (default: 10)" << std::endl;
        std::cerr << "  tex-res:<N>            Max texture resolution (default: 1024)" << std::endl;
        std::cerr << "  ktx2                   Enable KTX2/Basis texture compression" << std::endl;
        std::cerr << "  ktx2-quality:<N>       KTX2 quality 1-255 (default: 128)" << std::endl;
        std::cerr << "  ktx2-uastc             Use UASTC format (default), otherwise ETC1S" << std::endl;
        return 2;
    }

    std::string skp_file   = argv[1];
    std::string output_dir = argv[2];
    std::string output_arg = argv[3];
    std::string output_format = "glb";
    bool use_draco = false;

    CSkpExporter cSkpExporter;

    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        std::string argLower = ToLowerCopy(arg);
        
        if (argLower == "draco" || argLower == "true" || argLower == "--draco") {
            use_draco = true;
        } else if (argLower == "gltf" || argLower == "glb") {
            output_format = argLower;
        } else if (argLower.find("draco-speed:") == 0) {
            int val = std::stoi(arg.substr(12));
            cSkpExporter.options().set_draco_speed(std::max(0, std::min(10, val)));
        } else if (argLower.find("draco-pos:") == 0) {
            int val = std::stoi(arg.substr(10));
            cSkpExporter.options().set_draco_position_bits(std::max(8, std::min(24, val)));
        } else if (argLower.find("draco-tex:") == 0) {
            int val = std::stoi(arg.substr(10));
            cSkpExporter.options().set_draco_tex_bits(std::max(8, std::min(16, val)));
        } else if (argLower.find("draco-norm:") == 0) {
            int val = std::stoi(arg.substr(11));
            cSkpExporter.options().set_draco_normal_bits(std::max(6, std::min(16, val)));
        } else if (argLower.find("tex-res:") == 0) {
            int val = std::stoi(arg.substr(8));
            cSkpExporter.options().set_texture_max_resolution(std::max(64, std::min(4096, val)));
        } else if (argLower == "ktx2" || argLower == "--ktx2") {
            cSkpExporter.options().set_use_ktx2(true);
        } else if (argLower.find("ktx2-quality:") == 0) {
            int val = std::stoi(arg.substr(13));
            cSkpExporter.options().set_ktx2_quality(std::max(1, std::min(255, val)));
        } else if (argLower == "ktx2-uastc") {
            cSkpExporter.options().set_ktx2_uastc(true);
        }
    }
    if (argc < 5)
    {
        if (EndsWithIgnoreCase(output_arg, ".gltf"))
        {
            output_format = "gltf";
        }
        else if (EndsWithIgnoreCase(output_arg, ".glb"))
        {
            output_format = "glb";
        }
    }

    const bool has_separator = output_arg.find('/') != std::string::npos || output_arg.find('\\') != std::string::npos;
    const bool is_absolute   = (!output_arg.empty() && (output_arg[0] == '/' || output_arg[0] == '\\'))
                            || (output_arg.size() > 1 && output_arg[1] == ':');

    if (!output_dir.empty())
    {
        const char tail = output_dir[output_dir.size() - 1];
        if (tail != '/' && tail != '\\')
        {
            output_dir += "/";
        }
    }

    std::string gltf_file = output_arg;
    if (!is_absolute && !has_separator)
    {
        gltf_file = output_dir + output_arg;
    }

    std::cout << "Start conversion" << std::endl;
    if (use_draco) {
        std::cout << "Draco settings: speed=" << cSkpExporter.options().draco_speed()
                  << ", pos=" << cSkpExporter.options().draco_position_bits()
                  << ", tex=" << cSkpExporter.options().draco_tex_bits()
                  << ", norm=" << cSkpExporter.options().draco_normal_bits() << std::endl;
    }

    const bool ok = cSkpExporter.Convert(skp_file, output_dir, gltf_file, output_format, use_draco, nullptr);
    if (ok)
    {
        std::cout << "finished" << std::endl;
        return 0;
    }

    std::cerr << "conversion failed" << std::endl;
    return 1;
}
