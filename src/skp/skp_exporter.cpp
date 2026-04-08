#ifndef NOMINMAX
#define NOMINMAX
#endif
// Copyright 2013 Trimble Navigation Limited. All Rights Reserved.

/*
 * @Author: yaol 
 * @Date: 2025-02-18 17:27:31 
 * @Last Modified by:   yaol 
 * @Last Modified time: 2025-02-18 17:27:31 
 */

#include <string>
#include <sstream>
#include <vector>
#include <cassert>
#include <algorithm>
#include <cctype>
#include <stdint.h>  // For int32_t type
#include <cstdint>  // 添加这一行
#include <cmath>    // 用于 std::sqrt（法线计算）
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>
#include <fstream>

#include "skp_exporter.h"
#include "skp_texture_helper.h"
#include "skp_geom_utils.h"
#include "utils.h"
#include "gltflib/gltfdraco.h"
#include "texture_processor.h"
#include <meshoptimizer.h>



#include <SketchUpAPI/import_export/pluginprogresscallback.h>
#include <SketchUpAPI/initialize.h>
#include <SketchUpAPI/model/component_definition.h>
#include <SketchUpAPI/model/component_instance.h>
#include <SketchUpAPI/model/drawing_element.h>
#include <SketchUpAPI/model/edge.h>
#include <SketchUpAPI/model/entities.h>
#include <SketchUpAPI/model/entity.h>
#include <SketchUpAPI/model/face.h>
#include <SketchUpAPI/model/group.h>
#include <SketchUpAPI/model/layer.h>
#include <SketchUpAPI/model/loop.h>
#include <SketchUpAPI/model/material.h>
#include <SketchUpAPI/model/mesh_helper.h>
#include <SketchUpAPI/model/model.h>
#include <SketchUpAPI/model/texture.h>
#include <SketchUpAPI/model/texture_writer.h>
#include <SketchUpAPI/model/uv_helper.h>
#include <SketchUpAPI/model/vertex.h>

using namespace SkpGeomUtils;
#define pos(a, b) ((a) + ((b)*4))
// A simple SUStringRef wrapper class which makes usage simpler from C++.
class CSUString
{
  public:
    CSUString()
    {
        SUSetInvalid(su_str_);
        SUStringCreate(&su_str_);
    }

    ~CSUString() { SUStringRelease(&su_str_); }

    operator SUStringRef *() { return &su_str_; }

    std::string utf8()
    {
        size_t length;
        SUStringGetUTF8Length(su_str_, &length);
        std::string string;
        string.resize(length + 1);
        size_t returned_length;
        SUStringGetUTF8(su_str_, length, &string[0], &returned_length);
        return string;
    }

  private:
    SUStringRef su_str_;
};

// Utility function to get a material's name
static std::string GetMaterialName(SUMaterialRef material)
{
    CSUString name;
    SU_CALL(SUMaterialGetNameLegacyBehavior(material, name));
    return name.utf8();
}

// Utility function to get a layer's name
static std::string GetLayerName(SULayerRef layer)
{
    CSUString name;
    SU_CALL(SULayerGetName(layer, name));
    return name.utf8();
}

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

static std::string NormalizeOutputFormat(const std::string &output_format)
{
    const std::string normalized = ToLowerCopy(output_format);
    if (normalized == "gltf")
    {
        return "gltf";
    }
    if (normalized == "glb")
    {
        return "glb";
    }
    return "glb";
}

static std::string BuildOutputPathByFormat(const std::string &base_path, const std::string &output_format)
{
    std::string output_path = base_path;
    if (EndsWithIgnoreCase(output_path, ".gltf"))
    {
        output_path.resize(output_path.size() - 5);
    }
    else if (EndsWithIgnoreCase(output_path, ".glb"))
    {
        output_path.resize(output_path.size() - 4);
    }
    output_path += ".";
    output_path += output_format;
    return output_path;
}


CSkpExporter::CSkpExporter()
{
    SUSetInvalid(model_);
    SUSetInvalid(texture_writer_);
    activeFacetMap_ = &facetMap;
}

CSkpExporter::~CSkpExporter() {}

void CSkpExporter::ReleaseModelObjects()
{
    if (!SUIsInvalid(texture_writer_))
    {
        SUTextureWriterRelease(&texture_writer_);
        SUSetInvalid(texture_writer_);
    }

    if (!SUIsInvalid(model_))
    {
        SUModelRelease(&model_);
        SUSetInvalid(model_);
    }

    // Terminate the SDK
    SUTerminate();
}

bool CSkpExporter::Convert(const std::string &src_file,
                            const std::string &file_path,
                            const std::string &file_name,
                            const std::string &output_format,
                            bool use_draco,
                            SketchUpPluginProgressCallback *progress_callback)
{
    bool exported = false;
    outPath       = file_path;
    try
    {
        // Initialize the SDK
        SUInitialize();

        // Create the model from the src_file
        SUSetInvalid(model_);
        SUModelLoadStatus status;
        SU_CALL(SUModelCreateFromFileWithStatus(&model_, src_file.c_str(), &status));
        // Create a texture writer
        SUSetInvalid(texture_writer_);
        SU_CALL(SUTextureWriterCreate(&texture_writer_));
        
        // Load PBR configuration
        LoadMaterialConfig("material_config.json");
        
        // Materials
        std::cout << "WriteMaterials" << std::endl;
        WriteMaterials();
        // Geometry
        std::cout << "WriteGeometry" << std::endl;
        WriteGeometry();
        
        // 在导出到GLTF之前压缩纹理
        CompressAndResizeTextures();
        
        exported = exportToGltfImpl(file_name, output_format, use_draco) == 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Conversion error: " << e.what() << std::endl;
        exported = false;
    }
    catch (...)
    {
        std::cerr << "Unknown error occurred during conversion." << std::endl;
        exported = false;
    }
    ReleaseModelObjects();
    return exported;
}

