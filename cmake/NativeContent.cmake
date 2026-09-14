# Keep the target name stable. Published content is prepared explicitly with PrepareContent.py.
add_custom_target(native_sample_content
  COMMAND ${CMAKE_COMMAND} -E echo "Sample content is supplied by the mounted HyperionAssets repository"
  VERBATIM)
