add_library(hyperion_io STATIC Private/IOService.cpp Private/Adapters/FileSystem.cpp)
hyp_module(hyperion_io)
target_link_libraries(hyperion_io PUBLIC hyperion_tasks)
