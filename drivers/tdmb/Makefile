#
# Makefile for kernel SPI drivers.
#

ifeq ($(CONFIG_SPI_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif

# small core, mostly translating board-specific
# config declarations into driver model code
obj-$(CONFIG_SPI_MASTER)		+= tdmb.o
obj-$(CONFIG_SPI_MASTER)		+= INC_INTERFACE.o	INC_PROCESS.o	INC_FICDEC.o	INC_RF_CTRL500.o INC_QFIFO.o


#obj-$(CONFIG_SPI_S5PV210)       += spi_s3c_old.o
# 	... add above this line ...

# SPI protocol drivers (device/link on bus)
# 	... add above this line ...

# SPI slave controller drivers (upstream link)
# 	... add above this line ...

# SPI slave drivers (protocol for that link)
# 	... add above this line ...
