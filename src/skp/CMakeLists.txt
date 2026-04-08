file(GLOB CPPFILE ./*.cpp)
add_library(skp2native ${CPPFILE})

target_link_libraries(skp2native PRIVATE
    gltflib
)
