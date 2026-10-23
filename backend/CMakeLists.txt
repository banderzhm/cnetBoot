cmake_minimum_required(VERSION 3.30)
project(cnetboot VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

if(MSVC)
    # cnetmod's ORM reflection macros require the conforming preprocessor.
    # Keep the consumer contract explicit until the installed package exports
    # the same option used while building cnetmod itself.
    add_compile_options(/Zc:preprocessor /utf-8 /bigobj)
endif()

find_package(cnetmod 2 CONFIG REQUIRED)

function(cnetboot_module name)
    cmake_parse_arguments(ARG "" "" "SOURCES;MODULES;CNETMOD;DEPENDS" ${ARGN})
    set(target "cnetboot_${name}")
    add_library(${target} STATIC ${ARG_SOURCES})
    add_library(cnetboot::${name} ALIAS ${target})
    if(ARG_MODULES)
        target_sources(${target} PUBLIC
            FILE_SET CXX_MODULES
            BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
            FILES ${ARG_MODULES})
    endif()
    # runtime is the shared base component and also supplies the MSVC std
    # module units to Ninja consumers. Protocol/application components remain
    # explicit so a small library never pulls the all-components facade.
    target_link_libraries(${target} PUBLIC
        cnetmod::runtime ${ARG_CNETMOD} ${ARG_DEPENDS})
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
endfunction()

cnetboot_module(framework_error
    SOURCES framework/error/error_code.cpp
    MODULES framework/error/error_code.cppm)

cnetboot_module(framework_core
    SOURCES framework/time.cpp
    MODULES framework/pagination.cppm framework/time.cppm)

cnetboot_module(framework_web
    SOURCES
        framework/web/query.cpp
        framework/web/error_response.cpp
        framework/web/sse_stream.cpp
        framework/web/response.cpp
        framework/web/rate_limit.cpp
    MODULES
        framework/web/query.cppm
        framework/web/error_response.cppm
        framework/web/sse_stream.cppm
        framework/web/route_handler.cppm
        framework/web/response.cppm
        framework/web/rate_limit.cppm
    CNETMOD cnetmod::runtime cnetmod::http
    DEPENDS cnetboot::framework_error cnetboot::framework_core)

cnetboot_module(framework_audit
    SOURCES framework/audit/mysql_audit_log.cpp
    MODULES
        framework/audit/audit_log.cppm
        framework/audit/audit_record.cppm
        framework/audit/mysql_audit_log.cppm
    CNETMOD cnetmod::runtime cnetmod::application cnetmod::orm
    DEPENDS cnetboot::framework_error)

cnetboot_module(framework_config
    SOURCES framework/config/llm_settings.cpp
    MODULES framework/config/llm_settings.cppm
    CNETMOD cnetmod::application
    DEPENDS cnetboot::framework_error)

cnetboot_module(security
    SOURCES
        framework/security/jwt_security.cpp
        framework/security/authorization.cpp
        framework/security/request_identity.cpp
    MODULES
        framework/security/current_user.cppm
        framework/security/jwt_security.cppm
        framework/security/authorization.cppm
        framework/security/request_identity.cppm
    CNETMOD
        cnetmod::runtime
        cnetmod::application
        cnetmod::http
        cnetmod::redis
        cnetmod::orm
    DEPENDS
        cnetboot::framework_error
        cnetboot::framework_web
        cnetboot::framework_audit)

cnetboot_module(system
    SOURCES
        modules/system/dto/auth_dto.cpp
        modules/system/dto/user_dto.cpp
        modules/system/dto/authorization_dto.cpp
        modules/system/service/user_service.cpp
        modules/system/service/authorization_service.cpp
        modules/system/mapper/system_mapper_registry.cpp
        modules/system/mapper/user_mapper.cpp
        modules/system/mapper/role_mapper.cpp
        modules/system/mapper/menu_mapper.cpp
        modules/system/service/role_service.cpp
        modules/system/service/department_service.cpp
        modules/system/service/login_guard.cpp
        modules/system/service/auth_service.cpp
    MODULES
        modules/system/domain/sys_user.cppm
        modules/system/domain/sys_role.cppm
        modules/system/domain/sys_menu.cppm
        modules/system/domain/sys_user_role.cppm
        modules/system/domain/sys_role_menu.cppm
        modules/system/domain/sys_dept.cppm
        modules/system/domain/sys_role_dept.cppm
        modules/system/domain/user_record.cppm
        modules/system/dto/auth_dto.cppm
        modules/system/dto/user_dto.cppm
        modules/system/dto/authorization_dto.cppm
        modules/system/service/user_service.cppm
        modules/system/service/authorization_service.cppm
        modules/system/mapper/system_mapper_registry.cppm
        modules/system/mapper/user_mapper.cppm
        modules/system/mapper/role_mapper.cppm
        modules/system/mapper/menu_mapper.cppm
        modules/system/service/role_service.cppm
        modules/system/service/department_service.cppm
        modules/system/service/service_failure.cppm
        modules/system/service/login_guard.cppm
        modules/system/service/auth_service.cppm
    CNETMOD
        cnetmod::runtime
        cnetmod::application
        cnetmod::redis
        cnetmod::orm
    DEPENDS
        cnetboot::framework_error
        cnetboot::framework_core
        cnetboot::security)

cnetboot_module(web
    SOURCES
        modules/system/controller/auth_controller.cpp
        modules/system/controller/user_controller.cpp
        modules/system/controller/authorization_controller.cpp
    MODULES
        modules/system/controller/auth_controller.cppm
        modules/system/controller/user_controller.cppm
        modules/system/controller/authorization_controller.cppm
    CNETMOD
        cnetmod::runtime
        cnetmod::http
    DEPENDS
        cnetboot::security
        cnetboot::system
        cnetboot::framework_core
        cnetboot::framework_web)

cnetboot_module(ai
    SOURCES
        modules/ai/service/model/composed_llm_service.cpp
        modules/ai/controller/llm_routes.cpp
        modules/ai/service/llm_catalog.cpp
        modules/ai/service/llm_service.cpp
    MODULES
        modules/ai/service/model/composed_llm_service.cppm
        modules/ai/domain/llm_contract.cppm
        modules/ai/controller/llm_routes.cppm
        modules/ai/service/llm_catalog.cppm
        modules/ai/service/llm_service.cppm
    CNETMOD
        cnetmod::runtime
        cnetmod::application
        cnetmod::http
        cnetmod::openai
    DEPENDS
        cnetboot::framework_error
        cnetboot::framework_config
        cnetboot::framework_web)

cnetboot_module(chat
    SOURCES
        modules/chat/domain/chat_domain.cpp
        modules/chat/mapper/chat_mapper.cpp
        modules/chat/service/chat_service.cpp
        modules/chat/service/chat_service_factory.cpp
    MODULES
        modules/chat/domain/chat_domain.cppm
        modules/chat/domain/chat_record.cppm
        modules/chat/mapper/chat_mapper.cppm
        modules/chat/service/chat_service.cppm
        modules/chat/service/chat_service_factory.cppm
    CNETMOD
        cnetmod::runtime
        cnetmod::application
        cnetmod::orm
    DEPENDS
        cnetboot::framework_error
        cnetboot::framework_core
        cnetboot::framework_audit
        cnetboot::ai)

cnetboot_module(chat_web
    SOURCES
        modules/chat/dto/chat_dto.cpp
        modules/chat/controller/chat_route_bindings.cpp
        modules/chat/controller/chat_route_handlers.cpp
        modules/chat/controller/chat_routes.cpp
    MODULES
        modules/chat/dto/chat_dto.cppm
        modules/chat/controller/chat_route_context.cppm
        modules/chat/controller/chat_routes.cppm
    CNETMOD
        cnetmod::runtime
        cnetmod::http
    DEPENDS
        cnetboot::chat
        cnetboot::framework_core
        cnetboot::framework_web
        cnetboot::security)

add_executable(cnetboot_server
    bootstrap/main.cpp
    bootstrap/application.cpp
    bootstrap/platform_module.cpp
    bootstrap/identity_module.cpp
    bootstrap/ai_module.cpp
    bootstrap/chat_module.cpp)
target_sources(cnetboot_server PRIVATE
    FILE_SET CXX_MODULES
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES bootstrap/modules.cppm bootstrap/application.cppm)
target_link_libraries(cnetboot_server PRIVATE
    cnetmod::runtime
    cnetmod::application
    cnetmod::http
    cnetmod::mysql
    cnetmod::redis
    cnetmod::orm
    cnetmod::openai
    cnetboot::security
    cnetboot::system
    cnetboot::ai
    cnetboot::chat
    cnetboot::chat_web
    cnetboot::framework_config
    cnetboot::framework_audit
    cnetboot::framework_web
    cnetboot::web)

configure_file(config/application.yaml application.yaml COPYONLY)
file(COPY modules/system/mapper DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY migrations DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
configure_file(modules/chat/mapper/ChatMapper.xml mapper/ChatMapper.xml COPYONLY)

add_executable(cnetboot_framework_layering_boundaries_test
    testing/framework_layering_boundaries.cpp)
target_link_libraries(cnetboot_framework_layering_boundaries_test PRIVATE
    cnetmod::runtime)
add_custom_target(cnetboot_architecture_gate ALL
    COMMAND $<TARGET_FILE:cnetboot_framework_layering_boundaries_test>
    DEPENDS cnetboot_framework_layering_boundaries_test
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Running mandatory cnetBoot architecture gate"
    VERBATIM
    USES_TERMINAL)

if(BUILD_TESTING)
    add_executable(cnetboot_request_principal_test testing/request_principal.cpp)
    target_link_libraries(cnetboot_request_principal_test PRIVATE
        cnetmod::runtime cnetmod::http cnetboot::security)
    add_test(NAME cnetboot_request_principal COMMAND cnetboot_request_principal_test)

    add_executable(cnetboot_system_mapper_test testing/system_mapper_registry.cpp)
    target_link_libraries(cnetboot_system_mapper_test PRIVATE cnetmod::orm)
    add_test(NAME cnetboot_system_mapper_registry
        COMMAND cnetboot_system_mapper_test "${CMAKE_CURRENT_BINARY_DIR}/mapper")

    add_executable(cnetboot_authorization_snapshot_test testing/authorization_snapshot.cpp)
    target_link_libraries(cnetboot_authorization_snapshot_test PRIVATE
        cnetmod::runtime cnetmod::orm cnetboot::system)
    add_test(NAME cnetboot_authorization_snapshot COMMAND cnetboot_authorization_snapshot_test)

    set(CNETBOOT_UNIT_TESTS
        framework_query
        framework_error_registry
        framework_error_contract
        framework_pagination
        framework_route_contract
        framework_jwt
        framework_sse_frames
        framework_rate_limit
        framework_typed_response
        identity_sessions
        llm_provider_selection)
    foreach(name IN LISTS CNETBOOT_UNIT_TESTS)
        add_executable(cnetboot_${name}_test testing/${name}.cpp)
        add_test(NAME cnetboot_${name} COMMAND cnetboot_${name}_test)
    endforeach()

    target_link_libraries(cnetboot_framework_query_test PRIVATE cnetboot::framework_web)
    target_link_libraries(cnetboot_framework_error_registry_test PRIVATE cnetboot::framework_web)
    target_link_libraries(cnetboot_framework_error_contract_test PRIVATE
        cnetmod::runtime cnetmod::application cnetboot::framework_error)
    target_link_libraries(cnetboot_framework_pagination_test PRIVATE cnetboot::framework_core)
    target_link_libraries(cnetboot_framework_route_contract_test PRIVATE
        cnetmod::runtime cnetmod::http cnetboot::web cnetboot::ai cnetboot::chat_web)
    target_link_libraries(cnetboot_framework_jwt_test PRIVATE cnetmod::runtime)
    target_link_libraries(cnetboot_framework_sse_frames_test PRIVATE
        cnetmod::runtime cnetmod::http cnetboot::framework_web)
    target_link_libraries(cnetboot_framework_rate_limit_test PRIVATE
        cnetmod::runtime cnetmod::http cnetboot::framework_web)
    target_link_libraries(cnetboot_framework_typed_response_test PRIVATE
        cnetmod::runtime cnetmod::http cnetboot::framework_web)
    target_link_libraries(cnetboot_identity_sessions_test PRIVATE
        cnetmod::runtime cnetmod::redis cnetboot::security cnetboot::system)
    target_link_libraries(cnetboot_llm_provider_selection_test PRIVATE
        cnetmod::runtime cnetmod::application cnetmod::openai
        cnetboot::ai cnetboot::framework_config)

    add_executable(cnetboot_chat_mapper_registry_test testing/chat_mapper_registry.cpp)
    target_link_libraries(cnetboot_chat_mapper_registry_test PRIVATE
        cnetmod::orm cnetboot::chat)
    add_test(NAME cnetboot_chat_mapper_registry
        COMMAND cnetboot_chat_mapper_registry_test "${CMAKE_CURRENT_BINARY_DIR}/mapper")

    set_tests_properties(cnetboot_framework_error_contract PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME cnetboot_framework_layering_boundaries
        COMMAND cnetboot_framework_layering_boundaries_test)
    set_tests_properties(cnetboot_framework_layering_boundaries PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(cnetboot_identity_sessions PROPERTIES SKIP_RETURN_CODE 77)
endif()
