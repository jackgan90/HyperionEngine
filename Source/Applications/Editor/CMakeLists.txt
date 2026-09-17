add_executable(hyperion_editor
  Private/Main.cpp Private/Editor.manifest
  Private/EditorApplication.cpp Private/EditorPanels.cpp Private/EditorFrame.cpp Private/EditorAcceptance.cpp
  Private/EditorBenchmark.cpp Private/EditorDocument.cpp Private/EditorInspector.cpp Private/EditorDocumentAcceptance.cpp)
hyp_executable(hyperion_editor)
target_sources(hyperion_editor PRIVATE Private/EditorTransformAcceptance.cpp)
target_sources(hyperion_editor PRIVATE Private/EditorViews.cpp Private/EditorHistory.cpp)
target_sources(hyperion_editor PRIVATE Private/EditorViewAcceptance.cpp)
target_sources(hyperion_editor PRIVATE "${PROJECT_SOURCE_DIR}/Source/Applications/Resources/Hyperion.rc")
target_include_directories(hyperion_editor PRIVATE "${PROJECT_SOURCE_DIR}/Source/Applications/Resources")
target_link_libraries(hyperion_editor PRIVATE hyperion_gui hyperion_gui_renderer hyperion_render hyperion_assets
  hyperion_asset_types hyperion_scene hyperion_materials hyperion_platform hyperion_io hyperion_core hyperion_tasks
  hyperion_shaders hyperion_rhi hyperion_rhi_d3d12 hyperion_math)
target_compile_definitions(hyperion_editor PRIVATE HYP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_target_properties(hyperion_editor PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