static void WriteMaterialsTextureImage(SUMaterialRef material, const std::string &texture_image_file)
{
    assert(SUIsValid(material));
    // Only write the material's texture if a non-empty name was provided
    if (texture_image_file.empty())
        return;
    SUTextureRef texture = SU_INVALID;
    if (SUMaterialGetTexture(material, &texture) != SU_ERROR_NONE)
        return;
    // Write the texture using the provided file name
    SU_CALL(SUTextureWriteToFile(texture, texture_image_file.c_str()));
}

static SkpMaterialInfo GetMaterialInfo(SUMaterialRef material, const std::string &texture_directory)
{
    assert(SUIsValid(material));

    SkpMaterialInfo info;

    // Name
    info.name_ = GetMaterialName(material);

    // Color
    info.has_color_ = false;
    info.has_alpha_ = false;
    SUMaterialType type;
    SU_CALL(SUMaterialGetType(material, &type));
    // Color
    if ((type == SUMaterialType_Colored) || (type == SUMaterialType_ColorizedTexture))
    {
        SUColor color;
        if (SUMaterialGetColor(material, &color) == SU_ERROR_NONE)
        {
            info.has_color_ = true;
            info.color_     = color;
        }
    }

    // Alpha
    bool has_alpha = false;
    SU_CALL(SUMaterialGetUseOpacity(material, &has_alpha));
    if (has_alpha)
    {
        double alpha = 0;
        SU_CALL(SUMaterialGetOpacity(material, &alpha));
        info.has_alpha_ = true;
        info.alpha_     = alpha;
    }

    // Texture
    info.has_texture_ = false;
    if ((type == SUMaterialType_Textured) || (type == SUMaterialType_ColorizedTexture))
    {
        SUTextureRef texture = SU_INVALID;
        if (SUMaterialGetTexture(material, &texture) == SU_ERROR_NONE)
        {
            info.has_texture_ = true;
            // Get the PID from the texture to generate a unique output file name
            int32_t tex_id = 0;
            SU_CALL(SUEntityGetID(SUTextureToEntity(texture), &tex_id));
            // Generate a unique name for this material's texture
            std::stringstream sstream;
            sstream << texture_directory << "Texture" << tex_id << ".png";
            std::stringstream sstream_pic;
            sstream_pic << "Texture" << tex_id << ".png";
            info.texture_path_ = sstream.str();
            info.picture_name_ = sstream_pic.str();
            // Texture scale
            size_t width   = 0;
            size_t height  = 0;
            double s_scale = 0.0;
            double t_scale = 0.0;
            SU_CALL(SUTextureGetDimensions(texture, &width, &height, &s_scale, &t_scale));
            info.texture_sscale_ = s_scale;
            info.texture_tscale_ = t_scale;
        }
    }

    return info;
}

void CSkpExporter::WriteMaterials()
{
    if (options_.export_materials())
    {
        if (options_.export_materials_by_layer())
        {
            size_t num_layers;
            SU_CALL(SUModelGetNumLayers(model_, &num_layers));
            if (num_layers > 0)
            {
                std::vector<SULayerRef> layers(num_layers);
                SU_CALL(SUModelGetLayers(model_, num_layers, &layers[0], &num_layers));
                for (size_t i = 0; i < num_layers; i++)
                {
                    SULayerRef layer       = layers[i];
                    SUMaterialRef material = SU_INVALID;
                    if (SULayerGetMaterial(layer, &material) == SU_ERROR_NONE)
                    {
                        WriteMaterial(material);
                    }
                }
            }
        }
        else
        {
            size_t count = 0;
            SU_CALL(SUModelGetNumMaterials(model_, &count));
            if (count > 0)
            {
                std::vector<SUMaterialRef> materials(count);
                SU_CALL(SUModelGetMaterials(model_, count, &materials[0], &count));
                for (size_t i = 0; i < count; i++)
                {
                    WriteMaterial(materials[i]);
                }
            }
        }
    }
}

void CSkpExporter::WriteMaterial(SUMaterialRef material)
{
    if (SUIsInvalid(material))
        return;

    SkpMaterialInfo info    = GetMaterialInfo(material, outPath);
    materialMap[info.name_] = info;
    if (!info.texture_path_.empty())
    {
        std::cout << info.texture_path_ << std::endl;
    }
    WriteMaterialsTextureImage(material, info.texture_path_);
}

void CSkpExporter::WriteGeometry()
{
    if (options_.export_faces() || options_.export_edges())
    {
        SUTransformation identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        
        // 1. Process the root entities
        SUEntitiesRef model_entities;
        SU_CALL(SUModelGetEntities(model_, &model_entities));
        
        // Root node
        NodeInfo rootNode;
        rootNode.name = "Root";
        std::copy(identity.values, identity.values + 16, rootNode.matrix);
        nodeList.push_back(rootNode);
        int rootIdx = 0;

        // activeFacetMap_ already points to this->facetMap by constructor
        ProcessGeometryBatch(model_entities, identity, DEFAULT_BATCH_SIZE, rootIdx);
        
        // 确保处理完所有剩余数据
        faceBuffer.clear();
        faceBuffer.shrink_to_fit();
    }
}

void CSkpExporter::ProcessGeometryBatch(SUEntitiesRef entities, 
                                      const SUTransformation& transformation,
                                      size_t batchSize,
                                      int parentNodeIdx) {
    size_t num_faces = 0;
    size_t num_groups = 0;
    size_t num_instances = 0;
    
    SU_CALL(SUEntitiesGetNumFaces(entities, &num_faces));
    SU_CALL(SUEntitiesGetNumGroups(entities, &num_groups));
    SU_CALL(SUEntitiesGetNumInstances(entities, &num_instances));
    
    // 1. 处理面 (非批处理，一次性获取指针以保证逻辑正确)
    if (num_faces > 0) {
        std::vector<SUFaceRef> faces(num_faces);
        size_t actual_count = 0;
        SU_CALL(SUEntitiesGetFaces(entities, num_faces, &faces[0], &actual_count));
        
        for (size_t i = 0; i < actual_count; i++) {
            inheritance_manager_.PushElement(faces[i]);
            WriteFace(faces[i], transformation);
            inheritance_manager_.PopElement();
        }
    }
    
    // 2. 处理组和组件 (移除 else 分支，确保全部处理)
    if (num_groups > 0) {
        traversalGroupEntity(entities, transformation, parentNodeIdx);
    }
    
    if (num_instances > 0) {
        getComponentEntity(entities, transformation, parentNodeIdx);
    }
}

