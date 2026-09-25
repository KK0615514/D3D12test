cmake_minimum_required(VERSION 3.20)
project(ECS)

# 設定下拉選單
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "" FORCE)
endif()

# 強制utf-8
if(MSVC)
    add_compile_options("/utf-8")
endif()
add_compile_definitions(UNICODE _UNICODE)

set(CMAKE_CXX_STANDARD 20)

add_executable(ECS  
    "main.cpp"

    "system/Window.cpp" 
    "system/RenderSystem.cpp"   
    "system/D3D12Engine.cpp"
    "system/QueueManager.cpp"  
    "system/ResourceManager.cpp"
    "system/PipelineManager.cpp"  
    "system/GlbLoader.cpp"
    "system/Timer.cpp"

    "scene/Game3DScene.cpp"   

    #Entt
    "ecs_system/CameraSystem.cpp"

    #可刪除 placedresoource實作以前不動 
    "system/TextureManager.h"          "system/TextureManager.cpp"       
)

#release設定下 關閉cmd窗口
set_property(TARGET ECS PROPERTY WIN32_EXECUTABLE $<CONFIG:Release>)
if(MSVC)
    target_link_options(ECS PRIVATE 
        "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>" 
        "$<$<CONFIG:Release>:/ENTRY:mainCRTStartup>"
    )
endif()

# 引入各種資源
find_package(EnTT CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_library(vcpkg_headers INTERFACE)
target_include_directories(vcpkg_headers  INTERFACE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
target_include_directories(ECS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ECS PRIVATE 
    d3d12
    dxgi

    imgui::imgui            #除錯用
    EnTT::EnTT              #開源ecs
    vcpkg_headers           #stb_image , tinyglft

    #暫時用 聲音
    xaudio2
    winmm)

# 複製asset資料夾 到 專案位置
add_custom_command(
    TARGET ECS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/asset"
        "$<TARGET_FILE_DIR:ECS>/asset"
    COMMENT "Copying assets to output directory...")

# 指定shader內 *.hlsl 
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shader")
file(GLOB_RECURSE HLSL_FILES "${SHADER_DIR}/*.hlsl")
# 確保專案位置資料夾存在
set(CSO_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shader")
file(MAKE_DIRECTORY "${CSO_OUT_DIR}")
# 宣告一個空變數 存取所有編譯好的.cso
set(ALL_CSO_OUTPUTS "")
# 遍歷所有HLSL & 編譯
foreach(SHADER_PATH ${HLSL_FILES})
    # 取純檔名
    get_filename_component(SHADER_NAME ${SHADER_PATH} NAME_WE)
    # 定義路徑 & 名稱
    set(VS_OUT "${CSO_OUT_DIR}/${SHADER_NAME}_vs.cso")
    set(PS_OUT "${CSO_OUT_DIR}/${SHADER_NAME}_ps.cso")
    # 使用dxc 在cmd 編譯VS
    add_custom_command(
        OUTPUT ${VS_OUT}
        COMMAND dxc.exe -E VSMain -T vs_6_0 -Fo ${VS_OUT} ${SHADER_PATH}
        DEPENDS ${SHADER_PATH}
        COMMENT "自動編譯頂點著色器: ${SHADER_NAME} (VSMain -> vs_6_0)"
    )
    # 使用dxc 在cmd 編譯PS
    add_custom_command(
        OUTPUT ${PS_OUT}
        COMMAND dxc.exe -E PSMain -T ps_6_0 -Fo ${PS_OUT} ${SHADER_PATH}
        DEPENDS ${SHADER_PATH}
        COMMENT "自動編譯像素著色器: ${SHADER_NAME} (PSMain -> ps_6_0)"
    )
    #存取所有編譯好的 .cso
    list(APPEND ALL_CSO_OUTPUTS ${VS_OUT} ${PS_OUT})
endforeach()
# 批量綁定所有 .cso
target_sources(ECS PRIVATE ${ALL_CSO_OUTPUTS})