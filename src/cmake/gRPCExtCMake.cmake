function (build_ext_grpc)
    set(GRPC_INST_PREFIX ${ARGV0})
    set(CMAKELIST_CONTENT "
    cmake_minimum_required(VERSION ${CMAKE_MINIMUM_REQUIRED_VERSION})
    project(build_ext_grpc)
    include(ExternalProject)

    ExternalProject_Add(ep_grpc

        PREFIX ${CMAKE_BINARY_DIR}/external
        TIMEOUT 300

        GIT_REPOSITORY https://github.com/grpc/grpc

        GIT_TAG v1.46.2

        GIT_SHALLOW 1

        CMAKE_ARGS -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${GRPC_INST_PREFIX}

        BUILD_COMMAND make -j

        BUILD_ALWAYS 0

        LOG_CONFIGURE 1
        LOG_UPDATE 1
        LOG_PATCH 1
        LOG_BUILD 1
        LOG_INSTALL 1
    )
    
    set(GRPC_PATCH_CMD git am -3 ${CMAKE_SOURCE_DIR}/src/external/patches/grpc/0001-support-vsock_v1.46.2.patch)
    ExternalProject_Get_property(ep_grpc SOURCE_DIR)
    ExternalProject_Add_Step(ep_grpc patch_vsock
        DEPENDEES update
        WORKING_DIRECTORY \${SOURCE_DIR}
        COMMAND \${GRPC_PATCH_CMD}
        LOG 1
    )

    add_custom_target(build_grpc)
    add_dependencies(build_grpc ep_grpc)
    ")

    set(TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/ExternalProjects/grpc")

    file(WRITE "${TARGET_DIR}/CMakeLists.txt" "${CMAKELIST_CONTENT}")

    file(MAKE_DIRECTORY "${TARGET_DIR}" "${TARGET_DIR}/build")

    execute_process(COMMAND ${CMAKE_COMMAND}
        ..
        WORKING_DIRECTORY "${TARGET_DIR}/build")

    execute_process(COMMAND ${CMAKE_COMMAND}
        --build .
        WORKING_DIRECTORY "${TARGET_DIR}/build")

endfunction()

set(GRPC_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/INSTALL/grpc)


find_package(Protobuf
    CONFIG
    PATHS ${GRPC_INSTALL_PREFIX}
    NO_DEFAULT_PATH
)

find_package(gRPC
    CONFIG
    PATHS ${GRPC_INSTALL_PREFIX}
    NO_DEFAULT_PATH
)


if((NOT Protobuf_FOUND) OR (NOT gRPC_FOUND))
    build_ext_grpc(${GRPC_INSTALL_PREFIX})
    find_package(Protobuf
        CONFIG
        REQUIRED
        PATHS ${GRPC_INSTALL_PREFIX}
        NO_DEFAULT_PATH
    )

    find_package(gRPC
        CONFIG
        REQUIRED
        PATHS ${GRPC_INSTALL_PREFIX}
        NO_DEFAULT_PATH
    )
endif()


message(STATUS "Using protobuf ${Protobuf_VERSION}")
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
message(STATUS "libproto: ${_PROTOBUF_LIBPROTOBUF}")
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
message(STATUS "${_PROTOBUF_PROTOC}")


message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP gRPC::grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)