void CSkpExporter::WriteEntities(SUEntitiesRef entities, const SUTransformation &transformation, int parentNodeIdx)
{
    if (SUIsInvalid(entities)) {
        return;
    }
    
    // 使用 ProcessGeometryBatch 统一处理所有实体
    ProcessGeometryBatch(entities, transformation, DEFAULT_BATCH_SIZE, parentNodeIdx);
}
void CSkpExporter::traversalGroupEntity(SUEntitiesRef entities, const SUTransformation &transformation, int parentNodeIdx)
{
    // Groups in SketchUp are essentially ComponentInstances under the hood.
    // For simplicity, we can treat them similarly to instances or keep them as direct nodes.
    size_t num_groups = 0;
    SU_CALL(SUEntitiesGetNumGroups(entities, &num_groups));
    if (num_groups > 0)
    {
        std::vector<SUGroupRef> groups(num_groups);
        SU_CALL(SUEntitiesGetGroups(entities, num_groups, &groups[0], &num_groups));
        for (size_t g = 0; g < num_groups; g++)
        {
            SUGroupRef group = groups[g];
            SUTransformation transforMation2;
            SU_CALL(SUGroupGetTransform(group, &transforMation2));
            
            SUEntitiesRef group_entities = SU_INVALID;
            SU_CALL(SUGroupGetEntities(group, &group_entities));

            SUDrawingElementRef drawing_element = SUGroupToDrawingElement(group);
            SULayerRef layer;
            SUSetInvalid(layer);
            SUDrawingElementGetLayer(drawing_element, &layer);
            bool is_visible = true;
            SU_CALL(SULayerGetVisibility(layer, &is_visible));
            
            if (is_visible)
            {
                // Create a node for this group
                NodeInfo node;
                node.name = "Group_" + std::to_string(g);
                std::copy(transforMation2.values, transforMation2.values + 16, node.matrix);
                
                int nodeIdx = nodeList.size();
                nodeList.push_back(node);
                nodeList[parentNodeIdx].children.push_back(nodeIdx);

                inheritance_manager_.PushElement(group);
                SUTransformation group_identity = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
                WriteEntities(group_entities, group_identity, nodeIdx);
                inheritance_manager_.PopElement();
            }
        }
    }
}
void CSkpExporter::getComponentEntity(SUEntitiesRef entities, const SUTransformation &transformation, int parentNodeIdx)
{
    size_t componentInstanceLen;
    SUEntitiesGetNumInstances(entities, &componentInstanceLen);
    if (componentInstanceLen > 0)
    {
        std::vector<SUComponentInstanceRef> componentInstanceArr(componentInstanceLen);
        SUEntitiesGetInstances(entities, componentInstanceLen, &componentInstanceArr[0], &componentInstanceLen);
        for (size_t i = 0; i < componentInstanceLen; i++)
        {
            SUComponentDefinitionRef definition;
            SUComponentInstanceGetDefinition(componentInstanceArr[i], &definition);
            
            SUTransformation instance_transform;
            SUComponentInstanceGetTransform(componentInstanceArr[i], &instance_transform);
            
            // Adjust ratio for position
            instance_transform.values[12] *= ratio;
            instance_transform.values[13] *= ratio;
            instance_transform.values[14] *= ratio;

            SUDrawingElementRef drawing_element = SUComponentInstanceToDrawingElement(componentInstanceArr[i]);
            SULayerRef layer;
            SUSetInvalid(layer);
            SUDrawingElementGetLayer(drawing_element, &layer);
            bool is_visible = true;
            SU_CALL(SULayerGetVisibility(layer, &is_visible));
            
            if (is_visible)
            {
                inheritance_manager_.PushElement(componentInstanceArr[i]);
                
                // Create node
                NodeInfo node;
                std::copy(instance_transform.values, instance_transform.values + 16, node.matrix);
                
                // Check if mesh already exists for this definition
                if (definitionToMeshIndex.count(definition.ptr)) {
                    node.meshIndex = definitionToMeshIndex[definition.ptr];
                } else {
                    // Create new mesh for definition
                    MeshInfo newMesh;
                    int meshIdx = meshList.size();
                    meshList.push_back(newMesh);
                    definitionToMeshIndex[definition.ptr] = meshIdx;
                    
                    // Switch active facet map to the new mesh
                    std::unordered_map<Color, std::vector<cFacet>, colorHashFuc>* oldFacetMap = activeFacetMap_;
                    activeFacetMap_ = &meshList[meshIdx].facetMap;
                    
                    SUEntitiesRef def_entities = SU_INVALID;
                    SUComponentDefinitionGetEntities(definition, &def_entities);
                    
                    SUTransformation identity = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
                    node.meshIndex = meshIdx;
                    
                    // We need a node locally to represent the definition's internal structure
                    int nodeInMeshIdx = nodeList.size();
                    NodeInfo defNode;
                    defNode.name = "DefNode_" + std::to_string(meshIdx);
                    std::copy(identity.values, identity.values + 16, defNode.matrix);
                    nodeList.push_back(defNode);
                    
                    // Recursively process definition's entities
                    ProcessGeometryBatch(def_entities, identity, DEFAULT_BATCH_SIZE, nodeInMeshIdx);
                    
                    // Restore active facet map
                    activeFacetMap_ = oldFacetMap;
                }
                
                int nodeIdx = nodeList.size();
                nodeList.push_back(node);
                nodeList[parentNodeIdx].children.push_back(nodeIdx);
                
                inheritance_manager_.PopElement();
            }
        }
    }
}

