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

set(CHORDAI_ANALYSIS_SOURCES
    Source/Analysis/AudioPreprocessing.cpp
    Source/Analysis/ConstantQAnalysis.cpp
    Source/Analysis/TuningEstimator.cpp
    Source/Analysis/HarmonicPercussiveFilter.cpp
    Source/Analysis/ChromaExtractor.cpp
    Source/Analysis/OnsetEnvelope.cpp
    Source/Analysis/TempoBeatTracker.cpp
    Source/Analysis/KeyDetector.cpp
    Source/Analysis/ChordDecoder.cpp
    Source/Analysis/ClassicDspChordAnalyzer.cpp)

set(CHORDAI_MIDIGEN_SOURCES
    Source/MidiGen/AsIsRowGenerator.cpp
    Source/MidiGen/StyleVoicingGenerators.cpp
    Source/MidiGen/BassLineGenerator.cpp
    Source/MidiGen/MidiRowBuilder.cpp)

target_sources(ChordAI PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Import/AudioFileLoader.cpp
    Source/UI/ConveyorBeltComponent.cpp
    Source/UI/MidiRowView.cpp
    Source/UI/MidiSetsPanel.cpp
    Source/UI/WaveformView.cpp
    Source/UI/RegionSelectorOverlay.cpp
    Source/UI/ChordTimelineView.cpp
    Source/Analysis/AnalysisPipeline.cpp
    ${CHORDAI_ANALYSIS_SOURCES}
    ${CHORDAI_MIDIGEN_SOURCES})

target_link_libraries(ChordAI PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_gui_extra
    juce::juce_dsp)

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

# constant-q-cpp (Chris Cannam / QMUL, MIT-style license) — pinned to a specific
# commit on master (no tagged releases). No upstream CMakeLists.txt (only
# Makefile.linux/.osx/.mingw32) so FetchContent only populates the source tree;
# the static lib target below enumerates its sources explicitly.
FetchContent_Declare(constant_q_cpp
    GIT_REPOSITORY https://github.com/cannam/constant-q-cpp.git
    GIT_TAG        7ac84048e3e43c433d88e12d221af15e50f41591)  # master HEAD as of 2026-07-12 — verified via git ls-remote
FetchContent_MakeAvailable(constant_q_cpp)

# Upstream has no CMakeLists.txt (only Makefile.linux/.osx/.mingw32) — source
# list enumerated explicitly from the fetched tree (verified via `ls`, not guessed).
add_library(constant_q_cpp STATIC
    ${constant_q_cpp_SOURCE_DIR}/src/CQInverse.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/CQKernel.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/CQSpectrogram.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/Chromagram.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/ConstantQ.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/Pitch.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/dsp/FFT.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/dsp/KaiserWindow.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/dsp/MathUtilities.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/dsp/Resampler.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/dsp/SincWindow.cpp
    ${constant_q_cpp_SOURCE_DIR}/src/ext/kissfft/kiss_fft.c
    ${constant_q_cpp_SOURCE_DIR}/src/ext/kissfft/tools/kiss_fftr.c)

target_include_directories(constant_q_cpp SYSTEM PUBLIC
    ${constant_q_cpp_SOURCE_DIR})

# Matches upstream Makefile.inc's own GENERAL_FLAGS include path
# (-I. -Icq -Isrc -Isrc/ext/kissfft -Isrc/ext/kissfft/tools): internal .cpp
# files use unqualified #include "CQKernel.h" / "kiss_fft.h" etc, resolved
# relative to these directories, not the "cq/..." qualified public path.
target_include_directories(constant_q_cpp PRIVATE
    ${constant_q_cpp_SOURCE_DIR}/cq
    ${constant_q_cpp_SOURCE_DIR}/src
    ${constant_q_cpp_SOURCE_DIR}/src/ext/kissfft
    ${constant_q_cpp_SOURCE_DIR}/src/ext/kissfft/tools)

# Matches upstream Makefile.inc's -Dkiss_fft_scalar=double (this library's
# RealSequence/RealColumn types are vector<double>; kissfft defaults to float).
target_compile_definitions(constant_q_cpp PRIVATE
    kiss_fft_scalar=double)

target_link_libraries(ChordAI PRIVATE constant_q_cpp)

juce_add_console_app(ChordAITests PRODUCT_NAME "ChordAITests")

target_sources(ChordAITests PRIVATE
    Tests/InfrastructureTests.cpp
    Tests/AudioFileLoaderTests.cpp
    Tests/RegionStateTests.cpp
    Tests/WaveformRegionTests.cpp
    Tests/SyntheticFixturesTests.cpp
    Tests/AudioPreprocessingTests.cpp
    Tests/ChromaExtractorTests.cpp
    Tests/TuningEstimatorTests.cpp
    Tests/TempoBeatTrackerTests.cpp
    Tests/KeyDetectorTests.cpp
    Tests/ChordDecoderTests.cpp
    Tests/ClassicDspChordAnalyzerTests.cpp
    Tests/AnalysisPipelineTests.cpp
    Tests/ChordNameFormatterTests.cpp
    Tests/ChordTimelineLayoutTests.cpp
    Tests/ChordToneMapperTests.cpp
    Tests/StyleVoicingTests.cpp
    Tests/BassLineGeneratorTests.cpp
    Tests/MidiRowBuilderTests.cpp
    Tests/PluginProcessorMidiGenTests.cpp
    Tests/MidiSetsPanelLayoutTests.cpp
    Source/Import/AudioFileLoader.cpp
    Source/Analysis/AnalysisPipeline.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/ConveyorBeltComponent.cpp
    Source/UI/MidiRowView.cpp
    Source/UI/MidiSetsPanel.cpp
    Source/UI/WaveformView.cpp
    Source/UI/RegionSelectorOverlay.cpp
    Source/UI/ChordTimelineView.cpp
    ${CHORDAI_ANALYSIS_SOURCES}
    ${CHORDAI_MIDIGEN_SOURCES})

target_include_directories(ChordAITests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(ChordAITests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Name="ChordAI"
    CHORDAI_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/fixtures")

target_link_libraries(ChordAITests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
    constant_q_cpp)

juce_generate_juce_header(ChordAITests)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(ChordAITests TEST_PREFIX "ChordAITests.")
