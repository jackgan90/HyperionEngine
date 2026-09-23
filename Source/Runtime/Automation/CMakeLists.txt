add_library(hyperion_automation STATIC Private/Operation.cpp Private/Catalog.cpp Private/Jobs.cpp Private/Endpoint.cpp Private/Mcp.cpp)
hyp_module(hyperion_automation)
target_link_libraries(hyperion_automation PUBLIC hyperion_reflection)
