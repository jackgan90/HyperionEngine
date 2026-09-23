add_library(hyperion_scene_editing STATIC Private/SceneDocument.cpp Private/SceneHistory.cpp Private/SceneSave.cpp
  Private/SceneQueries.cpp Private/SceneRequestTypes.cpp Private/SceneStructure.cpp)
hyp_module(hyperion_scene_editing)
target_link_libraries(hyperion_scene_editing PUBLIC hyperion_scene hyperion_tasks PRIVATE hyperion_core hyperion_io)
