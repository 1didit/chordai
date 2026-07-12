cmake_minimum_required(VERSION 3.25)   # JUCE hard minimum is 3.22; 3.25+ avoids known target-property edge cases

# LANGUAGES C CXX (not CXX-only): JUCE's own CMakeLists.txt declares
# project(JUCE VERSION ... LANGUAGES C CXX) inside the external/JUCE subdirectory
# scope. CMake's per-directory variable scoping means the C-language compile-rule
# variables (e.g. CMAKE_C_COMPILE_OBJECT) it sets are local to that subdirectory
# and never propagate back to this root scope, causing "CMake Generate step failed"
# at generate time for any C-language target JUCE's build defines. Declaring C here
# too establishes those rules in root scope from the start (empirically verified fix).
project(ChordAI VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/JUCE)

juce_add_plugin(ChordAI
    VERSION 0.1.0
    COMPANY_NAME "ChordAI Dev"                     # TODO: placeholder — final company/product name not locked yet (see 01-RESEARCH.md Open Question 2)
    BUNDLE_ID "com.chordai-dev.chordai"            # TODO: placeholder — cheap to rename now, expensive once DAW users have cached scans
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE                   # installs to system plugin folders after each build
    PLUGIN_MANUFACTURER_CODE Chai                  # explicit and stable from commit 1 — never omit
    PLUGIN_CODE Cha1                                # explicit and stable from commit 1 — omitting randomizes identity per build
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ChordAI")

target_sources(ChordAI PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Import/AudioFileLoader.cpp
    Source/UI/ConveyorBeltComponent.cpp
    Source/UI/MidiSetsPlaceholder.cpp)

target_link_libraries(ChordAI PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_gui_extra)

target_compile_definitions(ChordAI PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

juce_generate_juce_header(ChordAI)

# --- Test infrastructure (Catch2 + CTest) ---------------------------------
enable_testing()
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1)
FetchContent_MakeAvailable(Catch2)

juce_add_console_app(ChordAITests PRODUCT_NAME "ChordAITests")

target_sources(ChordAITests PRIVATE
    Tests/InfrastructureTests.cpp
    Tests/AudioFileLoaderTests.cpp
    Tests/RegionStateTests.cpp
    Tests/WaveformRegionTests.cpp
    Source/Import/AudioFileLoader.cpp)

target_include_directories(ChordAITests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(ChordAITests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    CHORDAI_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/fixtures")

target_link_libraries(ChordAITests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_gui_basics)

juce_generate_juce_header(ChordAITests)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(ChordAITests TEST_PREFIX "ChordAITests.")
