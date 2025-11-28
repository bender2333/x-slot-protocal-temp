# Demo CMakeLists.txt

# 边缘节点示例
add_executable(demo_edge_node demo_edge_node.cpp)
target_link_libraries(demo_edge_node PRIVATE xslot)

# 汇聚节点示例
add_executable(demo_hub_node demo_hub_node.cpp)
target_link_libraries(demo_hub_node PRIVATE xslot)

# HMI 示例
add_executable(demo_hmi demo_hmi.cpp)
target_link_libraries(demo_hmi PRIVATE xslot)

