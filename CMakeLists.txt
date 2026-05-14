# Set the minimum version of CMake that can be used
# To find the cmake version run
# $ cmake --version
cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER "/opt/homebrew/opt/llvm/bin/clang++")
set(CMAKE_OSX_ARCHITECTURES "arm64")

# Set the project name
project (decaf_cc)

# Specify the path to libedit
set(LIBEDIT_INCLUDE_DIR "/opt/homebrew/opt/libedit/include")
set(LIBEDIT_LIBRARY "/opt/homebrew/opt/libedit/lib/libedit.dylib")

find_library(LibEdit_LIB NAMES edit PATHS ${LIBEDIT_LIBRARY})
find_path(LibEdit_INCLUDE_DIR NAMES editline/readline.h PATHS ${LIBEDIT_INCLUDE_DIR})

if(LibEdit_LIB AND LibEdit_INCLUDE_DIR)
    set(LibEdit_FOUND TRUE)
    add_library(LibEdit::LibEdit INTERFACE IMPORTED)
    set_target_properties(LibEdit::LibEdit PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LibEdit_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${LibEdit_LIB}")
else()
    message(FATAL_ERROR "libedit not found")
endif()

# Find LLVM package
find_package(LLVM REQUIRED CONFIG)

# Print some information about the LLVM package
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include directories: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM libraries: ${LLVM_LIBRARY_DIRS}")
message(STATUS "LLVM definitions: ${LLVM_DEFINITIONS}")

# Create a sources variable with a link to all cpp files to compile
set(SOURCES
    src/Lexer.cpp
    src/Parser.cpp
    src/FileHandler.cpp
    src/Logger.cpp
    src/CodeGenerator.cpp
    src/JIT.cpp
    src/main.cpp
)

SET(LLVM_LINKER_FLAGS "-Wswitch")
add_definitions(${LLVM_LINKER_FLAGS})

# Add include directories
include_directories(${LLVM_INCLUDE_DIRS})

# Add LLVM definitions (e.g. defines like NDEBUG)
add_definitions(${LLVM_DEFINITIONS})

Include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
)
FetchContent_MakeAvailable(Catch2)

# Add an executable with the above sources
add_executable(decaf_cc ${SOURCES})

# # Link the necessary LLVM libraries
# execute_process(COMMAND llvm-config --libs all
#                 OUTPUT_VARIABLE llvm_libraries)

