vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_download_distfile(
    ARCHIVE_FILE
    URLS "https://github.com/syoyo/tinyexr/archive/v${VERSION}.tar.gz"
    FILENAME "syoyo-tinyexr-v${VERSION}.tar.gz"
    SHA512 74b9b72f58198ebf09c41f1bea04d24f9c13996411cb55f21ddb7732646ca9ddee7cf1fd538888a26d670fa73e168ad901c2a92fb23c7839a2821a79855a2350
)

set(SOURCE_PATH "${CURRENT_BUILDTREES_DIR}/src/v${VERSION}-${TARGET_TRIPLET}.clean")
set(EXTRACT_PATH "${CURRENT_BUILDTREES_DIR}/src/v${VERSION}-${TARGET_TRIPLET}.extract")

file(REMOVE_RECURSE "${SOURCE_PATH}" "${EXTRACT_PATH}")
file(MAKE_DIRECTORY "${EXTRACT_PATH}")

vcpkg_execute_required_process(
    COMMAND tar -xf "${ARCHIVE_FILE}"
    WORKING_DIRECTORY "${EXTRACT_PATH}"
    LOGNAME extract
)

file(GLOB EXTRACTED_DIRS LIST_DIRECTORIES true "${EXTRACT_PATH}/*")
list(LENGTH EXTRACTED_DIRS EXTRACTED_DIR_COUNT)
if(NOT EXTRACTED_DIR_COUNT EQUAL 1)
    message(FATAL_ERROR "Unexpected tinyexr archive layout.")
endif()

list(GET EXTRACTED_DIRS 0 EXTRACTED_DIR)
file(RENAME "${EXTRACTED_DIR}" "${SOURCE_PATH}")
file(REMOVE_RECURSE "${EXTRACT_PATH}")

vcpkg_apply_patches(
    SOURCE_PATH "${SOURCE_PATH}"
    PATCHES "${CMAKE_CURRENT_LIST_DIR}/fixtargets.patch"
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DTINYEXR_BUILD_SAMPLE=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/README.md" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)

