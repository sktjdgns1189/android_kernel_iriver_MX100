cmd_arch/arm/plat-s5p/sleep.o := /home/sunghun/arm-2009q3/bin/arm-none-linux-gnueabi-gcc -Wp,-MD,arch/arm/plat-s5p/.sleep.o.d  -nostdinc -isystem /home/sunghun/arm-2009q3/bin/../lib/gcc/arm-none-linux-gnueabi/4.4.1/include -Iinclude  -I/media/5fbea9a5-212a-4618-914c-484220a5fb21/itb/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-s5pv210/include -Iarch/arm/plat-s5p/include -Iarch/arm/plat-samsung/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=7 -march=armv7-a  -include asm/unified.h -msoft-float -gdwarf-2       -c -o arch/arm/plat-s5p/sleep.o arch/arm/plat-s5p/sleep.S

deps_arch/arm/plat-s5p/sleep.o := \
  arch/arm/plat-s5p/sleep.S \
    $(wildcard include/config/debug/resume.h) \
  /media/5fbea9a5-212a-4618-914c-484220a5fb21/itb/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \
  include/linux/linkage.h \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  /media/5fbea9a5-212a-4618-914c-484220a5fb21/itb/arch/arm/include/asm/linkage.h \
  /media/5fbea9a5-212a-4618-914c-484220a5fb21/itb/arch/arm/include/asm/assembler.h \
    $(wildcard include/config/cpu/feroceon.h) \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/smp.h) \
  /media/5fbea9a5-212a-4618-914c-484220a5fb21/itb/arch/arm/include/asm/ptrace.h \
    $(wildcard include/config/cpu/endian/be8.h) \
    $(wildcard include/config/arm/thumb.h) \
  /media/5fbea9a5-212a-4618-914c-484220a5fb21/itb/arch/arm/include/asm/hwcap.h \
  arch/arm/mach-s5pv210/include/mach/hardware.h \
  arch/arm/mach-s5pv210/include/mach/map.h \
    $(wildcard include/config/mach/smdkc110.h) \
  arch/arm/plat-samsung/include/plat/map-base.h \
  arch/arm/plat-s5p/include/plat/map-s5p.h \
  include/asm/asm-offsets.h \
  /media/5fbea9a5-212a-4618-914c-484220a5fb21/itb/arch/arm/include/asm/memory.h \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/page/offset.h) \
    $(wildcard include/config/highmem.h) \
    $(wildcard include/config/dram/size.h) \
    $(wildcard include/config/dram/base.h) \
    $(wildcard include/config/zone/dma.h) \
    $(wildcard include/config/discontigmem.h) \
  include/linux/const.h \
  arch/arm/mach-s5pv210/include/mach/memory.h \
  /media/5fbea9a5-212a-4618-914c-484220a5fb21/itb/arch/arm/include/asm/sizes.h \
  include/asm-generic/memory_model.h \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
    $(wildcard include/config/sparsemem.h) \
  /media/5fbea9a5-212a-4618-914c-484220a5fb21/itb/arch/arm/include/asm/system.h \
    $(wildcard include/config/cpu/xsc3.h) \
    $(wildcard include/config/cpu/fa526.h) \
    $(wildcard include/config/cpu/sa1100.h) \
    $(wildcard include/config/cpu/sa110.h) \
    $(wildcard include/config/cpu/32v6k.h) \
  arch/arm/mach-s5pv210/include/mach/regs-gpio.h \
  arch/arm/mach-s5pv210/include/mach/gpio-bank-a0.h \
  arch/arm/mach-s5pv210/include/mach/gpio-bank-a1.h \
  arch/arm/mach-s5pv210/include/mach/regs-clock.h \
    $(wildcard include/config/cpu/s5pv210/evt1.h) \
  arch/arm/mach-s5pv210/include/mach/regs-mem.h \
  arch/arm/plat-samsung/include/plat/regs-serial.h \
    $(wildcard include/config/cpu/s5pv210.h) \

arch/arm/plat-s5p/sleep.o: $(deps_arch/arm/plat-s5p/sleep.o)

$(deps_arch/arm/plat-s5p/sleep.o):
