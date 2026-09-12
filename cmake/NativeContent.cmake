# Source inputs stay checked in; all runtime content is produced by the C++ importer.
add_custom_target(native_sample_content
  COMMAND $<TARGET_FILE:hyperion_asset_tool> import "${PROJECT_SOURCE_DIR}/assets/Models/Showcase.gltf" "${PROJECT_SOURCE_DIR}/out/content/Models/Showcase.hasset" --library "${PROJECT_SOURCE_DIR}/out/content"
  COMMAND $<TARGET_FILE:hyperion_asset_tool> import "${PROJECT_SOURCE_DIR}/assets/Scenes/Showcase.json" "${PROJECT_SOURCE_DIR}/out/content/Scenes/Showcase.hasset" --library "${PROJECT_SOURCE_DIR}/out/content"
  COMMAND $<TARGET_FILE:hyperion_asset_tool> import "${PROJECT_SOURCE_DIR}/assets/Scenes/Shadows.json" "${PROJECT_SOURCE_DIR}/out/content/Scenes/Shadows.hasset" --library "${PROJECT_SOURCE_DIR}/out/content"
  COMMAND $<TARGET_FILE:hyperion_asset_tool> import "${PROJECT_SOURCE_DIR}/assets/Scenes/SharedAssets.json" "${PROJECT_SOURCE_DIR}/out/content/Scenes/SharedAssets.hasset" --library "${PROJECT_SOURCE_DIR}/out/content"
  COMMAND $<TARGET_FILE:hyperion_asset_tool> import "${PROJECT_SOURCE_DIR}/assets/Scenes/Sponza.json" "${PROJECT_SOURCE_DIR}/out/content/Scenes/Sponza.hasset" --library "${PROJECT_SOURCE_DIR}/out/content"
  COMMAND $<TARGET_FILE:hyperion_asset_tool> catalog "${PROJECT_SOURCE_DIR}/out/content/Catalog.hasset" "${PROJECT_SOURCE_DIR}/out/content/Models/Showcase.hasset" "${PROJECT_SOURCE_DIR}/out/content/Scenes/Showcase.hasset" "${PROJECT_SOURCE_DIR}/out/content/Scenes/Shadows.hasset" "${PROJECT_SOURCE_DIR}/out/content/Scenes/SharedAssets.hasset" "${PROJECT_SOURCE_DIR}/out/content/Scenes/Sponza.hasset"
  DEPENDS hyperion_asset_tool
  COMMENT "Verify and build native sample assets"
  VERBATIM)
add_dependencies(hyperion_viewer native_sample_content)
