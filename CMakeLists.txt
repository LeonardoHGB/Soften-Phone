cmake_minimum_required(VERSION 3.21)
project(SPHONE VERSION 1.0.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Configuracao base. C++17, MOC/RCC automaticos do Qt.
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

# Casa com o runtime dinamico do Qt oficial (e do PJSIP buildado /MD).
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# CMAKE_PREFIX_PATH vem do bootstrap (env do usuario) apontando para o Qt.
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Network)
qt_standard_project_setup()

# ---------------------------------------------------------------------------
# Opcao: linkar o motor SIP PJSUA2. Mantido OFF ate o PJSIP estar buildado /MD
# (ver docs/build-pjsip.md). Assim da pra compilar e ver o shell visual antes.
# ---------------------------------------------------------------------------
option(SPHONE_WITH_PJSIP "Linkar o motor SIP PJSUA2 (requer PJSIP buildado /MD)" OFF)

set(SPHONE_SOURCES
    src/main.cpp
    src/core/brand.cpp
    src/core/diag.cpp
    src/core/updater.cpp
    src/core/selfprotect.cpp
    src/sip/pjengine.cpp
    src/sip/sipmanager.cpp
    src/data/sipconfig.cpp
    src/data/callhistory.cpp
    src/data/discordaudit.cpp
    src/data/exitaudit.cpp
    src/audio/tones.cpp
    src/audio/ringtone.cpp
    src/ui/controls.cpp
    src/ui/signalwidgets.cpp
    src/ui/recentsmodel.cpp
    src/ui/panels.cpp
    src/ui/promptdialog.cpp
    src/ui/updateform.cpp
    src/ui/mainwindow.cpp
    assets/assets.qrc
    app.rc
)

set(SPHONE_HEADERS
    src/core/brand.h
    src/core/paths.h
    src/core/diag.h
    src/core/version.h
    src/core/updater.h
    src/core/selfprotect.h
    src/data/sipconfig.h
    src/data/callaudit.h
    src/data/callhistory.h
    src/data/discordaudit.h
    src/data/exitaudit.h
    src/audio/tones.h
    src/audio/ringtone.h
    src/sip/pjengine.h
    src/sip/sipmanager.h
    src/ui/controls.h
    src/ui/signalwidgets.h
    src/ui/recentsmodel.h
    src/ui/panels.h
    src/ui/promptdialog.h
    src/ui/updateform.h
    src/ui/mainwindow.h
)

# WIN32 => sem janela de console (app GUI).
add_executable(SPHONE WIN32 ${SPHONE_SOURCES} ${SPHONE_HEADERS})
set_target_properties(SPHONE PROPERTIES OUTPUT_NAME "SPHONE")
target_include_directories(SPHONE PRIVATE src)
target_link_libraries(SPHONE PRIVATE
    Qt6::Widgets Qt6::Multimedia Qt6::Network
    ws2_32 ole32 winmm crypt32 user32 advapi32 shell32 dwmapi)

# ---------------------------------------------------------------------------
# PJSIP (quando SPHONE_WITH_PJSIP=ON). Linka as .lib estaticas geradas pelo
# build /MD (tools/build-pjsip-md.ps1 -> C:\src\pjproject\build-md), na ordem
# de dependencia. Os includes vem da arvore do pjproject.
# ---------------------------------------------------------------------------
if(SPHONE_WITH_PJSIP)
  set(PJSIP_ROOT  "C:/src/pjproject"          CACHE PATH "Raiz do pjproject")
  set(PJSIP_BUILD "C:/src/pjproject/build-md" CACHE PATH "Build /MD do PJSIP (libs estaticas)")
  target_compile_definitions(SPHONE PRIVATE SPHONE_WITH_PJSIP=1)
  target_include_directories(SPHONE PRIVATE
      ${PJSIP_ROOT}/pjlib/include
      ${PJSIP_ROOT}/pjlib-util/include
      ${PJSIP_ROOT}/pjnath/include
      ${PJSIP_ROOT}/pjmedia/include
      ${PJSIP_ROOT}/pjsip/include)

  # Ordem: dependentes antes das dependencias.
  set(_pjlibs
      pjsip/pjsua-lib pjsip/pjsip-ua pjsip/pjsip-simple pjsip/pjsip
      pjmedia/pjmedia-codec pjmedia/pjmedia pjmedia/pjmedia-audiodev pjmedia/pjmedia-videodev
      pjnath/pjnath pjlib-util/pjlib-util pjlib/pjlib
      third_party/g7221/g7221 third_party/gsm/gsm third_party/ilbc/ilbc
      third_party/resample/resample third_party/speex/speex third_party/srtp/srtp
      third_party/webrtc/webrtc third_party/webrtc_aec3/webrtc_aec3 third_party/yuv/yuv)
  foreach(_l IN LISTS _pjlibs)
    target_link_libraries(SPHONE PRIVATE "${PJSIP_BUILD}/${_l}.lib")
  endforeach()

  # Libs de sistema exigidas pelo PJSIP (alem das ja linkadas acima).
  target_link_libraries(SPHONE PRIVATE iphlpapi oleaut32 gdi32 dsound)
endif()

# ---------------------------------------------------------------------------
# Conveniencia: roda windeployqt apos o build para a pasta do exe (DLLs do Qt).
# No empacotamento final o Inno Setup chama o windeployqt (ver packaging).
# ---------------------------------------------------------------------------
find_program(WINDEPLOYQT_EXE windeployqt HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT_EXE)
  add_custom_command(TARGET SPHONE POST_BUILD
    COMMAND "${WINDEPLOYQT_EXE}" --no-translations --no-compiler-runtime "$<TARGET_FILE:SPHONE>"
    COMMENT "windeployqt: copiando DLLs do Qt")
endif()