int CSkpExporter::exportToGltfImpl(const std::string &gltfName, const std::string &outputFormat, bool use_draco) {
    const float weldEpsilon = static_cast<float>(options_.vertex_weld_epsilon());
    const float invEpsilon = 1.0f / weldEpsilon;
    
    struct VertexData {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
        int gridX, gridY, gridZ;

        bool operator==(const VertexData& o) const {
            return gridX == o.gridX && gridY == o.gridY && gridZ == o.gridZ &&
                   std::abs(nx - o.nx) < 0.01f &&
                   std::abs(ny - o.ny) < 0.01f &&
                   std::abs(nz - o.nz) < 0.01f &&
                   std::abs(u - o.u) < 0.001f &&
                   std::abs(v - o.v) < 0.001f;
        }
    };

    struct VertexDataHash {
        size_t operator()(const VertexData& v) const {
            size_t h1 = std::hash<int>()(v.gridX);
            size_t h2 = std::hash<int>()(v.gridY);
            size_t h3 = std::hash<int>()(v.gridZ);
            size_t h4 = std::hash<float>()(v.nx);
            size_t h5 = std::hash<float>()(v.ny);
            size_t h6 = std::hash<float>()(v.nz);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5);
        }
    };

    tinygltf::Model model;
    model.asset.version = "2.0";
    model.asset.generator = "zhuzhaoyun";
    tinygltf::Scene scene;
    model.scenes.push_back(scene);
    model.defaultScene = 0;
    
    // 初始化缓冲区，避免后续访问 model.buffers[0] 越界
    model.buffers.emplace_back();
    
    // 定义一个辅助Lambda用于添加Mesh
    auto addMeshLambda = [&](const std::unordered_map<Color, std::vector<cFacet>, colorHashFuc>& currentFacetMap, const std::string& meshName) -> int {
        if (currentFacetMap.empty()) return -1;
        
        tinygltf::Mesh mesh;
        mesh.name = meshName;
        
        for (auto &item : currentFacetMap) {
            tinygltf::Primitive primitive;
            primitive.mode = 4;  // triangles
            
            const std::vector<cFacet> &facetVec = item.second;
            if (facetVec.empty()) continue;

            // 收集顶点、法线和索引数据
            std::vector<float> positions;
            std::vector<float> normals;
            std::vector<float> uvs;
            std::vector<unsigned int> indices;

            // 1. 第一阶段：计算每个位置的累加法线
            // Key: x,y,z position, Value: Summed normal vector
            struct Pos { 
                float x, y, z; 
                bool operator==(const Pos& o) const { 
                    return x == o.x && y == o.y && z == o.z; 
                } 
            };
            struct PosHash {
                size_t operator()(const Pos& p) const {
                    auto h1 = std::hash<float>{}(p.x);
                    auto h2 = std::hash<float>{}(p.y);
                    auto h3 = std::hash<float>{}(p.z);
                    return h1 ^ (h2 << 1) ^ (h3 << 2);
                }
            };
            std::unordered_map<Pos, std::vector<float>, PosHash> posToNormals;

            for (const auto& facet : facetVec) {
                // 计算面法线
                float v[3][3] = {
                    {(float)facet.vertex[0].x, (float)facet.vertex[0].y, (float)facet.vertex[0].z},
                    {(float)facet.vertex[1].x, (float)facet.vertex[1].y, (float)facet.vertex[1].z},
                    {(float)facet.vertex[2].x, (float)facet.vertex[2].y, (float)facet.vertex[2].z}
                };
                float edge1[3] = {v[1][0] - v[0][0], v[1][1] - v[0][1], v[1][2] - v[0][2]};
                float edge2[3] = {v[2][0] - v[0][0], v[2][1] - v[0][1], v[2][2] - v[0][2]};
                float nx = edge1[1] * edge2[2] - edge1[2] * edge2[1];
                float ny = edge1[2] * edge2[0] - edge1[0] * edge2[2];
                float nz = edge1[0] * edge2[1] - edge1[1] * edge2[0];
                float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (len > 1e-8f) { nx /= len; ny /= len; nz /= len; }
                else { nx = 0; ny = 0; nz = 1.0f; }

                for (int j = 0; j < 3; j++) {
                    Pos p = {v[j][0], v[j][1], v[j][2]};
                    auto& nSum = posToNormals[p];
                    if (nSum.empty()) nSum = {0, 0, 0};
                    nSum[0] += nx; nSum[1] += ny; nSum[2] += nz;
                }
            }

            // 归一化累加法线
            std::unordered_map<Pos, std::vector<float>, PosHash> normalizedNormals;
            for (auto& entry : posToNormals) {
                float nx = entry.second[0], ny = entry.second[1], nz = entry.second[2];
                float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (len > 1e-8f) { nx /= len; ny /= len; nz /= len; }
                normalizedNormals[entry.first] = {nx, ny, nz};
            }

            // 2. 第二阶段：构建顶点数据并进行焊接
            std::unordered_map<VertexData, unsigned int, VertexDataHash> vertexCache;
            float posMin[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
            float posMax[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
            size_t vertexOffset = 0;

            for (const auto& facet : facetVec) {
                for (int j = 0; j < 3; j++) {
                    float x = (float)facet.vertex[j].x;
                    float y = (float)facet.vertex[j].y;
                    float z = (float)facet.vertex[j].z;
                    float u = (float)facet.uv[j].x;
                    float v_coord = (float)(-facet.uv[j].y);
                    
                    Pos p = {x, y, z};
                    const auto& sn = normalizedNormals[p];
                    float nx = sn[0], ny = sn[1], nz = sn[2];

                    int gridX = static_cast<int>(std::floor(x * invEpsilon));
                    int gridY = static_cast<int>(std::floor(y * invEpsilon));
                    int gridZ = static_cast<int>(std::floor(z * invEpsilon));

                    VertexData vData = {x, y, z, nx, ny, nz, u, v_coord, gridX, gridY, gridZ};
                    auto it = vertexCache.find(vData);
                    if (it != vertexCache.end()) {
                        indices.push_back(it->second);
                    } else {
                        unsigned int newIndex = (unsigned int)vertexOffset++;
                        vertexCache[vData] = newIndex;
                        indices.push_back(newIndex);
                        
                        posMin[0] = (std::min)(posMin[0], x); posMin[1] = (std::min)(posMin[1], y); posMin[2] = (std::min)(posMin[2], z);
                        posMax[0] = (std::max)(posMax[0], x); posMax[1] = (std::max)(posMax[1], y); posMax[2] = (std::max)(posMax[2], z);
                        
                        positions.push_back(x); positions.push_back(y); positions.push_back(z);
                        normals.push_back(nx); normals.push_back(ny); normals.push_back(nz);
                        uvs.push_back(u); uvs.push_back(v_coord);
                    }
                }
            }

            // 3. 第三阶段：使用 meshoptimizer 进行顶点顺序和缓存优化
            if (!indices.empty() && vertexOffset > 0) {
                // 优化顶点缓存（提高读性能）
                meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertexOffset);
                
                // 优化顶点获取（提高内存局部性）
                // 必须按顺序对所有流进行重映射
                std::vector<unsigned int> remap(vertexOffset);
                size_t unique_vertices = meshopt_optimizeVertexFetchRemap(remap.data(), indices.data(), indices.size(), vertexOffset);
                
                meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
                
                std::vector<float> optimized_positions(unique_vertices * 3);
                meshopt_remapVertexBuffer(optimized_positions.data(), positions.data(), vertexOffset, sizeof(float) * 3, remap.data());
                positions = std::move(optimized_positions);
                
                std::vector<float> optimized_normals(unique_vertices * 3);
                meshopt_remapVertexBuffer(optimized_normals.data(), normals.data(), vertexOffset, sizeof(float) * 3, remap.data());
                normals = std::move(optimized_normals);
                
                if (!uvs.empty()) {
                    std::vector<float> optimized_uvs(unique_vertices * 2);
                    meshopt_remapVertexBuffer(optimized_uvs.data(), uvs.data(), vertexOffset, sizeof(float) * 2, remap.data());
                    uvs = std::move(optimized_uvs);
                }
                
                vertexOffset = unique_vertices;
            }

            // POSITION
            {
                tinygltf::BufferView bv; bv.buffer = 0; bv.byteOffset = model.buffers[0].data.size();
                bv.byteLength = positions.size() * sizeof(float); bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
                model.buffers[0].data.insert(model.buffers[0].data.end(), (uint8_t*)positions.data(), (uint8_t*)positions.data() + bv.byteLength);
                int bvIdx = model.bufferViews.size(); model.bufferViews.push_back(bv);
                tinygltf::Accessor acc; acc.bufferView = bvIdx; acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                acc.count = positions.size() / 3; acc.type = TINYGLTF_TYPE_VEC3;
                acc.minValues = {posMin[0], posMin[1], posMin[2]}; acc.maxValues = {posMax[0], posMax[1], posMax[2]};
                primitive.attributes["POSITION"] = model.accessors.size(); model.accessors.push_back(acc);
            }
            // NORMAL
            {
                tinygltf::BufferView bv; bv.buffer = 0; bv.byteOffset = model.buffers[0].data.size();
                bv.byteLength = normals.size() * sizeof(float); bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
                model.buffers[0].data.insert(model.buffers[0].data.end(), (uint8_t*)normals.data(), (uint8_t*)normals.data() + bv.byteLength);
                int bvIdx = model.bufferViews.size(); model.bufferViews.push_back(bv);
                tinygltf::Accessor acc; acc.bufferView = bvIdx; acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                acc.count = normals.size() / 3; acc.type = TINYGLTF_TYPE_VEC3;
                primitive.attributes["NORMAL"] = model.accessors.size(); model.accessors.push_back(acc);
            }
            // UV
            if (!uvs.empty() && !item.first.imageUri.empty()) {
                tinygltf::BufferView bv; bv.buffer = 0; bv.byteOffset = model.buffers[0].data.size();
                bv.byteLength = uvs.size() * sizeof(float); bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
                model.buffers[0].data.insert(model.buffers[0].data.end(), (uint8_t*)uvs.data(), (uint8_t*)uvs.data() + bv.byteLength);
                int bvIdx = model.bufferViews.size(); model.bufferViews.push_back(bv);
                tinygltf::Accessor acc; acc.bufferView = bvIdx; acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                acc.count = uvs.size() / 2; acc.type = TINYGLTF_TYPE_VEC2;
                primitive.attributes["TEXCOORD_0"] = model.accessors.size(); model.accessors.push_back(acc);
            }
            // INDICES (Optimization: use smallest possible type)
            {
                tinygltf::BufferView bv; bv.buffer = 0; bv.byteOffset = model.buffers[0].data.size();
                bv.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
                
                if (vertexOffset < 256) {
                    std::vector<unsigned char> byteIndices(indices.begin(), indices.end());
                    bv.byteLength = byteIndices.size() * sizeof(unsigned char);
                    model.buffers[0].data.insert(model.buffers[0].data.end(), (uint8_t*)byteIndices.data(), (uint8_t*)byteIndices.data() + bv.byteLength);
                    tinygltf::Accessor acc; acc.bufferView = model.bufferViews.size();
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                    acc.count = indices.size(); acc.type = TINYGLTF_TYPE_SCALAR;
                    primitive.indices = model.accessors.size(); model.accessors.push_back(acc);
                } else if (vertexOffset < 65535) {
                    std::vector<unsigned short> shortIndices(indices.begin(), indices.end());
                    bv.byteLength = shortIndices.size() * sizeof(unsigned short);
                    model.buffers[0].data.insert(model.buffers[0].data.end(), (uint8_t*)shortIndices.data(), (uint8_t*)shortIndices.data() + bv.byteLength);
                    tinygltf::Accessor acc; acc.bufferView = model.bufferViews.size();
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                    acc.count = indices.size(); acc.type = TINYGLTF_TYPE_SCALAR;
                    primitive.indices = model.accessors.size(); model.accessors.push_back(acc);
                } else {
                    bv.byteLength = indices.size() * sizeof(unsigned int);
                    model.buffers[0].data.insert(model.buffers[0].data.end(), (uint8_t*)indices.data(), (uint8_t*)indices.data() + bv.byteLength);
                    tinygltf::Accessor acc; acc.bufferView = model.bufferViews.size();
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                    acc.count = indices.size(); acc.type = TINYGLTF_TYPE_SCALAR;
                    primitive.indices = model.accessors.size(); model.accessors.push_back(acc);
                }
                model.bufferViews.push_back(bv);
            }
            
            // MATERIAL
            tinygltf::Material material;
            material.pbrMetallicRoughness.baseColorFactor = {item.first.r, item.first.g, item.first.b, item.first.a};
            material.name = item.first.name;
            
            // PBR 增强：使用配置文件或内置启发式设置
            float metallic = 0.0f;
            float roughness = 0.8f;
            ApplyPbrMapping(item.first.name, metallic, roughness);

            material.pbrMetallicRoughness.metallicFactor = metallic;
            material.pbrMetallicRoughness.roughnessFactor = roughness;
            
            // Alpha 混合模式处理
            std::string matNameLower = ToLowerCopy(item.first.name);
            if (item.first.a < 0.99) {
                // 如果材质名包含 leaf, fence, tree 等，且有透明度，通常 MASK 模式效果更稳
                if (matNameLower.find("leaf") != std::string::npos || 
                    matNameLower.find("tree") != std::string::npos ||
                    matNameLower.find("fence") != std::string::npos ||
                    matNameLower.find("alpha") != std::string::npos) {
                    material.alphaMode = "MASK";
                    material.alphaCutoff = 0.5;
                } else {
                    material.alphaMode = "BLEND";
                }
                material.doubleSided = true;
            } else {
                material.alphaMode = "OPAQUE";
                material.doubleSided = false;
            }

            if (!item.first.imageUri.empty()) {
                std::string processedTexturePath = ProcessTexture(item.first.imageUri);
                tinygltf::Image gltfImg; gltfImg.uri = processedTexturePath;
                int imgIdx = model.images.size(); model.images.push_back(gltfImg);
                tinygltf::Texture gltfTex; gltfTex.source = imgIdx;
                int texIdx = model.textures.size(); model.textures.push_back(gltfTex);
                material.pbrMetallicRoughness.baseColorTexture.index = texIdx;
            }
            primitive.material = model.materials.size(); model.materials.push_back(material);
            mesh.primitives.push_back(primitive);
        }
        int meshIdx = model.meshes.size(); model.meshes.push_back(mesh);
        return meshIdx;
    };

    // 1. Add all constituent meshes
    std::vector<int> modelMeshIndices;
    for (size_t i = 0; i < meshList.size(); i++) {
        modelMeshIndices.push_back(addMeshLambda(meshList[i].facetMap, "Mesh_" + std::to_string(i)));
    }
    
    // 2. Add root mesh
    int rootMeshIdx = addMeshLambda(facetMap, "RootMesh");

    // 3. Add nodes and link to meshes
    for (size_t i = 0; i < nodeList.size(); i++) {
        tinygltf::Node node;
        node.name = nodeList[i].name;
        node.matrix = std::vector<double>(nodeList[i].matrix, nodeList[i].matrix + 16);
        
        if (nodeList[i].meshIndex >= 0 && (size_t)nodeList[i].meshIndex < modelMeshIndices.size()) {
            node.mesh = modelMeshIndices[nodeList[i].meshIndex];
        } else if (i == 0 && rootMeshIdx >= 0) {
            // Apply root mesh to root node
            node.mesh = rootMeshIdx;
        }
        
        node.children = nodeList[i].children;
        model.nodes.push_back(node);
    }
    
    // Set scene root
    if (!model.nodes.empty()) {
        model.scenes[0].nodes.push_back(0); // 0 is the root node
    }
    
    tinygltf::TinyGLTF gltf;
    const std::string normalizedFormat = NormalizeOutputFormat(outputFormat);
    const std::string outputPath = BuildOutputPathByFormat(gltfName, normalizedFormat);
    const bool writeBinary = normalizedFormat == "glb";

    bool ret = gltf.WriteGltfSceneToFile(&model, outputPath,
                                        true,  // embedImages
                                        true,  // embedBuffers
                                        true,  // prettyPrint
                                        writeBinary);
    
    if (ret && use_draco) {
        std::cout << "Starting Draco compression..." << std::endl;
        gltf::GltfDraco dracoTool(&model);
        dracoTool.encode(
            options_.draco_speed(),
            options_.draco_position_bits(),
            options_.draco_tex_bits(),
            options_.draco_normal_bits(),
            options_.draco_color_bits(),
            options_.draco_generic_bits()
        );
        
        // Save again with Draco extension
        ret = gltf.WriteGltfSceneToFile(&model, outputPath,
                                          true,  // embedImages
                                          true,  // embedBuffers
                                          true,  // prettyPrint
                                          writeBinary);
    }
    
    return ret ? 0 : 1;
}


