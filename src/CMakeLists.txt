add_subdirectory(bin)
add_subdirectory(libs)
set(ALL_PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_WRITE GROUP_EXECUTE
  WORLD_READ WORLD_WRITE WORLD_EXECUTE)
install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/src/archive
  DESTINATION .
  DIRECTORY_PERMISSIONS ${ALL_PERMISSIONS})
install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/src/comm
  DESTINATION .
  DIRECTORY_PERMISSIONS ${ALL_PERMISSIONS})
