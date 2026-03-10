function(add_ebpf_program NAME SRC)
    # 1. Locate Tooling
    find_program(CLANG_BPF NAMES clang-20 clang REQUIRED)
    # Specifically look for the standalone bpftool first to avoid the /usr/sbin/ wrapper if possible
    find_program(BPFTOOL NAMES bpftool REQUIRED)

    set(BPF_OBJ "${CMAKE_CURRENT_BINARY_DIR}/${NAME}.bpf.o")
    set(BPF_SKEL_DIR "${CMAKE_CURRENT_BINARY_DIR}/bpf_skeletons")
    set(BPF_SKEL "${BPF_SKEL_DIR}/${NAME}.skel.h")

    file(MAKE_DIRECTORY ${BPF_SKEL_DIR})

    # 2. Dynamic Architecture Detection (Crucial for Open Source)
    # Maps system arch to eBPF target arch (e.g., x86_64 -> x86, aarch64 -> arm64)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        set(BPF_TARGET_ARCH "x86")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(BPF_TARGET_ARCH "arm64")
    else()
        set(BPF_TARGET_ARCH ${CMAKE_SYSTEM_PROCESSOR})
    endif()

    # 3. Kernel Header Resolution
    # We use 'uname -m' dynamically to find the correct system headers for the current environment
    execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH_NAME OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(ARCH_INC "/usr/include/${ARCH_NAME}-linux-gnu")

    # 4. Compile BPF to Object
    # We add -I${ARCH_INC} and -D__TARGET_ARCH_${BPF_TARGET_ARCH} dynamically
    add_custom_command(
        OUTPUT ${BPF_OBJ}
        COMMAND ${CLANG_BPF} -g -O2 -target bpf 
                -D__TARGET_ARCH_${BPF_TARGET_ARCH}
                -I${CMAKE_CURRENT_SOURCE_DIR}/include
                -I${ARCH_INC}
                -c ${SRC} -o ${BPF_OBJ}
        DEPENDS ${SRC}
        COMMENT "[eBPF] Building kernel object: ${NAME} (${BPF_TARGET_ARCH})"
        VERBATIM
    )

    # 5. Generate Skeleton Header
    # Tip: Some bpftool versions require 'sudo' or specific permissions, 
    # but in CI/Docker, 'gen skeleton' is a pure userspace transformation of the ELF file.
    add_custom_command(
        OUTPUT ${BPF_SKEL}
        COMMAND ${BPFTOOL} gen skeleton ${BPF_OBJ} > ${BPF_SKEL}
        DEPENDS ${BPF_OBJ}
        COMMENT "[eBPF] Generating C++ skeleton: ${NAME}"
        VERBATIM
    )

    add_custom_target(${NAME}_skel DEPENDS ${BPF_SKEL})
    
    # Export variables to parent scope
    set(${NAME}_INCLUDE_DIR "${BPF_SKEL_DIR}" PARENT_SCOPE)
    set(${NAME}_OBJECT "${BPF_OBJ}" PARENT_SCOPE)
endfunction()