void CSkpExporter::WriteFace(SUFaceRef face, const SUTransformation &transformation)
{
    if (SUIsInvalid(face))
        return;

    SkpFaceInfo info;

    // Get Current layer off of our stack and then get the id from it
    SULayerRef layer = inheritance_manager_.GetCurrentLayer();
    if (!SUIsInvalid(layer))
    {
        info.layer_name_ = GetLayerName(layer);
    }

    // Get the current front and back materials off of our stack
    SUMaterialRef front_material = inheritance_manager_.GetCurrentFrontMaterial();
    if (!SUIsInvalid(front_material))
    {
        // Material name
        info.front_mat_name_ = GetMaterialName(front_material);
        // std::cout << "info.front_mat_name_=" << info.front_mat_name_ << std::endl;

        // Has texture ?
        SUTextureRef texture_ref = SU_INVALID;
        info.has_front_texture_  = SUMaterialGetTexture(front_material, &texture_ref) == SU_ERROR_NONE;
    }
    SUMaterialRef back_material = inheritance_manager_.GetCurrentBackMaterial();
    if (!SUIsInvalid(back_material))
    {
        // Material name
        info.back_mat_name_ = GetMaterialName(back_material);

        // Has texture ?
        SUTextureRef texture_ref = SU_INVALID;
        info.has_back_texture_   = SUMaterialGetTexture(back_material, &texture_ref) == SU_ERROR_NONE;
    }

    // Get a uv helper
    SUUVHelperRef uv_helper = SU_INVALID;
    SUFaceGetUVHelper(face, info.has_front_texture_, info.has_back_texture_, texture_writer_, &uv_helper);

    // If this is a complex face with one or more holes in it
    // we tessellate it into triangles using the polygon mesh class, then
    // export each triangle as a face.
    info.has_single_loop_ = false;

    // Create and process mesh
    SUMeshHelperRef mesh_ref = SU_INVALID;
    SU_CALL(SUMeshHelperCreateWithTextureWriter(&mesh_ref, face, texture_writer_));

    // Get the vertices
    size_t num_vertices = 0;
    SU_CALL(SUMeshHelperGetNumVertices(mesh_ref, &num_vertices));
    if (num_vertices == 0)
        return;
    std::vector<SUPoint3D> vertices(num_vertices);
    SU_CALL(SUMeshHelperGetVertices(mesh_ref, num_vertices, &vertices[0], &num_vertices));

    // Get triangle indices.
    size_t num_triangles = 0;
    SU_CALL(SUMeshHelperGetNumTriangles(mesh_ref, &num_triangles));

    const size_t num_indices = 3 * num_triangles;
    size_t num_retrieved     = 0;
    std::vector<size_t> indices(num_indices);
    std::vector<SUVector3D> normalArr(num_triangles);
    
    if (num_triangles > 0) {
        size_t num_normals = 0;
        SUMeshHelperGetNormals(mesh_ref, num_triangles, &normalArr[0], &num_normals);
        SU_CALL(SUMeshHelperGetVertexIndices(mesh_ref, num_indices, &indices[0], &num_retrieved));
    }

    // Get UV coords.
    std::vector<SUPoint3D> front_stq(num_vertices);
    std::vector<SUPoint3D> back_stq(num_vertices);
    size_t count;
    if (info.has_front_texture_)
    {
        SU_CALL(SUMeshHelperGetFrontSTQCoords(mesh_ref, num_vertices, &front_stq[0], &count));
    }
    if (info.has_back_texture_)
    {
        SU_CALL(SUMeshHelperGetBackSTQCoords(mesh_ref, num_vertices, &back_stq[0], &count));
    }
    SkpMaterialInfo &materialInfo     = materialMap[info.front_mat_name_];
    SkpMaterialInfo &materialInfoBack = materialMap[info.back_mat_name_];
    Color color;
    Color colorBack;
    if (info.front_mat_name_ != "")
    {
        if (materialInfo.has_color_)
        {
            color.r = (double)(materialInfo.color_.red) / 255;
            color.g = (double)(materialInfo.color_.green) / 255;
            color.b = (double)(materialInfo.color_.blue) / 255;
        }
        else
        {
            color.r = 1.0;
            color.g = 1.0;
            color.b = 1.0;
        }
        if (materialInfo.has_alpha_)
        {
            color.a = (double)(materialInfo.color_.alpha) / 255;
        }
        else
        {
            color.a = 1.0;
        }
        color.name = info.front_mat_name_;
        if (info.has_front_texture_)
        {
            color.imageUri = materialInfo.picture_name_;
        }
    }
    else
    {
        color.r        = 1.0;
        color.g        = 1.0;
        color.b        = 1.0;
        color.a        = 1.0;
        color.name     = "default";
        color.imageUri = materialInfo.picture_name_;
    }
    if (info.back_mat_name_ != "")
    {
        if (materialInfoBack.has_color_)
        {
            colorBack.r = (double)(materialInfoBack.color_.red) / 255;
            colorBack.g = (double)(materialInfoBack.color_.green) / 255;
            colorBack.b = (double)(materialInfoBack.color_.blue) / 255;
        }
        else
        {
            colorBack.r = 1.0;
            colorBack.g = 1.0;
            colorBack.b = 1.0;
        }
        if (materialInfoBack.has_alpha_)
        {
            colorBack.a = (double)(materialInfoBack.color_.alpha) / 255;
        }
        else
        {
            colorBack.a = 1.0;
        }
        colorBack.name = info.front_mat_name_;
        if (info.has_back_texture_)
        {
            colorBack.imageUri = materialInfoBack.picture_name_;
        }
    }
    
    // Process triangles
    for (size_t i = 0; i < num_triangles; i++) {
        cFacet aFacet;
        
        // Face normal
        SUVector3D normal = normalArr[i];
        aFacet.normal = Vector3(normal.x, normal.y, normal.z);
        
        // Triangle color/material
        Color color1((double)color.r, (double)color.g, (double)color.b, (double)color.a, color.imageUri, color.name);
        
        for (int j = 0; j < 3; j++) {
            size_t vertexIndex = indices[i * 3 + j];
            SUPoint3D pt = vertices[vertexIndex];
            
            // Transform point
            double transformedPt[3] = {pt.x, pt.y, pt.z};
            double result[3] = {0,0,0};
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    result[r] += transformedPt[c] * transformation.values[pos(r, c)];
                }
                result[r] += transformation.values[pos(r, 3)];
            }
            
            aFacet.vertex[j] = Vector3(result[0], result[1], result[2]);
            
            if (info.has_front_texture_) {
                SUPoint3D stq = front_stq[vertexIndex];
                aFacet.uv[j] = Vector3(stq.x * materialInfo.texture_sscale_, stq.y * materialInfo.texture_tscale_, 0.0);
            } else {
                aFacet.uv[j] = Vector3(0.0, 0.0, 0.0);
            }
        }
        
        // Add to the current active mesh
        if (activeFacetMap_) {
            (*activeFacetMap_)[color1].push_back(aFacet);
        }
    }
    
    SU_CALL(SUMeshHelperRelease(&mesh_ref));
    SU_CALL(SUUVHelperRelease(&uv_helper));
}

