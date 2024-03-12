cmake_minimum_required(VERSION 3.15)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_C_FLAGS "-mcpu=cortex-m3 -mthumb -Os -g")
set(CMAKE_C_FLAGS_DEBUG "-O0")
project(Firware C)
if(UNIX AND NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT) # Use 'CMAKE_INSTALL_PREFIX' as alternative system root
	set(CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
	include(GNUInstallDirs)
	include_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})
	link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
endif()
if(LIBOPENCM3_DIR)
	include_directories(${LIBOPENCM3_DIR}/include)
	link_directories(${LIBOPENCM3_DIR}/lib)
endif()
add_compile_options(-ffreestanding -ffunction-sections -fdata-sections -fsingle-precision-constant
	-Wall -Wextra -Wpedantic -Wundef -Wshadow -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes
	-Wno-variadic-macros -Wno-unused-result -Wno-unused-parameter)
add_compile_definitions(STM32F1)
add_link_options(-nostartfiles -Wl,--gc-sections)

function(add_object name)
	foreach(name_ ${name} ${ARGN})
		set(src src/${name_}.c)
		list(APPEND srcs ${src})
	endforeach()
	add_library(${name} OBJECT ${srcs})
endfunction()

function(add_target name)
	set(elf ${name}.elf)
	set(bin ${name}.bin)
	set(hex ${name}.hex)
	add_executable(${elf} src/${name}.c)
	target_link_options(${elf} PRIVATE -T${CMAKE_SOURCE_DIR}/src/common.ld)
	target_link_libraries(${elf} c_nano gcc nosys opencm3_stm32f1 ${ARGN})
	add_custom_command(
		OUTPUT ${bin} ${hex}
		COMMAND arm-none-eabi-objcopy -O binary ${elf} ${bin}
		COMMAND arm-none-eabi-objcopy -O ihex ${elf} ${hex}
		DEPENDS ${elf}
	)
	add_custom_target(${name} ALL DEPENDS ${hex})
	add_custom_target(flash-${name} COMMAND st-flash --reset --connect-under-reset --format ihex write ${hex} DEPENDS ${hex})
endfunction()

add_object(serial sensor)

add_target(volvo serial)
