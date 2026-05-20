cmake_minimum_required(VERSION 3.22)
project(SmartHood_Firmware C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================
# 目标芯片
# ============================================================
set(CPU_FLAGS
    -mcpu=cortex-m33
    -mthumb
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
)

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${CPU_FLAGS} -Wall -Wextra -Os -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_SOURCE_DIR}/STM32H562VITX_FLASH.ld -Wl,--gc-sections")

# ============================================================
# 芯片定义
# ============================================================
add_definitions(
    -DSTM32H562xx
    -DUSE_HAL_DRIVER
    -DARM_MATH_CM33
    -DARM_MATH_DSP
)

# ============================================================
# 包含路径
# ============================================================
include_directories(
    Core/Inc
    Drivers/SHT40
    Drivers/SGP40
    Drivers/PMS7003
    Drivers/Hall
    Middleware/AI
    Drivers/CMSIS/Include
    Drivers/STM32H5xx_HAL_Driver/Inc
)

# ============================================================
# 源文件
# ============================================================

# --- Core ---
set(CORE_SOURCES
    Core/Src/main.c
    Core/Src/sensor_hub.c
    Core/Src/ai_engine.c
    Core/Src/motor_control.c
    Core/Src/openmv_bridge.c
    Core/Src/system_manager.c
)

# --- 传感器驱动 ---
set(DRIVER_SOURCES
    Drivers/SHT40/sht40.c
    Drivers/SGP40/sgp40.c
    Drivers/PMS7003/pms7003.c
    Drivers/Hall/hall.c
)

# --- HAL 驱动 (STM32Cube 生成) ---
set(HAL_SOURCES
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_cortex.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gpio.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_i2c.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_adc.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_tim.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_iwdg.c
    Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr_ex.c
)

# --- 启动 + 系统 ---
set(STARTUP_SOURCES
    Drivers/CMSIS/Device/ST/STM32H5xx/Source/Templates/gcc/startup_stm32h562xx.s
    Drivers/CMSIS/Device/ST/STM32H5xx/Source/Templates/system_stm32h5xx.c
)

# --- AI Middleware (STM32Cube.AI生成) ---
# 取消注释以下行当model.c已由STM32Cube.AI生成:
# set(AI_SOURCES
#     Middleware/AI/model.c
#     Middleware/AI/network.c
#     Middleware/AI/network_data.c
# )
# add_definitions(-DAI_MODEL_ENABLED)

# ============================================================
# 可执行文件
# ============================================================
add_executable(${PROJECT_NAME}
    ${CORE_SOURCES}
    ${DRIVER_SOURCES}
    ${HAL_SOURCES}
    ${STARTUP_SOURCES}
    ${AI_SOURCES}
)

# ============================================================
# 链接库
# ============================================================
target_link_libraries(${PROJECT_NAME}
    -lc -lm -lnosys
    -Wl,--start-group
    -larm_cortexM33l_math   # CMSIS-DSP
    -Wl,--end-group
)

# ============================================================
# 烧录/调试目标 (OpenOCD)
# ============================================================
add_custom_target(flash
    COMMAND openocd -f interface/stlink.cfg -f target/stm32h5x.cfg
            -c "program ${PROJECT_NAME}.elf verify reset exit"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Flashing firmware to STM32H562VIT6"
)

add_custom_target(debug
    COMMAND openocd -f interface/stlink.cfg -f target/stm32h5x.cfg
    COMMENT "Starting OpenOCD debug server"
)
