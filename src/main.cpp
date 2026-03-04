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
#include "skp2xml/xmlexporter.h"
#include "tinyxml2/tinyxml2.h"
#include "skp2xml/xmlexporter.h"

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
        std::cerr << "Usage: skp2gltf.exe <input.skp> <output_dir> <output_name_or_path> [output_format:gltf|glb]" << std::endl;
        return 2;
    }

    std::string skp_file   = argv[1];
    std::string output_dir = argv[2];
    std::string output_arg = argv[3];
    std::string output_format = "glb";

    if (argc >= 5)
    {
        output_format = ToLowerCopy(argv[4]);
        if (output_format != "gltf" && output_format != "glb")
        {
            std::cerr << "Invalid output format: " << argv[4] << ". Supported formats: gltf, glb" << std::endl;
            return 2;
        }
    }
    else
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

    CXmlExporter cXmlExporter;
    std::cout << "Start conversion" << std::endl;

    // C:\model\allRvtFile\skp\67beca5e7d1b0078ee8c7fee.skp 
    // C:\model\model\skp\67beca5e7d1b0078ee8c7fee\ 
    // C:\model\model\skp\67beca5e7d1b0078ee8c7fee
    const bool ok = cXmlExporter.Convert(skp_file, output_dir, gltf_file, output_format, nullptr);
    if (ok)
    {
        std::cout << "finished" << std::endl;
        return 0;
    }

    std::cerr << "conversion failed" << std::endl;
    return 1;
}
