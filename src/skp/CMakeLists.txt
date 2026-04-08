file(GLOB CPPFILE ./*.cpp)
add_library(skp2native ${CPPFILE})

target_include_directories(skp2native PRIVATE
    ${PROJECT_SOURCE_DIR}/include/skp
    ${PROJECT_SOURCE_DIR}/thirdparty/SketchUpAPI
    ${PROJECT_SOURCE_DIR}/thirdparty
)

target_link_libraries(skp2native PRIVATE
    gltflib
    meshoptimizer
)