// 添加新的方法实现
void CSkpExporter::CompressAndResizeTextures() {
    for (auto& materialPair : materialMap) {
        const auto& materialInfo = materialPair.second;
        if (!materialInfo.texture_path_.empty()) {
            materialPair.second.picture_name_ = ProcessTexture(materialInfo.texture_path_);
        }
    }
}

std::string CSkpExporter::ProcessTexture(const std::string& texturePath) {
    return TextureProcessor::ProcessTexture(texturePath, options_.texture_max_resolution());
}

size_t CSkpExporter::GetOrCreateVertexIndex(const VertexKey& key) {
    auto it = vertexCache.find(key);
    if (it != vertexCache.end()) {
        return it->second;
    }
    
    size_t newIndex = uniqueVertices.size();
    vertexCache[key] = newIndex;
    uniqueVertices.push_back(key);
    return newIndex;
}

void CSkpExporter::ClearVertexCache() {
    vertexCache.clear();
    uniqueVertices.clear();
}

std::string CSkpExporter::GetMetadataJson(const std::string& src_file) {
    using json = nlohmann::json;
    json meta;
    
    try {
        SUInitialize();
        SUSetInvalid(model_);
        SUModelLoadStatus status;
        if (SUModelCreateFromFileWithStatus(&model_, src_file.c_str(), &status) != SU_ERROR_NONE) {
            return "{\"error\": \"Failed to load model\"}";
        }
        
        // Basic Stats
        size_t num_faces = 0;
        size_t num_materials = 0;
        size_t num_layers = 0;
        size_t num_definitions = 0;
        
        SUEntitiesRef entities;
        SUModelGetEntities(model_, &entities);
        SUEntitiesGetNumFaces(entities, &num_faces);
        SUModelGetNumMaterials(model_, &num_materials);
        SUModelGetNumLayers(model_, &num_layers);
        SUModelGetNumComponentDefinitions(model_, &num_definitions);
        
        meta["faces"] = num_faces;
        meta["materials_count"] = num_materials;
        meta["layers_count"] = num_layers;
        meta["definitions_count"] = num_definitions;
        
        // Unit info
        SUModelUnits units;
        SUModelGetUnits(model_, &units);
        meta["units"] = static_cast<int>(units);
        
        // Materials list
        if (num_materials > 0) {
            std::vector<SUMaterialRef> materials(num_materials);
            SUModelGetMaterials(model_, num_materials, &materials[0], &num_materials);
            for (auto m : materials) {
                meta["materials"].push_back(GetMaterialName(m));
            }
        }
        
        // Layers list
        if (num_layers > 0) {
            std::vector<SULayerRef> layers(num_layers);
            SUModelGetLayers(model_, num_layers, &layers[0], &num_layers);
            for (auto l : layers) {
                meta["layers"].push_back(GetLayerName(l));
            }
        }
        
        SUModelRelease(&model_);
        SUTerminate();
        
        meta["status"] = "success";
    } catch (const std::exception& e) {
        meta["error"] = e.what();
        meta["status"] = "failed";
    } catch (...) {
        meta["error"] = "Unknown error";
        meta["status"] = "failed";
    }
    
    return meta.dump();
}

