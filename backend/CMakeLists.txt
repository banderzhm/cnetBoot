cmake_minimum_required(VERSION 3.30)
project(cnetnexus VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

find_package(cnetmod 2 CONFIG REQUIRED)

# ZhouYiLab ships its own C++23 module set plus an nlohmann JSON module. Its
# CMake package excludes json.cppm from MODULE_FILES and serves it through the
# nlohmann_json_modules archive, so both must reach every consuming target.
find_package(ZhouYiLab 1 CONFIG REQUIRED)

function(cnetnexus_module name)
    # ZHOYI marks a module that compiles the ZhouYiLab module set into its own
    # interface. ZhouYiLab's package excludes json.cppm from MODULE_FILES and
    # serves it through nlohmann_json_modules, so that module file has to be
    # added back explicitly or no BMI for nlohmann.json is ever produced.
    cmake_parse_arguments(ARG "ZHOYI" "" "SOURCES;MODULES;DEPENDS" ${ARGN})
    set(target "cnetnexus_${name}")
    add_library(${target} STATIC ${ARG_SOURCES})
    add_library(cnetnexus::${name} ALIAS ${target})

    set(module_base_dirs "${CMAKE_CURRENT_SOURCE_DIR}")
    set(module_files ${ARG_MODULES})
    set(extra_links "")
    if(ARG_ZHOYI)
        list(APPEND module_base_dirs "${ZhouYiLab_MODULE_BASE_DIR}")
        list(APPEND module_files
            ${ZhouYiLab_MODULE_FILES}
            "${ZhouYiLab_MODULE_BASE_DIR}/json.cppm"
            "${ZhouYiLab_FMT_MODULE_FILE}")
        list(APPEND extra_links
            "$<TARGET_FILE:ZhouYiLab::ZhouYiLabCore>"
            ZhouYiLab::nlohmann_json_modules)
        target_compile_definitions(${target} PRIVATE MAGIC_ENUM_USE_STD_MODULE)
        target_include_directories(${target} PRIVATE "${ZhouYiLab_INCLUDE_DIR}")
    endif()

    if(module_files)
        target_sources(${target} PUBLIC
            FILE_SET CXX_MODULES
            BASE_DIRS ${module_base_dirs}
            FILES ${module_files})
    endif()
    target_link_libraries(${target} PUBLIC cnetmod::cnetmod_core ${ARG_DEPENDS} ${extra_links})
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
endfunction()

# Registered error codes and the web-layer helpers shared by every module. They
# sit below security because error responses and SSE frames depend on them.
cnetnexus_module(framework_error
    SOURCES
        framework/error/error_code.cpp
    MODULES
        framework/error/error_code.cppm)

# Transport-neutral request/result contracts shared by business modules.
cnetnexus_module(framework_core
    MODULES
        framework/pagination.cppm)

cnetnexus_module(framework_web
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
    DEPENDS
        cnetnexus::framework_error
        cnetnexus::framework_core)

# Append-only audit contract. Modules record write operations through this
# seam so the composition root picks the storage backing.
cnetnexus_module(framework_audit
    SOURCES
        framework/audit/mysql_audit_log.cpp
    MODULES
        framework/audit/audit_log.cppm
        framework/audit/audit_record.cppm
        framework/audit/mysql_audit_log.cppm
    DEPENDS
        cnetnexus::framework_error)

# Typed llm options section plus pure directory/target resolution. cnetmod owns
# file reading, ENV expansion and section decoding.
cnetnexus_module(framework_config
    SOURCES
        framework/config/llm_settings.cpp
    MODULES
        framework/config/llm_settings.cppm
    DEPENDS
        cnetnexus::framework_error)

cnetnexus_module(security
    SOURCES
        framework/security/jwt_security.cpp
        framework/security/authorization.cpp
    MODULES
        framework/security/current_user.cppm
        framework/security/jwt_security.cppm
        framework/security/authorization.cppm
    DEPENDS
        cnetnexus::framework_error
        cnetnexus::framework_web)

cnetnexus_module(system
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
    DEPENDS
        cnetnexus::framework_error
        cnetnexus::framework_core
        cnetnexus::security)

cnetnexus_module(web
    SOURCES
        modules/system/controller/auth_controller.cpp
        modules/system/controller/user_controller.cpp
        modules/system/controller/authorization_controller.cpp
    MODULES
        modules/system/controller/auth_controller.cppm
        modules/system/controller/user_controller.cppm
        modules/system/controller/authorization_controller.cppm
    DEPENDS
        cnetnexus::security
        cnetnexus::system
        cnetnexus::framework_core
        # 系统控制器用 framework/web 的 route_handler 与端点元数据声明路由。
        cnetnexus::framework_web)

# Reading module: LLM contract, reading (SSE) generation and the cache.
cnetnexus_module(reading
    SOURCES
        modules/reading/service/model/composed_llm_service.cpp
        modules/reading/domain/reading_domain.cpp
        modules/reading/mapper/reading_cache_mapper.cpp
        modules/reading/controller/llm_routes.cpp
        modules/reading/service/llm_catalog.cpp
        modules/reading/service/llm_service.cpp
        modules/reading/service/reading_service.cpp
        modules/reading/service/reading_cache_service.cpp
        modules/reading/service/reading_use_case.cpp
    MODULES
        modules/reading/service/model/composed_llm_service.cppm
        modules/reading/domain/llm_contract.cppm
        modules/reading/domain/reading_domain.cppm
        modules/reading/domain/reading_cache_record.cppm
        modules/reading/mapper/reading_cache_mapper.cppm
        modules/reading/controller/llm_routes.cppm
        modules/reading/service/llm_catalog.cppm
        modules/reading/service/llm_service.cppm
        modules/reading/service/reading_service.cppm
        modules/reading/service/reading_cache_service.cppm
        modules/reading/service/reading_use_case.cppm
    DEPENDS
        cnetnexus::framework_error
        cnetnexus::framework_config
        # LLM 目录端点在注册处用端点元数据声明匿名访问，需要 route_handler。
        cnetnexus::framework_web)

# Chart module: chart generation, archives, cases, tags and statistics.
#
# 上游 ZhouYiLab 的适配层位于 service/xuanxue/：它是**唯一** import ZhouYi.* 的
# 目录（边界由 verify 脚本与 CI 断言守住，见 docs/layering.md）。适配层放在
# service 之下而不是独立分层，是因为它履行的是 service 层的职责——把外部依赖的
# 数据结构与异常语义翻译成本项目的领域模型与错误码，不承载业务编排。
# 本目标因此需要编译上游模块集（ZHOYI 标记）。
cnetnexus_module(chart
    SOURCES
        modules/chart/service/xuanxue/bazi_adapter.cpp
        modules/chart/service/xuanxue/bazi_liu_nian_projection.cpp
        modules/chart/service/xuanxue/bazi_projection.cpp
        modules/chart/service/xuanxue/da_liu_ren_adapter.cpp
        modules/chart/service/xuanxue/da_liu_ren_projection.cpp
        modules/chart/service/xuanxue/liu_yao_adapter.cpp
        modules/chart/service/xuanxue/mei_hua_adapter.cpp
        modules/chart/service/xuanxue/qi_men_adapter.cpp
        modules/chart/service/xuanxue/zi_wei_adapter.cpp
        modules/chart/service/xuanxue/json_bridge.cpp
        modules/chart/domain/archive.cpp
        modules/chart/dto/chart_dto.cpp
        modules/chart/dto/chart_projection.cpp
        modules/chart/domain/cases.cpp
        modules/chart/domain/catalog.cpp
        modules/chart/domain/naming.cpp
        modules/chart/domain/stats.cpp
        modules/chart/mapper/chart_mapper_registry.cpp
        # memory_chart_case_mapper.cpp / memory_chart_archive_mapper.cpp 是同一个
        # 模块（cnetnexus.chart.mapper.memory_chart_case_mapper）的两个实现单元，
        # 只被 testing/ 使用，因此移到下面的 cnetnexus_chart_testing 目标，
        # 不再链进生产库。bazi 命盘缓存直接用 Application 托管的 redis_template。
        modules/chart/mapper/mysql_chart_archive_mapper.cpp
        modules/chart/mapper/mysql_chart_case_mapper.cpp
        modules/chart/mapper/mysql_chart_tag_mapper.cpp
        modules/chart/service/bazi_service.cpp
        modules/chart/service/chart_generation_service.cpp
        modules/chart/service/chart_reading_service.cpp
        modules/chart/service/chart_reading_scope_sections.cpp
        modules/chart/service/catalog_service.cpp
        modules/chart/service/chart_archive_service.cpp
        modules/chart/service/da_liu_ren_service.cpp
        modules/chart/service/impl/case_service.cpp
        modules/chart/service/case_service_factory.cpp
        modules/chart/service/impl/stats_service.cpp
        modules/chart/service/liu_yao_service.cpp
        modules/chart/service/mei_hua_service.cpp
        modules/chart/service/qi_men_service.cpp
        modules/chart/service/zi_wei_service.cpp
    MODULES
        modules/chart/service/xuanxue/bazi_adapter.cppm
        modules/chart/service/xuanxue/bazi_liu_nian_projection.cppm
        modules/chart/service/xuanxue/bazi_projection.cppm
        modules/chart/service/xuanxue/da_liu_ren_adapter.cppm
        modules/chart/service/xuanxue/da_liu_ren_projection.cppm
        modules/chart/service/xuanxue/liu_yao_adapter.cppm
        modules/chart/service/xuanxue/mei_hua_adapter.cppm
        modules/chart/service/xuanxue/qi_men_adapter.cppm
        modules/chart/service/xuanxue/zi_wei_adapter.cppm
        modules/chart/service/xuanxue/json_bridge.cppm
        modules/chart/domain/archive.cppm
        modules/chart/dto/chart_dto.cppm
        modules/chart/dto/chart_projection.cppm
        modules/chart/domain/bazi.cppm
        modules/chart/domain/cases.cppm
        modules/chart/domain/catalog.cppm
        modules/chart/domain/da_liu_ren.cppm
        modules/chart/domain/liu_yao.cppm
        modules/chart/domain/mei_hua.cppm
        modules/chart/domain/naming.cppm
        modules/chart/domain/qi_men.cppm
        modules/chart/domain/stats.cppm
        modules/chart/domain/chart_record.cppm
        modules/chart/domain/zi_wei.cppm
        modules/chart/mapper/chart_mapper_registry.cppm
        modules/chart/mapper/chart_archive_repository.cppm
        modules/chart/mapper/chart_case_mapper.cppm
        modules/chart/mapper/mysql_chart_case_mapper.cppm
        modules/chart/mapper/user_count_mapper.cppm
        modules/chart/service/algorithm_api.cppm
        modules/chart/service/bazi_service.cppm
        modules/chart/service/chart_generation_service.cppm
        modules/chart/service/chart_reading_service.cppm
        modules/chart/service/catalog_service.cppm
        modules/chart/service/chart_archive_service.cppm
        modules/chart/service/compute_executor.cppm
        modules/chart/service/da_liu_ren_service.cppm
        modules/chart/service/impl/case_service.cppm
        modules/chart/service/case_service_factory.cppm
        modules/chart/service/impl/stats_service.cppm
        modules/chart/service/liu_yao_service.cppm
        modules/chart/service/mei_hua_service.cppm
        modules/chart/service/qi_men_service.cppm
        modules/chart/service/zi_wei_service.cppm
    DEPENDS
        cnetnexus::framework_error
        cnetnexus::framework_core
        cnetnexus::framework_audit
        cnetnexus::reading
    ZHOYI)

# 仅供测试使用的内存版 chart mapper。它们原本直接编进 cnetnexus_chart，于是
# 连服务器二进制也带上了这份纯测试代码；现在拆成独立目标，只由测试链接。
# 注意两个 .cpp 实现的是同一个模块（memory_chart_case_mapper.cppm 同时导出
# memory_chart_case_mapper 与 memory_chart_archive_mapper），因此接口单元也归属本目标。
cnetnexus_module(chart_testing
    SOURCES
        testing/support/memory_chart_archive_mapper.cpp
        testing/support/memory_chart_case_mapper.cpp
    MODULES
        testing/support/memory_chart_case_mapper.cppm
    DEPENDS
        cnetnexus::chart
        cnetnexus::framework_core)

cnetnexus_module(chat
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
    DEPENDS
        cnetnexus::framework_error
        cnetnexus::framework_core
        cnetnexus::framework_audit
        cnetnexus::reading
        cnetnexus::chart)

cnetnexus_module(chart_web
    SOURCES
        modules/chart/controller/archive_access.cpp
        modules/chart/controller/archive_routes.cpp
        modules/chart/controller/batch_routes.cpp
        modules/chart/controller/bazi_routes.cpp
        modules/chart/controller/case_route_bindings.cpp
        modules/chart/controller/case_route_handlers.cpp
        modules/chart/controller/catalog_routes.cpp
        modules/chart/controller/chart_route_support.cpp
        modules/chart/controller/da_liu_ren_routes.cpp
        modules/chart/controller/liu_yao_routes.cpp
        modules/chart/controller/mei_hua_routes.cpp
        modules/chart/controller/qi_men_routes.cpp
        modules/chart/controller/reading_responder.cpp
        modules/chart/controller/reading_route_bindings.cpp
        modules/chart/controller/reading_route_support.cpp
        modules/chart/controller/reading_scope.cpp
        modules/chart/controller/stats_routes.cpp
        modules/chart/controller/zi_wei_routes.cpp
    MODULES
        modules/chart/controller/archive_access.cppm
        modules/chart/controller/archive_routes.cppm
        modules/chart/controller/batch_routes.cppm
        modules/chart/controller/bazi_routes.cppm
        modules/chart/controller/case_routes.cppm
        modules/chart/controller/catalog_routes.cppm
        modules/chart/controller/chart_route_support.cppm
        modules/chart/controller/da_liu_ren_routes.cppm
        modules/chart/controller/liu_yao_routes.cppm
        modules/chart/controller/mei_hua_routes.cppm
        modules/chart/controller/qi_men_routes.cppm
        modules/chart/controller/reading_responder.cppm
        modules/chart/controller/reading_routes.cppm
        modules/chart/controller/reading_scope.cppm
        modules/chart/controller/stats_routes.cppm
        modules/chart/controller/zi_wei_routes.cppm
    DEPENDS
        cnetnexus::chart
        cnetnexus::framework_core
        cnetnexus::reading
        cnetnexus::framework_web
        cnetnexus::framework_audit
        cnetnexus::security)

cnetnexus_module(chat_web
    SOURCES
        modules/chat/dto/chat_dto.cpp
        modules/chat/controller/chat_route_bindings.cpp
        modules/chat/controller/chat_route_handlers.cpp
        modules/chat/controller/chat_routes.cpp
    MODULES
        modules/chat/dto/chat_dto.cppm
        modules/chat/controller/chat_route_context.cppm
        modules/chat/controller/chat_routes.cppm
    DEPENDS
        cnetnexus::chat
        cnetnexus::framework_core
        cnetnexus::chart
        # chat_route_handlers imports cnetnexus.chart.controller.route_support,
        # which chart_web owns; module visibility is not transitive, so the
        # controller library must be a direct dependency.
        cnetnexus::chart_web
        cnetnexus::reading
        cnetnexus::framework_web
        cnetnexus::security)

add_executable(cnetnexus_server
    bootstrap/main.cpp
    bootstrap/application.cpp
    bootstrap/platform_module.cpp
    bootstrap/identity_module.cpp
    bootstrap/reading_module.cpp
    bootstrap/chart_module.cpp
    bootstrap/chat_module.cpp)
target_sources(cnetnexus_server PRIVATE
    FILE_SET CXX_MODULES
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        bootstrap/modules.cppm
        bootstrap/application.cppm)
target_link_libraries(cnetnexus_server PRIVATE
    cnetmod::cnetmod_core
    cnetnexus::security
    cnetnexus::system
    cnetnexus::chart
    cnetnexus::chat
    cnetnexus::reading
    cnetnexus::chart_web
    cnetnexus::chat_web
    # 组合根直接 import 这些模块（模块可见性不传递）。
    cnetnexus::framework_config
    cnetnexus::framework_audit
    cnetnexus::framework_web
    cnetnexus::web)

configure_file(config/application.yaml application.yaml COPYONLY)
file(COPY modules/system/mapper DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY migrations DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
# Business-module XML statements join the same loader directory as the
# system mappers so one registry start loads every statement set.
configure_file(modules/reading/mapper/ReadingCacheMapper.xml
    mapper/ReadingCacheMapper.xml COPYONLY)
configure_file(modules/chat/mapper/ChatMapper.xml
    mapper/ChatMapper.xml COPYONLY)
configure_file(modules/chart/mapper/ChartMapper.xml
    mapper/ChartMapper.xml COPYONLY)

if(BUILD_TESTING)
    add_executable(cnetnexus_request_principal_test
        testing/request_principal.cpp)
    target_link_libraries(cnetnexus_request_principal_test PRIVATE
        cnetnexus::security)
    add_test(NAME cnetnexus_request_principal
        COMMAND cnetnexus_request_principal_test)

    add_executable(cnetnexus_system_mapper_test
        testing/system_mapper_registry.cpp)
    target_link_libraries(cnetnexus_system_mapper_test PRIVATE
        cnetmod::cnetmod_core)
    add_test(NAME cnetnexus_system_mapper_registry
        COMMAND cnetnexus_system_mapper_test
            "${CMAKE_CURRENT_BINARY_DIR}/mapper")

    add_executable(cnetnexus_authorization_snapshot_test
        testing/authorization_snapshot.cpp)
    target_link_libraries(cnetnexus_authorization_snapshot_test PRIVATE
        cnetnexus::system)
    add_test(NAME cnetnexus_authorization_snapshot
        COMMAND cnetnexus_authorization_snapshot_test)


    # ---- Migrated Xuanxue suites: target cnetnexus_<x>_test, add_test cnetnexus_<x> ----
    set(CNETNEXUS_UNIT_TESTS
        framework_query framework_error_registry framework_error_contract
        framework_layering_boundaries
        framework_pagination
        framework_route_contract
        framework_jwt framework_sse_frames framework_rate_limit
        framework_typed_response
        identity_sessions
        chart_catalog_service chart_bazi_service chart_liu_yao_service
        chart_mei_hua_service chart_zi_wei_service chart_da_liu_ren_service
        chart_qi_men_service chart_bazi_cache chart_archive_service
        chart_case_service chart_archive_ownership chart_batch_limit
        reading_ai_prompt reading_service reading_use_case reading_scope reading_prompt_render
        llm_provider_selection)

    foreach(name IN LISTS CNETNEXUS_UNIT_TESTS)
        add_executable(cnetnexus_${name}_test testing/${name}.cpp)
        add_test(NAME cnetnexus_${name} COMMAND cnetnexus_${name}_test)
    endforeach()

    # Framework suites. error_response pulls in cnetmod.protocol.http, so the web
    # helpers are linked explicitly instead of relying on transitivity.
    target_link_libraries(cnetnexus_framework_query_test PRIVATE cnetnexus::framework_web)
    target_link_libraries(cnetnexus_framework_error_registry_test PRIVATE cnetnexus::framework_web)
    target_link_libraries(cnetnexus_framework_layering_boundaries_test PRIVATE
        cnetmod::cnetmod_core)
    target_link_libraries(cnetnexus_framework_pagination_test PRIVATE
        cnetnexus::framework_core)
    target_link_libraries(cnetnexus_framework_route_contract_test PRIVATE
        cnetmod::cnetmod_core
        cnetnexus::web
        cnetnexus::reading
        cnetnexus::chart_web
        cnetnexus::chat_web)
    target_link_libraries(cnetnexus_framework_sse_frames_test PRIVATE
        cnetmod::cnetmod_core cnetnexus::framework_web)
    target_link_libraries(cnetnexus_framework_error_contract_test PRIVATE
        cnetmod::cnetmod_core cnetnexus::framework_error)
    target_link_libraries(cnetnexus_framework_jwt_test PRIVATE cnetmod::cnetmod_core)
    target_link_libraries(cnetnexus_framework_rate_limit_test PRIVATE
        cnetmod::cnetmod_core cnetnexus::framework_web)
    target_link_libraries(cnetnexus_framework_typed_response_test PRIVATE
        cnetmod::cnetmod_core cnetnexus::framework_web)
    target_link_libraries(cnetnexus_identity_sessions_test PRIVATE
        cnetmod::cnetmod_core cnetnexus::security cnetnexus::system)

    # Chart suites rely on the chart target's public framework dependencies.
    foreach(name chart_catalog_service chart_bazi_service chart_liu_yao_service
                 chart_mei_hua_service chart_zi_wei_service chart_da_liu_ren_service
                 chart_qi_men_service chart_bazi_cache chart_archive_service
                 chart_case_service chart_archive_ownership chart_batch_limit)
        target_link_libraries(cnetnexus_${name}_test PRIVATE cnetnexus::chart)
    endforeach()

    # 归属与小批量上限用例直接驱动 HTTP 适配层（chart.controller.*），
    # 因此需要控制器库本身，而 chart 的模块可见性不传递。
    target_link_libraries(cnetnexus_chart_archive_ownership_test PRIVATE
        cnetnexus::chart_web cnetnexus::chart_testing)
    target_link_libraries(cnetnexus_chart_batch_limit_test PRIVATE
        cnetnexus::chart_web cnetnexus::chart_testing cnetnexus::framework_core)
    # 这两个用例直接构造内存版 case/archive mapper。
    target_link_libraries(cnetnexus_chart_case_service_test PRIVATE
        cnetnexus::chart_testing)
    target_link_libraries(cnetnexus_chart_archive_service_test PRIVATE
        cnetnexus::chart_testing)

    # Reading suites; prompt rendering also imports the chat domain.
    target_link_libraries(cnetnexus_reading_ai_prompt_test PRIVATE cnetnexus::reading)
    target_link_libraries(cnetnexus_reading_service_test PRIVATE cnetnexus::reading)
    target_link_libraries(cnetnexus_reading_use_case_test PRIVATE cnetnexus::reading)
    target_link_libraries(cnetnexus_reading_scope_test PRIVATE cnetnexus::reading)
    target_link_libraries(cnetnexus_reading_prompt_render_test PRIVATE cnetnexus::chat)
    # 多供应商目录/补全适配套件：只连纯逻辑层与 reading 目标，不连 cnetmod 服务。
    target_link_libraries(cnetnexus_llm_provider_selection_test PRIVATE
        cnetmod::cnetmod_core cnetnexus::reading cnetnexus::framework_config)

    # The XML contract suite loads the mapper directory copied into the build.
    add_executable(cnetnexus_chart_mapper_test testing/chart_mapper.cpp)
    target_link_libraries(cnetnexus_chart_mapper_test PRIVATE cnetnexus::chart)
    add_test(NAME cnetnexus_chart_mapper
        COMMAND cnetnexus_chart_mapper_test "${CMAKE_CURRENT_BINARY_DIR}/mapper")

    add_executable(cnetnexus_chat_mapper_registry_test
        testing/chat_mapper_registry.cpp)
    target_link_libraries(cnetnexus_chat_mapper_registry_test PRIVATE
        cnetnexus::chat)
    add_test(NAME cnetnexus_chat_mapper_registry
        COMMAND cnetnexus_chat_mapper_registry_test
            "${CMAKE_CURRENT_BINARY_DIR}/mapper")

    # The error-code contract suite reads contracts/error_codes.yaml from the
    # source tree, so it runs with backend/ as its working directory.
    set_tests_properties(cnetnexus_framework_error_contract PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    # 分层边界用例扫描源码树（modules/ 下的目录与 import 语句），因此也以
    # backend/ 作为工作目录。
    set_tests_properties(cnetnexus_framework_layering_boundaries PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    # 命盘缓存用例直连真实 Redis；连不上时返回 77，记为 Skipped 而不是通过。
    set_tests_properties(cnetnexus_chart_bazi_cache cnetnexus_identity_sessions PROPERTIES
        SKIP_RETURN_CODE 77)

endif()
