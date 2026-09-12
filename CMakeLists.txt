cmake_minimum_required(VERSION 3.20)
project(ECS)

#設定debug release那些
if(CMAKE_CONFIGURATION_TYPES)
    # 設定你希望在 Visual Studio 下拉選單中看到的模式
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;MinSizeRel;RelWithDebInfo" CACHE STRING "" FORCE)
endif()

#強制utf-8
if(MSVC)
    add_compile_options("/utf-8")
endif()
add_compile_definitions(UNICODE _UNICODE)

set(CMAKE_CXX_STANDARD 20)

add_executable(ECS  
    "main.cpp"

    "system/Window.h"                  "system/Window.cpp" 
    "system/RenderSystem.h"            "system/RenderSystem.cpp"   
    "system/D3D12Engine.h"             "system/D3D12Engine.cpp"
    "system/TextureManager.h"          "system/TextureManager.cpp"
    "system/RenderManager.h"           "system/RenderManager.cpp"  
    "system/QueueManager.h"            "system/QueueManager.cpp"  
    "system/Timer.h"                   "system/Timer.cpp"
    "system/ShaderConstantBuffer.h" 
    "system/ShaderStructureBuffer.h" 
    "system/InputManager.h"
    "system/CommonUtils.h"

    "scene/IScene.h"
    "scene/MenuScene.cpp"       "scene/MenuScene.h" 
    "scene/GameScene.cpp"       "scene/GameScene.h" 
    "scene/ResultScene.cpp"     "scene/ResultScene.h" 

    "gameobject/Enemy.h" 
    "gameobject/Player.h"  
    "gameobject/Bullet.h" "gameobject/Crosshair.h" "gameobject/UI.h"
    "gameobject/Audio.h"  "gameobject/Audio.cpp" 
    "gameobject/Text.h"
)

#---------------------------------------------------------------------------
# 2. 只有在 Release 模式下才動態開啟 WIN32_EXECUTABLE 屬性
set_property(TARGET ECS PROPERTY WIN32_EXECUTABLE $<CONFIG:Release>)

# 3. 完美解決 LNK2019 錯誤：將視窗模式的進入點導向您的 int main()
if(MSVC)
    target_link_options(ECS PRIVATE 
        "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>" 
        "$<$<CONFIG:Release>:/ENTRY:mainCRTStartup>"
    )
endif()
#---------------------------------------------------------------------------


add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
target_link_libraries(ECS PRIVATE 
    d3d12
    dxgi

    stb

    #暫時用
    xaudio2
    winmm

)

# 複製asset資料夾到專案位置
add_custom_command(
    TARGET ECS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/asset"
        "$<TARGET_FILE_DIR:ECS>/asset"
    COMMENT "Copying assets to output directory..."
)

# 指定資料夾內 *.hlsl 
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shader")
file(GLOB_RECURSE HLSL_FILES "${SHADER_DIR}/*.hlsl")

# 確保目標資料夾存在
set(CSO_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shader")
file(MAKE_DIRECTORY "${CSO_OUT_DIR}")

# 宣告一個空的變數，用來收集所有編譯出來的 .cso 檔案路徑
set(ALL_CSO_OUTPUTS "")

# 用迴圈遍歷每一個偵測到的 HLSL 檔案，自動生成編譯規則
foreach(SHADER_PATH ${HLSL_FILES})
    # 取得純檔名（不含路徑與副檔名），例如從 "C:/.../TextureLoader.hlsl" 提取出 "TextureLoader"
    get_filename_component(SHADER_NAME ${SHADER_PATH} NAME_WE)
    
    # 動態定義這個 Shader 編譯後的 VS 和 PS 輸出路徑
    set(VS_OUT "${CSO_OUT_DIR}/${SHADER_NAME}_vs.cso")
    set(PS_OUT "${CSO_OUT_DIR}/${SHADER_NAME}_ps.cso")
    
    # 🟢 2a. 自動註冊頂點著色器編譯規則（固定尋找 VSMain 進入點）
    add_custom_command(
        OUTPUT ${VS_OUT}
        COMMAND dxc.exe -E VSMain -T vs_6_0 -Fo ${VS_OUT} ${SHADER_PATH}
        DEPENDS ${SHADER_PATH}
        COMMENT "自動編譯頂點著色器: ${SHADER_NAME} (VSMain -> vs_6_0)"
    )
    # 🟢 2b. 自動註冊像素著色器編譯規則（固定尋找 PSMain 進入點）
    add_custom_command(
        OUTPUT ${PS_OUT}
        COMMAND dxc.exe -E PSMain -T ps_6_0 -Fo ${PS_OUT} ${SHADER_PATH}
        DEPENDS ${SHADER_PATH}
        COMMENT "自動編譯像素著色器: ${SHADER_NAME} (PSMain -> ps_6_0)"
    )
    # 將這次生成的兩個 .cso 檔案路徑，塞進我們的大清單中
    list(APPEND ALL_CSO_OUTPUTS ${VS_OUT} ${PS_OUT})
endforeach()

# 完美對接：把收集到的一大坨 .cso 全部綁定到你的執行檔
target_sources(ECS PRIVATE ${ALL_CSO_OUTPUTS})