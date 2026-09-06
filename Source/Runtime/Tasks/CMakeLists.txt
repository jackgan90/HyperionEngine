add_library(hyperion_tasks STATIC Private/Adapters/TaskSystem.cpp)
hyp_module(hyperion_tasks)
target_link_libraries(hyperion_tasks PUBLIC hyperion_core PRIVATE TBB::tbb)
