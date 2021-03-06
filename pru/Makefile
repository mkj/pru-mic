# PRU_CGT environment variable must point to the TI PRU compiler directory. E.g.:
#(Linux) export PRU_CGT=/home/jason/ti/ccs_v6_1_0/ccsv6/tools/compiler/ti-cgt-pru_2.1.0
#(Windows) set PRU_CGT=C:/TI/ccs_v6_0_1/ccsv6/tools/compiler/ti-cgt-pru_2.1.0
ifdef PRU_CGT
CLPRU=$(PRU_CGT)/bin/clpru
else
CLPRU=clpru
endif

# default paths for debian beaglebone
ifndef PRU_C_INCLUDES
PRU_C_INCLUDES=--include_path=/usr/share/ti/cgt-pru/include --include_path=/usr/share/ti/cgt-pru/lib
endif

ifndef PRU_C_LIBS
PRU_C_LIBS=--search_path=/usr/share/ti/cgt-pru/include --search_path=/usr/share/ti/cgt-pru/lib
endif

ifndef PRUSDK
PRUSDK=/usr/lib/ti/pru-software-support-package
endif


MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIR := $(notdir $(patsubst %/,%,$(dir $(MKFILE_PATH))))
PROJ_NAME=$(CURRENT_DIR)
LINKER_COMMAND_FILE=./AM335x_PRU.cmd
LIBS=$(PRU_C_LIBS) --library=$(PRUSDK)/lib/rpmsg_lib.lib
INCLUDE=$(PRU_C_INCLUDES) --include_path=$(PRUSDK)/include --include_path=$(PRUSDK)/include/am335x
STACK_SIZE=0x100
HEAP_SIZE=0x100
GEN_DIR=gen

#Common compiler and linker flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
CFLAGS=-v3 --display_error_number --endian=little --hardware_mac=on --obj_directory=$(GEN_DIR) --pp_directory=$(GEN_DIR) --asm_directory=$(GEN_DIR) -ppd -ppa --c99
# generate interleaved assembly output
CFLAGS += -k  --symdebug:none
# speed vs size. Don't go higher than -O2 since that involves link time optimisation etc.
CFLAGS += -mf5 -O4
#Linker flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
LFLAGS=--reread_libs --warn_sections --stack_size=$(STACK_SIZE) --heap_size=$(HEAP_SIZE)

ifdef TEST
CFLAGS += -DTEST_CLOCK_OUT
endif

MAP=$(GEN_DIR)/$(PROJ_NAME).map
#Using .object instead of .obj in order to not conflict with the CCS build process

TARGET=$(GEN_DIR)/main_pru1.out $(GEN_DIR)/main_pru0.out
COMMON_OBJECTS = $(GEN_DIR)/asmpr0.object

all: printStart $(TARGET) printEnd

printStart:
	@echo ''
	@echo '************************************************************'
	@echo 'Building project: $(PROJ_NAME)'

printEnd:
	@echo ''
	@echo 'Finished building project: $(PROJ_NAME)'
	@echo '************************************************************'
	@echo ''

# Invokes the linker (-z flag) to make the .out file
$(GEN_DIR)/%.out: $(GEN_DIR)/%.object $(LINKER_COMMAND_FILE) $(COMMON_OBJECTS)
	@echo ''
	@echo 'Building target: $@'
	@echo 'Invoking: PRU Linker'
	$(CLPRU) $(CFLAGS) -z $(LIBS) $(LFLAGS) -o $@ $^ -m$(MAP) --library=libc.a $(LIBS)
	@echo 'Finished building target: $@'
	@echo ''
	@echo 'Output files can be found in the "$(GEN_DIR)" directory'

# Invokes the compiler on all c files in the directory to create the object files
$(GEN_DIR)/%.object: %.c
	@mkdir -p $(GEN_DIR)
	@echo ''
	@echo 'Building file: $<'
	@echo 'Invoking: PRU Compiler'
	$(CLPRU) $(INCLUDE) $(CFLAGS) -fe $@ $<

$(GEN_DIR)/%.object: %.asm
	$(CLPRU) $(INCLUDE) $(CFLAGS) -fe $@ $<

install: $(TARGET)
	@echo Installing
	cp -fv gen/main_pru0.out /lib/firmware/am335x-pru0-fw
	cp -fv gen/main_pru1.out /lib/firmware/am335x-pru1-fw

load:
	#rmmod pru_rproc
	#modprobe pru_rproc
	./setpins
	echo stop > /sys/class/remoteproc/remoteproc1/state || true
	echo stop > /sys/class/remoteproc/remoteproc2/state || true
	echo am335x-pru0-fw > /sys/class/remoteproc/remoteproc1/firmware
	echo am335x-pru1-fw > /sys/class/remoteproc/remoteproc2/firmware
	echo start > /sys/class/remoteproc/remoteproc1/state
	echo start > /sys/class/remoteproc/remoteproc2/state
	dmesg | grep pru | tail -3

.PHONY: all clean load

# Remove the $(GEN_DIR) directory
clean:
	@echo ''
	@echo '************************************************************'
	@echo 'Cleaning project: $(PROJ_NAME)'
	@echo ''
	@echo 'Removing files in the "$(GEN_DIR)" directory'
	@rm -rf $(GEN_DIR)
	@echo ''
	@echo 'Finished cleaning project: $(PROJ_NAME)'
	@echo '************************************************************'
	@echo ''

# Includes the dependencies that the compiler creates (-ppd and -ppa flags)
-include $(OBJECTS:%.object=%.pp)

