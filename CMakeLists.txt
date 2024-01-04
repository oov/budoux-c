cmake_minimum_required(VERSION 3.20)

option(FORMAT_SOURCES "execute clang-format" ON)
option(USE_ADDRESS_SANITIZER "use address sanitizer" OFF)
option(BUDOUXC_EMBED_MODELS "embed pretrained models" ON)
option(TARGET_WASI_SDK "target wasi-sdk" OFF)

project(budouxc C)
enable_testing()

if(FORMAT_SOURCES)
  file(GLOB sources LIST_DIRECTORIES false CONFIGURE_DEPENDS "*.h" "*.c")
  find_program(CLANG_FORMAT_EXE clang-format)
  add_custom_target(${PROJECT_NAME}-format ALL
    COMMAND ${CLANG_FORMAT_EXE} -style=file -i ${sources}
  )
endif()

include(FetchContent)
FetchContent_Declare(
  budoux
  GIT_REPOSITORY https://github.com/google/budoux
  GIT_TAG 8df9b5d936a14f37a9ea45d22f64ee5b7c16fc8a
)
FetchContent_Declare(
  json-parser
  GIT_REPOSITORY https://github.com/json-parser/json-parser
  GIT_TAG 94f66d8f83c1d84ccccd7540ec2f51cf8325d272
)
FetchContent_Declare(
  hashmap-c
  GIT_REPOSITORY https://github.com/oov/hashmap.c
  GIT_TAG ab8f0085c75e0928ad58adfbb1981711710332c0
)
FetchContent_MakeAvailable(budoux json-parser hashmap-c)

set(embedded_models ${CMAKE_CURRENT_BINARY_DIR}/embedded_models)
find_program(XXD xxd REQUIRED CMAKE_FIND_ROOT_PATH_BOTH)
add_custom_target(generate_budoux_models SOURCES
  ${budoux_SOURCE_DIR}/budoux/models/ja.json
  ${budoux_SOURCE_DIR}/budoux/models/zh-hans.json
  ${budoux_SOURCE_DIR}/budoux/models/zh-hant.json
)
add_custom_command(
OUTPUT
  ${embedded_models}/ja.c
  ${embedded_models}/zh_hans.c
  ${embedded_models}/zh_hant.c
COMMAND
  ${CMAKE_COMMAND} -E make_directory ${embedded_models}
COMMAND
  xxd -i ja.json > ${embedded_models}/ja.c
COMMAND
  xxd -i zh-hans.json > ${embedded_models}/zh_hans.c
COMMAND
  xxd -i zh-hant.json > ${embedded_models}/zh_hant.c
DEPENDS
  ${budoux_SOURCE_DIR}/budoux/models/ja.json
  ${budoux_SOURCE_DIR}/budoux/models/zh-hans.json
  ${budoux_SOURCE_DIR}/budoux/models/zh-hant.json
WORKING_DIRECTORY
  ${budoux_SOURCE_DIR}/budoux/models
)

add_library(budouxc
  budoux-c.c
  ${json-parser_SOURCE_DIR}/json.c
  ${hashmap-c_SOURCE_DIR}/hashmap.c
  $<$<BOOL:${BUDOUXC_EMBED_MODELS}>:${embedded_models}/ja.c ${embedded_models}/zh_hans.c ${embedded_models}/zh_hant.c>
)
target_include_directories(budouxc PRIVATE ${json-parser_SOURCE_DIR} ${hashmap-c_SOURCE_DIR})
target_compile_definitions(budouxc PRIVATE $<$<NOT:$<BOOL:${BUDOUXC_EMBED_MODELS}>>:BUDOUXC_NO_EMBEDDED_MODELS> $<$<STREQUAL:$<TARGET_PROPERTY:budouxc,TYPE>,SHARED_LIBRARY>:BUDOUXC_SHARED> BUDOUXC_EXPORT)
target_link_libraries(budouxc PRIVATE m)
install(TARGETS budouxc
        EXPORT libbudouxc
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
install(
FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/budoux-c.h
DESTINATION
  include
)
add_dependencies(budouxc generate_budoux_models)

set(is_clang "$<C_COMPILER_ID:Clang>")
set(v16_or_later "$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,16>")
set(compile_options
-Wall
-Wextra
-Werror
-Weverything
-Wshadow
-Werror=return-type
-pedantic-errors
-Wno-declaration-after-statement
-Wno-padded
$<$<AND:${is_clang},${v16_or_later}>:-Wno-unsafe-buffer-usage>
)
set_source_files_properties(budoux-c.c example.c PROPERTIES COMPILE_OPTIONS "${compile_options}")

target_compile_options(budouxc
PRIVATE
  $<$<BOOL:${USE_ADDRESS_SANITIZER}>:-fsanitize=address>
)
target_link_options(budouxc
PRIVATE
  $<$<BOOL:${USE_ADDRESS_SANITIZER}>:-fsanitize=address>
  $<$<AND:$<STREQUAL:${CMAKE_BUILD_TYPE},Release>,$<STREQUAL:$<TARGET_PROPERTY:budouxc,TYPE>,SHARED_LIBRARY>>:-s>
)

if(BUDOUXC_EMBED_MODELS)
  add_executable(budouxc_example example.c)
  target_link_libraries(budouxc_example budouxc)

  add_executable(test_budouxc_callback test_callback.c)
  target_link_libraries(test_budouxc_callback budouxc)
  if(TARGET_WASI_SDK)
    add_test(NAME test_budouxc_callback COMMAND wasmtime test_budouxc_callback)
  else()
    add_test(NAME test_budouxc_callback COMMAND test_budouxc_callback)
  endif()
endif()
