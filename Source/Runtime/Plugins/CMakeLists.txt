add_library(hyperion_plugins STATIC Private/PluginRuntime.cpp Private/PluginPlan.cpp Private/PluginServices.cpp)
hyp_module(hyperion_plugins)
target_link_libraries(hyperion_plugins PRIVATE hyperion_core)
