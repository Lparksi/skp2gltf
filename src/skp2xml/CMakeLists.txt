file(GLOB CPPFILE ./*.cpp)
add_library(skp2xml ${CPPFILE})

target_link_libraries(skp2xml PRIVATE
    gltflib
    tinyxml2
)
