# Included in the Viewer's directory: source properties and IDE filters have directory scope.
set(viewer_content shaders/Common.hlsli shaders/Triangle.hlsl shaders/Gui.hlsl shaders/Model.hlsl experiments/Triangle.json experiments/Model.json
  README.md docs/Architecture.md docs/SourceLayout.md docs/Dependencies.md docs/VisualStudio.md docs/CodingStyle.md docs/AssetPipeline.md docs/RenderDoc.md
  AGENTS.md .clang-format .clang-tidy .editorconfig .gitattributes
  GenerateSolution.cmd tools/GenerateSolution.ps1 tools/VisualStudio.py tools/CheckStyle.py)
list(TRANSFORM viewer_content PREPEND "${PROJECT_SOURCE_DIR}/")
set_source_files_properties(${viewer_content} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(hyperion_viewer PRIVATE ${viewer_content})
source_group(TREE "${PROJECT_SOURCE_DIR}" FILES ${viewer_content})
