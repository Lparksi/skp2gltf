/*
 * @Description: 
 * @Author: yaol
 * @Date: 2021-08-06 10:22:27
 * @LastEditTime: 2021-08-06 15:07:37
 * @LastEditors: yaol
 * @FilePath: \common\xmlinheritancemanager.h
 */
// Copyright 2013 Trimble Navigation Limited. All Rights Reserved.

#ifndef SKPTOXML_COMMON_SKPINHERITANCEMANAGER_H
#define SKPTOXML_COMMON_SKPINHERITANCEMANAGER_H

#include "skp_geom_utils.h"
#include <SketchUpAPI/color.h>
#include <SketchUpAPI/model/defs.h>
#include <vector>

// CSkpInheritanceManager - A cross-platform class that manages the properties
// of geometric elements (faces and edges) that can be inherited from component
// instances, groups and images.  These properties are transformations to world
// space, layers and materials.
class CSkpInheritanceManager {
 public:
  CSkpInheritanceManager();
  CSkpInheritanceManager(bool bMaterialsByLayer);
  virtual ~CSkpInheritanceManager();

  void PushElement(SUGroupRef element);
  void PushElement(SUImageRef element);
  void PushElement(SUFaceRef element);
  void PushElement(SUEdgeRef element);
  void PushElement(SUComponentDefinitionRef element);
  void PushElement(SUComponentInstanceRef group) ;
  void PopElement();

  SULayerRef GetCurrentLayer() const;
  SUMaterialRef GetCurrentFrontMaterial() const;
  SUMaterialRef GetCurrentBackMaterial() const;
  SUColor GetCurrentEdgeColor() const;

 protected: //Methods
  void PushMaterial(SUDrawingElementRef drawing_element);
  void PushLayer(SUDrawingElementRef drawing_element);

 protected: //Data
  bool materials_by_layer_;
  std::vector<SULayerRef> layers_;
  std::vector<SUMaterialRef> front_materials_;
  std::vector<SUMaterialRef> back_materials_;
  std::vector<SUColor> edge_colors_;
};

#endif // SKPTOXML_COMMON_SKPINHERITANCEMANAGER_H
