// Copyright 2013 Trimble Navigation Limited. All Rights Reserved.

#ifndef SKPTOXML_COMMON_SKPOPTIONS_H
#define SKPTOXML_COMMON_SKPOPTIONS_H

class CSkpOptions {
 public:
  CSkpOptions(void) {
   export_materials_ = true;
   export_faces_ = true;
   export_edges_ = true;
   export_materials_by_layer_ = false;
   export_layers_ = true;
   export_options_ = false;
   
   draco_speed_ = 5;
   draco_position_bits_ = 14;
   draco_tex_bits_ = 12;
   draco_normal_bits_ = 10;
   draco_color_bits_ = 8;
   draco_generic_bits_ = 8;
   
   texture_max_resolution_ = 1024;
   vertex_weld_epsilon_ = 1e-6;
   
   use_ktx2_ = false;
   ktx2_quality_ = 128;
   ktx2_uastc_ = true;
  }

  virtual ~CSkpOptions(void) {}

  inline bool export_materials() const { return export_materials_; }
  inline void set_export_materials(bool value) { export_materials_ = value; }

  inline bool export_faces() const { return export_faces_; }
  inline void set_export_faces(bool value) { export_faces_ = value; }

  inline bool export_edges() const { return export_edges_; }
  inline void set_export_edges(bool value) { export_edges_ = value; }

  inline bool export_materials_by_layer() const {
      return export_materials_by_layer_;
  }
  inline void set_export_materials_by_layer(bool value) {
      export_materials_by_layer_ = value;
  }

  inline bool export_layers() const { return export_layers_; }
  inline void set_export_layers(bool value) { export_layers_ = value; }

  inline bool export_options() const { return export_options_; }
  inline void set_export_options(bool value) { export_options_ = value; }

  inline int draco_speed() const { return draco_speed_; }
  inline void set_draco_speed(int value) { draco_speed_ = value; }

  inline int draco_position_bits() const { return draco_position_bits_; }
  inline void set_draco_position_bits(int value) { draco_position_bits_ = value; }

  inline int draco_tex_bits() const { return draco_tex_bits_; }
  inline void set_draco_tex_bits(int value) { draco_tex_bits_ = value; }

  inline int draco_normal_bits() const { return draco_normal_bits_; }
  inline void set_draco_normal_bits(int value) { draco_normal_bits_ = value; }

  inline int draco_color_bits() const { return draco_color_bits_; }
  inline void set_draco_color_bits(int value) { draco_color_bits_ = value; }

  inline int draco_generic_bits() const { return draco_generic_bits_; }
  inline void set_draco_generic_bits(int value) { draco_generic_bits_ = value; }

  inline int texture_max_resolution() const { return texture_max_resolution_; }
  inline void set_texture_max_resolution(int value) { texture_max_resolution_ = value; }

  inline double vertex_weld_epsilon() const { return vertex_weld_epsilon_; }
  inline void set_vertex_weld_epsilon(double value) { vertex_weld_epsilon_ = value; }

  inline bool use_ktx2() const { return use_ktx2_; }
  inline void set_use_ktx2(bool value) { use_ktx2_ = value; }

  inline int ktx2_quality() const { return ktx2_quality_; }
  inline void set_ktx2_quality(int value) { ktx2_quality_ = value; }

  inline bool ktx2_uastc() const { return ktx2_uastc_; }
  inline void set_ktx2_uastc(bool value) { ktx2_uastc_ = value; }

 private:
  bool export_materials_;
  bool export_faces_;
  bool export_edges_;
  bool export_materials_by_layer_;
  bool export_layers_;
  bool export_options_;
  
  int draco_speed_;
  int draco_position_bits_;
  int draco_tex_bits_;
  int draco_normal_bits_;
  int draco_color_bits_;
  int draco_generic_bits_;
  
  int texture_max_resolution_;
  double vertex_weld_epsilon_;
  
  bool use_ktx2_;
  int ktx2_quality_;
  bool ktx2_uastc_;
};

#endif // SKPTOXML_COMMON_SKPOPTIONS_H
