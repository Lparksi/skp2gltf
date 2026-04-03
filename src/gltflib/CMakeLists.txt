add_library(gltflib 
    ${CMAKE_CURRENT_SOURCE_DIR}/creategltfcommon.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/parsegltfcommon.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/parsemeshdata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gltfdraco.cpp
    ${PROJECT_SOURCE_DIR}/include/gltflib/gltfdraco.h
    ${PROJECT_SOURCE_DIR}/include/gltflib/parsemeshdata.h
)

target_include_directories(gltflib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(gltflib PUBLIC
    draco
)

target_include_directories(gltflib PUBLIC
    ${draco_SOURCE_DIR}/src
    ${draco_BINARY_DIR}
)