# Can't believe I had to go through that, but test aren't built automatically by cmake,
# so we need a special dependency hack. See https://stackoverflow.com/a/10824578
function (add_dl_test)
    add_test (NAME "${ARGV0}_BUILD" COMMAND "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target "${ARGV0}" "--config" "$<CONFIG>" )
    add_test ("${ARGV0}" "${ARGV0}")
    add_executable("${ARGV0}" "${ARGV0}.cpp")
    set_tests_properties("${ARGV0}" PROPERTIES DEPENDS "${ARGV0}_BUILD")
    # Common lib for all tests.
    target_link_libraries("${ARGV0}" daltonGUI)
endfunction()

add_definitions(-DTEST_IMAGES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/images/")

add_dl_test (test_Utils)
add_dl_test (test_Filters)
