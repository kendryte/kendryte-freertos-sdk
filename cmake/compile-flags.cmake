add_compile_flags(LD
        -nostartfiles
        -static
        -Wl,--gc-sections
        -Wl,-static
        -Wl,--start-group
        -Wl,--whole-archive
        -Wl,--no-whole-archive
        -Wl,--end-group
        -Wl,-EL
        "-T \"${SDK_ROOT}/lds/kendryte.ld\""
        )

# C Flags Settings
add_compile_flags(BOTH
        -mcmodel=medany
        -mabi=lp64f
        -march=rv64imafc
        -fno-common
        -ffunction-sections
        -fdata-sections
        -fstrict-volatile-bitfields
        -ffast-math
        -fno-math-errno
        -fsingle-precision-constant
        -O2
        -ggdb
        )

add_compile_flags(C -std=gnu11)
add_compile_flags(CXX -std=gnu++17)

if (BUILDING_SDK)
    add_compile_flags(BOTH
            -Wall
            -Werror=all
            -Wno-error=unused-function
            -Wno-error=unused-but-set-variable
            -Wno-error=unused-variable
            -Wno-error=deprecated-declarations
            -Wno-error=maybe-uninitialized
            -Wextra
            -Werror=frame-larger-than=65536
            -Wno-unused-parameter
            -Wno-unused-function
            -Wno-implicit-fallthrough
            -Wno-sign-compare
            -Wno-error=missing-braces
            )

    add_compile_flags(C -Wno-old-style-declaration)
else ()
    add_compile_flags(BOTH -L"${SDK_ROOT}/include/")
endif ()

