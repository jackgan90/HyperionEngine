add_library(hyperion_scene_editing STATIC Private/SceneDocument.cpp Private/SceneHistory.cpp Private/SceneSave.cpp
  Private/SceneQueries.cpp Private/SceneRequestTypes.cpp Private/SceneStructure.cpp
  Private/SceneAuthoring.cpp Private/SceneAuthoringTypes.cpp
  Private/SceneComponentEditing.cpp Private/SceneComponentEditingTypes.cpp Private/SceneDocumentHost.cpp)
hyp_module(hyperion_scene_editing)
target_sources(hyperion_scene_editing PRIVATE Private/ScenePlacement.cpp)
target_link_libraries(hyperion_scene_editing PUBLIC hyperion_scene hyperion_tasks PRIVATE hyperion_core hyperion_io)
