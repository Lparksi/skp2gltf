add_library(gltflib 
    ${CMAKE_CURRENT_SOURCE_DIR}/creategltfcommon.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/parsegltfcommon.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/parsemeshdata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gltfdraco.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf_impl.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/tinygltf/tiny_gltf.h
    ${PROJECT_SOURCE_DIR}/thirdparty/tinygltf/stb_image.h
    ${PROJECT_SOURCE_DIR}/thirdparty/tinygltf/stb_image_write.h
    ${PROJECT_SOURCE_DIR}/thirdparty/tinygltf/stb_image_resize2.h
    ${PROJECT_SOURCE_DIR}/include/gltflib/gltfdraco.h
    ${PROJECT_SOURCE_DIR}/include/gltflib/parsemeshdata.h
)

target_include_directories(gltflib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(gltflib PUBLIC
    draco
)