void CSkpExporter::LoadMaterialConfig(const std::string& config_path) {
    try {
        std::ifstream f(config_path);
        if (f.is_open()) {
            f >> material_config_;
        } else {
            // Default config if file missing
            material_config_ = {
                {"mappings", {
                    {{"keywords", {"metal", "steel"}}, {"metallic", 1.0}, {"roughness", 0.2}},
                    {{"keywords", {"glass"}}, {"metallic", 0.0}, {"roughness", 0.05}}
                }},
                {"defaults", {{"metallic", 0.0}, {"roughness", 0.6}}}
            };
        }
    } catch (...) {
        // Fallback to safe defaults
        material_config_ = {{"defaults", {{"metallic", 0.0}, {"roughness", 0.6}}}};
    }
}

void CSkpExporter::ApplyPbrMapping(const std::string& mat_name, float& metallic, float& roughness) {
    if (material_config_.is_null() || !material_config_.contains("defaults")) {
        metallic = 0.0f;
        roughness = 0.6f;
        return;
    }
    
    metallic = material_config_["defaults"]["metallic"];
    roughness = material_config_["defaults"]["roughness"];
    
    std::string lowerName = ToLowerCopy(mat_name);
    
    if (material_config_.contains("mappings")) {
        for (auto& mapping : material_config_["mappings"]) {
            if (mapping.contains("keywords")) {
                for (auto& keyword : mapping["keywords"]) {
                    if (lowerName.find(ToLowerCopy(keyword)) != std::string::npos) {
                        metallic = mapping["metallic"];
                        roughness = mapping["roughness"];
                        return;
                    }
                }
            }
        }
    }
}
