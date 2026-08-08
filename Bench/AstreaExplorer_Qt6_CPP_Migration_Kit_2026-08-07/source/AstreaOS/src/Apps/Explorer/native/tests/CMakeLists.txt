qt_add_executable(tst_native_bootstrap
    tst_native_bootstrap.cpp
)

target_link_libraries(tst_native_bootstrap PRIVATE
    Qt6::Core
    Qt6::Test
)

target_compile_definitions(tst_native_bootstrap PRIVATE
    ASTREA_EXPLORER_BIN="${ASTREA_EXPLORER_BIN}"
)

add_test(NAME native_bootstrap COMMAND tst_native_bootstrap)
