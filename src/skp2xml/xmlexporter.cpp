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

#include "xmlexporter.h"
#include "xmltexturehelper.h"
#include "xmlgeomutils.h"
#include "utils.h"
#include "gltflib/gltfdraco.h"
#include "texture_processor.h"


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

using namespace XmlGeomUtils;
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


CXmlExporter::CXmlExporter()
{
    SUSetInvalid(model_);
    SUSetInvalid(texture_writer_);
    activeFacetMap_ = &facetMap;
}

CXmlExporter::~CXmlExporter() {}

void CXmlExporter::ReleaseModelObjects()
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

bool CXmlExporter::Convert(const std::string &src_file,
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
        // Open the xml file for creation
        if (!file_.Open("123.xml", true))
        {
            ReleaseModelObjects();
            return exported;
        }
        // Materials
        std::cout << "WriteMaterials" << std::endl;
        WriteMaterials();
        // Geometry
        std::cout << "WriteGeometry" << std::endl;
        WriteGeometry();
        file_.Close(IsCancelled(progress_callback));
        
        // 在导出到GLTF之前压缩纹理
        CompressAndResizeTextures();
        
        exported = exportToGltfImpl(file_name, output_format, use_draco) == 0;
    }
    catch (...)
    {
        exported = false;
        file_.Close(true);
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

static XmlMaterialInfo GetMaterialInfo(SUMaterialRef material, const std::string &texture_directory)
{
    assert(SUIsValid(material));

    XmlMaterialInfo info;

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

void CXmlExporter::WriteMaterials()
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

void CXmlExporter::WriteMaterial(SUMaterialRef material)
{
    if (SUIsInvalid(material))
        return;

    XmlMaterialInfo info    = GetMaterialInfo(material, outPath);
    materialMap[info.name_] = info;
    if (!info.texture_path_.empty())
    {
        std::cout << info.texture_path_ << std::endl;
    }
    WriteMaterialsTextureImage(material, info.texture_path_);
    file_.WriteMaterialInfo(info);
}

void CXmlExporter::WriteGeometry()
{
    if (options_.export_faces() || options_.export_edges())
    {
        file_.StartGeometry();
        
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
        
        file_.PopParentNode();
    }
}

void CXmlExporter::ProcessGeometryBatch(SUEntitiesRef entities, 
                                      const SUTransformation& transformation,
                                      size_t batchSize,
                                      int parentNodeIdx) {
    size_t num_faces = 0;
    size_t num_groups = 0;
    size_t num_instances = 0;
    
    SU_CALL(SUEntitiesGetNumFaces(entities, &num_faces));
    SU_CALL(SUEntitiesGetNumGroups(entities, &num_groups));
    SU_CALL(SUEntitiesGetNumInstances(entities, &num_instances));
    
    // std::cout << "Processing entity with:" << std::endl;
    // std::cout << "- Faces: " << num_faces << std::endl;
    // std::cout << "- Groups: " << num_groups << std::endl; 
    // std::cout << "- Instances: " << num_instances << std::endl;
    
    // 处理直接的面
    if (num_faces > 0) {
        std::vector<SUFaceRef> faces(std::min<size_t>(batchSize, num_faces));
        
        for (size_t offset = 0; offset < num_faces; offset += batchSize) {
            size_t currentBatchSize = std::min<size_t>(batchSize, num_faces - offset);
            faces.resize(currentBatchSize);
            
            SU_CALL(SUEntitiesGetFaces(entities, currentBatchSize, &faces[0], &currentBatchSize));
            
            for (size_t i = 0; i < currentBatchSize; i++) {
                inheritance_manager_.PushElement(faces[i]);
                WriteFace(faces[i], transformation);
                inheritance_manager_.PopElement();
            }
        }
        faces.clear();
        faces.shrink_to_fit();
    } else {
        // 处理组和组件
        if (num_groups > 0) {
            traversalGroupEntity(entities, transformation, parentNodeIdx);
        }
        
        if (num_instances > 0) {
            getComponentEntity(entities, transformation, parentNodeIdx);
        }
    }
}

void CXmlExporter::WriteEntities(SUEntitiesRef entities, const SUTransformation &transformation, int parentNodeIdx)
{
    if (SUIsInvalid(entities)) {
        return;
    }
    
    // 使用 ProcessGeometryBatch 统一处理所有实体
    ProcessGeometryBatch(entities, transformation, DEFAULT_BATCH_SIZE, parentNodeIdx);
}
void CXmlExporter::traversalGroupEntity(SUEntitiesRef entities, const SUTransformation &transformation, int parentNodeIdx)
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
void CXmlExporter::getComponentEntity(SUEntitiesRef entities, const SUTransformation &transformation, int parentNodeIdx)
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

int CXmlExporter::exportToGltfImpl(const std::string &gltfName, const std::string &outputFormat, bool use_draco) {
    struct VertexData {
        float x, y, z;
        float nx, ny, nz;
        float u, v;

        bool operator==(const VertexData& o) const {
            return x == o.x && y == o.y && z == o.z &&
                   nx == o.nx && ny == o.ny && nz == o.nz &&
                   u == o.u && v == o.v;
        }
    };

    struct VertexDataHash {
        size_t operator()(const VertexData& v) const {
            size_t h1 = std::hash<float>()(v.x);
            size_t h2 = std::hash<float>()(v.y);
            size_t h3 = std::hash<float>()(v.z);
            size_t h4 = std::hash<float>()(v.nx);
            size_t h5 = std::hash<float>()(v.ny);
            size_t h6 = std::hash<float>()(v.nz);
            size_t h7 = std::hash<float>()(v.u);
            size_t h8 = std::hash<float>()(v.v);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5) ^ (h7 << 6) ^ (h8 << 7);
        }
    };

    tinygltf::Model model;
    model.asset.version = "2.0";
    model.asset.generator = "zhuzhaoyun";
    tinygltf::Scene scene;
    model.scenes.push_back(scene);
    model.defaultScene = 0;
    
    // 定义一个辅助Lambda用于添加Mesh
    auto addMeshLambda = [&](const std::unordered_map<Color, std::vector<cFacet>, colorHashFuc>& currentFacetMap, const std::string& meshName) -> int {
        if (currentFacetMap.empty()) return -1;
        
        tinygltf::Mesh mesh;
        mesh.name = meshName;
        
        for (auto &item : currentFacetMap) {
            tinygltf::Primitive primitive;
            primitive.mode = 4;  // triangles
            
            // 收集顶点、法线和索引数据
            std::vector<float> positions;
            std::vector<float> normals;
            std::vector<float> uvs;
            std::vector<unsigned int> indices;

            // 顶点复用缓存
            std::unordered_map<VertexData, unsigned int, VertexDataHash> vertexCache;

            // 添加用于计算边界的变量
            float posMin[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
            float posMax[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
            
            const std::vector<cFacet> &facetVec = item.second;
            size_t vertexOffset = 0;
            
            for (size_t i = 0; i < facetVec.size(); i++) {
                // 先提取三角形的三个顶点坐标
                float v[3][3] = {
                    {(float)facetVec[i].vertex[0].x, (float)facetVec[i].vertex[0].y, (float)facetVec[i].vertex[0].z},
                    {(float)facetVec[i].vertex[1].x, (float)facetVec[i].vertex[1].y, (float)facetVec[i].vertex[1].z},
                    {(float)facetVec[i].vertex[2].x, (float)facetVec[i].vertex[2].y, (float)facetVec[i].vertex[2].z}
                };
                
                // 计算面法线
                float edge1[3] = {v[1][0] - v[0][0], v[1][1] - v[0][1], v[1][2] - v[0][2]};
                float edge2[3] = {v[2][0] - v[0][0], v[2][1] - v[0][1], v[2][2] - v[0][2]};
                float nx = edge1[1] * edge2[2] - edge1[2] * edge2[1];
                float ny = edge1[2] * edge2[0] - edge1[0] * edge2[2];
                float nz = edge1[0] * edge2[1] - edge1[1] * edge2[0];
                float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (len > 1e-8f) { nx /= len; ny /= len; nz /= len; }
                else { nx = 0; ny = 0; nz = 1.0f; }
                
                for (int j = 0; j < 3; j++) {
                    float x = v[j][0], y = v[j][1], z = v[j][2];
                    float u = (float)facetVec[i].uv[j].x, v_coord = (float)(-facetVec[i].uv[j].y);
                    
                    VertexData vData = {x, y, z, nx, ny, nz, u, v_coord};
                    auto it = vertexCache.find(vData);
                    if (it != vertexCache.end()) {
                        indices.push_back(it->second);
                    } else {
                        unsigned int newIndex = vertexOffset++;
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
            // INDICES (Optimization: use USHORT if possible)
            {
                tinygltf::BufferView bv; bv.buffer = 0; bv.byteOffset = model.buffers[0].data.size();
                if (vertexOffset < 65535) {
                    std::vector<unsigned short> shortIndices(indices.begin(), indices.end());
                    bv.byteLength = shortIndices.size() * sizeof(unsigned short);
                    model.buffers[0].data.insert(model.buffers[0].data.end(), (uint8_t*)shortIndices.data(), (uint8_t*)shortIndices.data() + bv.byteLength);
                } else {
                    bv.byteLength = indices.size() * sizeof(unsigned int);
                    model.buffers[0].data.insert(model.buffers[0].data.end(), (uint8_t*)indices.data(), (uint8_t*)indices.data() + bv.byteLength);
                }
                bv.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
                int bvIdx = model.bufferViews.size(); model.bufferViews.push_back(bv);
                tinygltf::Accessor acc; acc.bufferView = bvIdx;
                acc.componentType = (vertexOffset < 65535) ? TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT : TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                acc.count = indices.size(); acc.type = TINYGLTF_TYPE_SCALAR;
                primitive.indices = model.accessors.size(); model.accessors.push_back(acc);
            }
            
            // MATERIAL
            tinygltf::Material material;
            material.pbrMetallicRoughness.baseColorFactor = {item.first.r, item.first.g, item.first.b, item.first.a};
            material.name = item.first.name; material.pbrMetallicRoughness.metallicFactor = 0.0; material.pbrMetallicRoughness.roughnessFactor = 1.0;
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
        // speed=10, bits: pos=11, tex=10, normal=8, color=8, generic=8
        dracoTool.encode(10, 11, 10, 8, 8, 8);
        
        // Save again with Draco extension
        ret = gltf.WriteGltfSceneToFile(&model, outputPath,
                                          true,  // embedImages
                                          true,  // embedBuffers
                                          true,  // prettyPrint
                                          writeBinary);
    }
    
    return ret ? 0 : 1;
}


void CXmlExporter::WriteFace(SUFaceRef face, const SUTransformation &transformation)
{
    if (SUIsInvalid(face))
        return;

    XmlFaceInfo info;

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

    std::vector<SUVector3D> normalArr(num_triangles);
    size_t num_normals = 0;
    SUMeshHelperGetNormals(mesh_ref, num_triangles, &normalArr[0], &num_normals);

    const size_t num_indices = 3 * num_triangles;
    size_t num_retrieved     = 0;
    std::vector<size_t> indices(num_indices);
    SU_CALL(SUMeshHelperGetVertexIndices(mesh_ref, num_indices, &indices[0], &num_retrieved));

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
    XmlMaterialInfo &materialInfo     = materialMap[info.front_mat_name_];
    XmlMaterialInfo &materialInfoBack = materialMap[info.back_mat_name_];
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
    
    std::vector<size_t> optimizedIndices;
    std::vector<Vector3> uniqueVerticesVec;
    std::vector<Vector3> uniqueUVsVec;

    for (size_t i = 0; i < num_triangles; i++) {
        for (size_t j = 0; j < 3; j++) {
            size_t index = indices[i * 3 + j];
            
            VertexKey key;
            // 转换顶点坐标
            double vertex[3] = {vertices[index].x, vertices[index].y, vertices[index].z};
            double transformed[3] = {0, 0, 0};
            
            for (int ii = 0; ii < 3; ii++) {
                for (int jj = 0; jj < 3; jj++) {
                    transformed[ii] += vertex[jj] * transformation.values[pos(ii, jj)];
                }
                transformed[ii] += transformation.values[pos(ii, 3)];
            }
            
            key.x = transformed[0];
            key.y = transformed[1];
            key.z = transformed[2];
            
            // 设置UV坐标
            if (info.has_front_texture_) {
                SUPoint3D stq = front_stq[index];
                key.u = stq.x * materialInfo.texture_sscale_;
                key.v = stq.y * materialInfo.texture_tscale_;
            } else {
                key.u = key.v = 0.0;
            }
            
            // 获取或创建新的顶点索引
            size_t optimizedIndex = GetOrCreateVertexIndex(key);
            optimizedIndices.push_back(optimizedIndex);
            
            if (optimizedIndex == uniqueVerticesVec.size()) {
                // 这是一个新顶点
                uniqueVerticesVec.push_back(Vector3(transformed[0], transformed[1], transformed[2]));
                if (info.has_front_texture_) {
                    uniqueUVsVec.push_back(Vector3(key.u, key.v, 0));
                }
            }
        }

        // 创建面片并添加到facetMap
        cFacet aFacet;
        // ... 设置法线和颜色 ...
        
        for (int j = 0; j < 3; j++) {
            size_t idx = optimizedIndices[i * 3 + j];
            aFacet.vertex[j] = uniqueVerticesVec[idx];
            if (info.has_front_texture_) {
                aFacet.uv[j] = uniqueUVsVec[idx];
            }
        }
        
        // 添加到facetMap
        Color color1((double)color.r, (double)color.g, (double)color.b, 
                    (double)color.a, color.imageUri, color.name);
        (*activeFacetMap_)[color1].push_back(aFacet);
    }

    // 清理本次处理的顶点缓存
    ClearVertexCache();
    
    SU_CALL(SUMeshHelperRelease(&mesh_ref));  // 及时释放mesh资源
    SU_CALL(SUUVHelperRelease(&uv_helper));
}

// 添加新的方法实现
void CXmlExporter::CompressAndResizeTextures() {
    for (auto& materialPair : materialMap) {
        const auto& materialInfo = materialPair.second;
        if (!materialInfo.texture_path_.empty()) {
            materialPair.second.picture_name_ = ProcessTexture(materialInfo.texture_path_);
        }
    }
}

std::string CXmlExporter::ProcessTexture(const std::string& texturePath) {
    return TextureProcessor::ProcessTexture(texturePath, 1024);
}

size_t CXmlExporter::GetOrCreateVertexIndex(const VertexKey& key) {
    auto it = vertexCache.find(key);
    if (it != vertexCache.end()) {
        return it->second;
    }
    
    size_t newIndex = uniqueVertices.size();
    vertexCache[key] = newIndex;
    uniqueVertices.push_back(key);
    return newIndex;
}

void CXmlExporter::ClearVertexCache() {
    vertexCache.clear();
    uniqueVertices.clear();
}
