vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO CharmedBaryon/CommonLibSSE
        REF 9b7c386d0355e756b4153416a76b0532f755f8de
        SHA512   6c9e4861a985eda04074ee157996a5f5294d3e2ce5c79ff481c4b11c86caf5a49847d31d9b5509fc1ae786c01438364f98de6ed3dc28b84dc408bde23dbfa057
        HEAD_REF main
        PATCHES SKSE_RUNTIME_n_fmt.patch
)

vcpkg_configure_cmake(
        SOURCE_PATH "${SOURCE_PATH}"
        PREFER_NINJA
        OPTIONS -DBUILD_TESTS=off -DSKSE_SUPPORT_XBYAK=on
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME "CommonLibSSE-ng" CONFIG_PATH "lib/cmake/CommonLibSSE")
vcpkg_copy_pdbs()

file(GLOB CMAKE_CONFIGS "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSE/*.cmake")
file(INSTALL ${CMAKE_CONFIGS} DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE")
file(INSTALL "${SOURCE_PATH}/cmake/CommonLibSSE.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE-ng")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSE")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")