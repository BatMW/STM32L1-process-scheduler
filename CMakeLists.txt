cmake_minimum_required(VERSION 3.13)
project(base_hal C CXX ASM)

# Compiler flags
set(CPU_FLAGS "-mcpu=cortex-m3 -mthumb")
set(CMAKE_C_FLAGS "${CPU_FLAGS} -Wall -O2 -g")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "-T${CMAKE_SOURCE_DIR}/stm32_flash.ld -Wl,-Map=${PROJECT_NAME}.map")

# Define STM32 part symbol for HAL/LL drivers
add_compile_definitions(STM32L152xC)

# === Set absolute paths to external libs ===
set(ST_ROOT "$ENV{HOME}/dev/embedded/ST")
set(VENDOR_CMSIS_ROOT "$ENV{HOME}/dev/embedded/vendor/CMSIS")

# Source files
file(GLOB SRC_C ${CMAKE_SOURCE_DIR}/src/*.c)
file(GLOB SRC_HAL "${ST_ROOT}/stm32l1xx-hal-driver/Src/*.c")

list(REMOVE_ITEM SRC_HAL "${ST_ROOT}/stm32l1xx-hal-driver/Src/stm32l1xx_hal_timebase_tim_template.c")

set(SRC
    ${SRC_C}
    ${SRC_HAL}
    ${ST_ROOT}/STM32l1/Source/Templates/system_stm32l1xx.c
    ${ST_ROOT}/STM32l1/Source/Templates/gcc/startup_stm32l152xc.s
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/inc
    ${ST_ROOT}/stm32l1xx-hal-driver/Inc
    ${ST_ROOT}/STM32l1/Include
    ${ST_ROOT}/STM32l1/Source/Templates
    ${VENDOR_CMSIS_ROOT}/CMSIS/Core/Include
)

# Build target
add_executable(${PROJECT_NAME}.elf ${SRC})

# Post-build: create hex, bin, and size output
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE_UTIL} ${PROJECT_NAME}.elf
)
