/*
 * @Description:
 * @Author: yaol
 * @Date: 2021-08-06 10:22:27
 * @LastEditTime: 2026-04-06 09:04:41
 * @LastEditors: Antigravity
 * @FilePath: include/skp/skp_exporter.h
 */
// Copyright 2013 Trimble Navigation Limited. All Rights Reserved.

#ifndef SKPTOXML_COMMON_SKPEXPORTER_H
#define SKPTOXML_COMMON_SKPEXPORTER_H

#include "skp_stats.h"
#include "skp_options.h"
#include "skp_inheritance_manager.h"
#include <deque>
#include <vector>
#include <unordered_map>
#include <map>
#include <string>

#include <SketchUpAPI/import_export/pluginprogresscallback.h>
#include <SketchUpAPI/model/defs.h>
#include <SketchUpAPI/model/transformation.h>
#include <SketchUpAPI/model/color.h>

struct SkpMaterialInfo {
    std::string name_;
    bool has_color_;
    SUColor color_;
    bool has_alpha_;
    double alpha_;
    bool has_texture_;
    std::string texture_path_;
    std::string picture_name_;
    double texture_sscale_;
    double texture_tscale_;
};

struct SkpFaceInfo {
    std::string layer_name_;
    std::string front_mat_name_;
    std::string back_mat_name_;
    bool has_front_texture_;
    bool has_back_texture_;
    bool has_single_loop_;
};

struct SkpEdgeInfo {
    std::string layer_name_;
    std::string mat_name_;
    bool has_single_loop_;
};

class Color
{
  public:
    Color(double _r, double _g, double _b, double _a, std::string _imageUri, std::string _name)
        : r(_r), g(_g), b(_b), a(_a), imageUri(_imageUri), name(_name)
    {}
    Color() {}
    double r;
    double g;
    double b;
    double a;
    std::string name;
    std::string imageUri = "";
    bool operator==(const Color &t) const { return r == t.r && g == t.g && b == t.b && a == t.a && imageUri == t.imageUri; }
};

struct colorHashFuc
{
    size_t operator()(const Color &key) const
    {
        return ((std::hash<int>()((int)key.r * 10000) ^ (std::hash<int>()((int)key.g * 10000) << 1)) >> 1) ^
               (std::hash<int>()((int)key.b * 10000) << 1);
    }
};

class Vector3
{
  public:
    Vector3() {}
    Vector3(double _x, double _y, double _z)
    {
        x = _x;
        y = _y;
        z = _z;
    }
    inline bool empty() { return x == -1 && y == -1 && z == -1; }
    double x = -1, y = -1, z = -1;
    Vector3 Cross(const Vector3 &V)
    {
        Vector3 v;
        v.x = y * V.z - z * V.y;
        v.y = z * V.x - x * V.z;
        v.z = x * V.y - y * V.x;
        return v;
    }
    double Dot(const Vector3 &V)
    {
        double res;
        res = x * V.x + y * V.y + z * V.z;
        return res;
    }
    Vector3 operator-(const Vector3 V)
    {
        Vector3 v;
        v.x = x - V.x;
        v.y = y - V.y;
        v.z = z - V.z;
        return v;
    }
};

class cFacet
{
  public:
    Vector3 normal, vertex[3], uv[3];
    double color[4];
    Vector3 Middle()
    {
        Vector3 v;
        v.x = v.y = v.z = 0;
        for (int i = 0; i < 3; i++)
        {
            v.x += vertex[i].x;
            v.y += vertex[i].y;
            v.z += vertex[i].z;
        }
        v.x /= 3;
        v.y /= 3;
        v.z /= 3;
        return v;
    }
};

struct MeshInfo {
    std::unordered_map<Color, std::vector<cFacet>, colorHashFuc> facetMap;
    std::string name;
};

struct NodeInfo {
    int meshIndex = -1;
    std::string name;
    double matrix[16];
    std::vector<int> children;
};

class CSkpExporter
{
  public:
    CSkpExporter();
    virtual ~CSkpExporter();

    // Convert
    bool Convert(const std::string &from_file,
                 const std::string &file_path,
                 const std::string &file_name,
                 const std::string &output_format,
                 bool use_draco,
                 SketchUpPluginProgressCallback *callback);

    // Set user options
    CSkpOptions &options() { return options_; }
    CSkpExportStats &stats() { return stats_; }

    int exportToGltfImpl(const std::string &gltfName, const std::string &outputFormat, bool use_draco);
    void addFace(SUEntitiesRef entities, const SUTransformation &transformation);
    void getComponentEntity(SUEntitiesRef entities, const SUTransformation &transformation, int parentNodeIdx);

  private:
    // Clean up slapi objects
    void ReleaseModelObjects();

    void WriteMaterials();
    void WriteMaterial(SUMaterialRef material);

    void WriteGeometry();
    void WriteEntities(SUEntitiesRef entities, const SUTransformation &transformation, int parentNodeIdx);
    void traversalGroupEntity(SUEntitiesRef entities, const SUTransformation &transformation, int parentNodeIdx);
    void WriteFace(SUFaceRef face, const SUTransformation &transformation);
    void WriteEdge(SUEdgeRef edge);
    void WriteCurve(SUCurveRef curve);
    void addFaces(const SUTransformation &transformation);

    // 添加新的辅助方法用于分批处理
    void ProcessGeometryBatch(SUEntitiesRef entities, const SUTransformation& transformation, size_t batchSize, int parentNodeIdx);
    
    // 用于批处理的缓存
    std::vector<SUFaceRef> faceBuffer;
    static const size_t DEFAULT_BATCH_SIZE = 1000;

    // 用于顶点去重的结构
    struct VertexKey {
        double x, y, z;
        double u, v;  
        
        bool operator<(const VertexKey& other) const {
            const double EPSILON = 1e-7;
            if (std::abs(x - other.x) > EPSILON) return x < other.x;
            if (std::abs(y - other.y) > EPSILON) return y < other.y;
            if (std::abs(z - other.z) > EPSILON) return z < other.z;
            if (std::abs(u - other.u) > EPSILON) return u < other.u;
            return v < other.v;
        }
    };

    // 顶点缓存
    std::map<VertexKey, size_t> vertexCache;
    std::vector<VertexKey> uniqueVertices;
    
    // 用于存储压缩后的纹理信息
    struct CompressedTexture {
        std::string originalPath;
        std::string compressedPath;
        int width;
        int height;
        bool isCompressed;
    };
    
    std::map<std::string, CompressedTexture> textureCache;
    
    // 新增的辅助方法
    void CompressAndResizeTextures();
    std::string ProcessTexture(const std::string& texturePath);
    size_t GetOrCreateVertexIndex(const VertexKey& key);
    void ClearVertexCache();

  private:
    CSkpOptions options_;
    CSkpExportStats stats_;

    // SLAPI model and texture writer
    SUModelRef model_;
    SUTextureWriterRef texture_writer_;

    // Stack
    CSkpInheritanceManager inheritance_manager_;

    std::unordered_map<std::string, SkpMaterialInfo> materialMap;
    std::unordered_map<Color, std::vector<cFacet>, colorHashFuc> facetMap; // Root mesh
    std::unordered_map<Color, std::vector<cFacet>, colorHashFuc>* activeFacetMap_ = nullptr;
    
    std::map<void*, int> definitionToMeshIndex;
    std::deque<MeshInfo> meshList;
    std::vector<NodeInfo> nodeList;
    std::string outPath;
    double ratio = 1;
};

#endif  // SKPTOXML_COMMON_SKPEXPORTER_H
