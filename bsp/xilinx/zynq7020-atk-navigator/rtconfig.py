import os

ARCH = 'arm'
CPU = 'cortex-a'
CROSS_TOOL = 'gcc'
PLATFORM = 'gcc'
EXEC_PATH = os.getenv('RTT_EXEC_PATH') or '/usr/bin'
BUILD = 'release'

LINK_SCRIPT = 'link.lds'

if PLATFORM == 'gcc':
    PREFIX = os.getenv('RTT_CC_PREFIX') or 'arm-none-eabi-'
    CC = PREFIX + 'gcc'
    CPP = PREFIX + 'gcc'
    CPPFLAGS = ' -E -P -x assembler-with-cpp'
    CXX = PREFIX + 'g++'
    AS = PREFIX + 'gcc'
    AR = PREFIX + 'ar'
    LINK = PREFIX + 'gcc'
    TARGET_EXT = 'elf'
    SIZE = PREFIX + 'size'
    OBJDUMP = PREFIX + 'objdump'
    OBJCPY = PREFIX + 'objcopy'
    STRIP = PREFIX + 'strip'

    DEVICE = ' -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=softfp'
    COMMON = DEVICE + ' -ffunction-sections -fdata-sections -fno-strict-aliasing'
    CFLAGS = COMMON + ' -Wall -Wno-cpp -std=gnu99 -fdiagnostics-color=always'
    CXXFLAGS = COMMON + ' -Wall -fno-rtti -fno-exceptions -fdiagnostics-color=always'
    AFLAGS = DEVICE + ' -c -x assembler-with-cpp'
    LFLAGS = DEVICE + ' -Wl,--build-id=none,--gc-sections,-Map=rtthread.map,-cref,-u,system_vectors'
    LFLAGS += ' -T ' + LINK_SCRIPT + ' -nostartfiles -static -lgcc'

    if BUILD == 'debug':
        CFLAGS += ' -O0 -gdwarf-2'
        CXXFLAGS += ' -O0 -gdwarf-2'
        AFLAGS += ' -gdwarf-2'
    else:
        CFLAGS += ' -O2'
        CXXFLAGS += ' -O2'

    POST_ACTION = OBJCPY + ' -O binary $TARGET rtthread.bin\n' + SIZE + ' $TARGET\n'