# Link the executable to LLVM libraries
target_link_libraries(decaf_cc
    PRIVATE
    LLVMWindowsManifest
    LLVMXRay
    LLVMLibDriver
    LLVMDlltoolDriver
    LLVMCoverage
    LLVMLineEditor
    LLVMXCoreDisassembler
    LLVMXCoreCodeGen
    LLVMXCoreDesc
    LLVMXCoreInfo
    LLVMX86TargetMCA
    LLVMX86Disassembler
    LLVMX86AsmParser
    LLVMX86CodeGen
    LLVMX86Desc
    LLVMX86Info
    LLVMWebAssemblyDisassembler
    LLVMWebAssemblyAsmParser
    LLVMWebAssemblyCodeGen
    LLVMWebAssemblyDesc
    LLVMWebAssemblyUtils
    LLVMWebAssemblyInfo
    LLVMVEDisassembler
    LLVMVEAsmParser
    LLVMVECodeGen
    LLVMVEDesc
    LLVMVEInfo
    LLVMSystemZDisassembler
    LLVMSystemZAsmParser
    LLVMSystemZCodeGen
    LLVMSystemZDesc
    LLVMSystemZInfo
    LLVMSparcDisassembler
    LLVMSparcAsmParser
    LLVMSparcCodeGen
    LLVMSparcDesc
    LLVMSparcInfo
    LLVMRISCVTargetMCA
    LLVMRISCVDisassembler
    LLVMRISCVAsmParser
    LLVMRISCVCodeGen
    LLVMRISCVDesc
    LLVMRISCVInfo
    LLVMPowerPCDisassembler
    LLVMPowerPCAsmParser
    LLVMPowerPCCodeGen
    LLVMPowerPCDesc
    LLVMPowerPCInfo
    LLVMNVPTXCodeGen
    LLVMNVPTXDesc
    LLVMNVPTXInfo
    LLVMMSP430Disassembler
    LLVMMSP430AsmParser
    LLVMMSP430CodeGen
    LLVMMSP430Desc
    LLVMMSP430Info
    LLVMMipsDisassembler
    LLVMMipsAsmParser
    LLVMMipsCodeGen
    LLVMMipsDesc
    LLVMMipsInfo
    LLVMLoongArchDisassembler
    LLVMLoongArchAsmParser
    LLVMLoongArchCodeGen
    LLVMLoongArchDesc
    LLVMLoongArchInfo
    LLVMLanaiDisassembler
    LLVMLanaiCodeGen
    LLVMLanaiAsmParser
    LLVMLanaiDesc
    LLVMLanaiInfo
    LLVMHexagonDisassembler
    LLVMHexagonCodeGen
    LLVMHexagonAsmParser
    LLVMHexagonDesc
    LLVMHexagonInfo
    LLVMBPFDisassembler
    LLVMBPFAsmParser
    LLVMBPFCodeGen
    LLVMBPFDesc
    LLVMBPFInfo
    LLVMAVRDisassembler
    LLVMAVRAsmParser
    LLVMAVRCodeGen
    LLVMAVRDesc
    LLVMAVRInfo
    LLVMARMDisassembler
    LLVMARMAsmParser
    LLVMARMCodeGen
    LLVMARMDesc
    LLVMARMUtils
    LLVMARMInfo
    LLVMAMDGPUTargetMCA
    LLVMAMDGPUDisassembler
    LLVMAMDGPUAsmParser
    LLVMAMDGPUCodeGen
    LLVMAMDGPUDesc
    LLVMAMDGPUUtils
    LLVMAMDGPUInfo
    LLVMAArch64Disassembler
    LLVMAArch64AsmParser
    LLVMAArch64CodeGen
    LLVMAArch64Desc
    LLVMAArch64Utils
    LLVMAArch64Info
    LLVMOrcJIT
    LLVMWindowsDriver
    LLVMMCJIT
    LLVMJITLink
    LLVMInterpreter
    LLVMExecutionEngine
    LLVMRuntimeDyld
    LLVMOrcTargetProcess
    LLVMOrcShared
    LLVMDWP
    LLVMDebugInfoLogicalView
    LLVMDebugInfoGSYM
    LLVMOption
    LLVMObjectYAML
    LLVMObjCopy
    LLVMMCA
    LLVMMCDisassembler
    LLVMLTO
    LLVMCFGuard
    LLVMFrontendOpenACC
    LLVMFrontendHLSL
    LLVMExtensions
    Polly
    PollyISL
    LLVMPasses
    LLVMCoroutines
    LLVMipo
    LLVMInstrumentation
    LLVMVectorize
    LLVMLinker
    LLVMFrontendOpenMP
    LLVMDWARFLinkerParallel
    LLVMDWARFLinker
    LLVMGlobalISel
    LLVMMIRParser
    LLVMAsmPrinter
    LLVMSelectionDAG
    LLVMCodeGen
    LLVMObjCARCOpts
    LLVMIRPrinter
    LLVMInterfaceStub
    LLVMFileCheck
    LLVMFuzzMutate
    LLVMTarget
    LLVMScalarOpts
    LLVMInstCombine
    LLVMAggressiveInstCombine
    LLVMTransformUtils
    LLVMBitWriter
    LLVMAnalysis
    LLVMProfileData
    LLVMSymbolize
    LLVMDebugInfoPDB
    LLVMDebugInfoMSF
    LLVMDebugInfoDWARF
    LLVMObject
    LLVMTextAPI
    LLVMMCParser
    LLVMIRReader
    LLVMAsmParser
    LLVMMC
    LLVMDebugInfoCodeView
    LLVMBitReader
    LLVMFuzzerCLI
    LLVMCore
    LLVMRemarks
    LLVMBitstreamReader
    LLVMBinaryFormat
    LLVMTargetParser
    LLVMTableGen
    LLVMSupport
    LLVMDemangle
    Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(decaf_cc)

# Set the directories that should be included in the build command for this target
# when running g++ these will be included as -I/directory/path/
target_include_directories(decaf_cc
    PRIVATE 
        ${PROJECT_SOURCE_DIR}/src
        /usr/local/include
)