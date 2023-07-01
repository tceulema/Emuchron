#
# Monochron firmware makefile
#

# MCU name
MCU = atmega328p
F_CPU = 8000000

# Output format, can be [srec, ihex, binary]
FORMAT = ihex

# Target file name (without extension)
TARGET = monochron

# Target file name (with extensions)
TARGET_EXT = a90 cof eep elf hex lnk lss map obj sym
TARGET_FILES = $(addprefix $(TARGET).,$(TARGET_EXT))

# Optimization level, can be [0, 1, 2, 3, s]. 0 turns off optimization.
# Note: Level 3 is not always the best. See avr-libc FAQ.
OPT = s

# The base source files for creating Monochron firmware. What needs to be
# added are the required clock plugins.
BASE = anim.c buttons.c config.c glcd.c i2c.c ks0108.c monomain.c util.c

# Below are examples of SRC definitions for Monochron firmware.
# For the C source files the C dependencies are automatically generated.
# Based on the selection of source files in SRC, configure the clocks in
# monochron[] in anim.c [firmware].

# Monochron clocks - 1
SRC = $(BASE) \
  clock/digital.c clock/analog.c clock/puzzle.c clock/spotfire.c \
  clock/cascade.c clock/speeddial.c clock/spiderplot.c clock/trafficlight.c

# Monochron clocks - 2
#SRC = $(BASE) \
#  clock/example.c clock/marioworld.c clock/mosquito.c clock/nerd.c \
#  clock/pong.c clock/slider.c clock/wave.c

# Monochron clocks - 3
#SRC = $(BASE) \
#  clock/analog.c clock/bigdigit.c clock/digital.c clock/qr.c clock/qrencode.c

# Monochron clocks - 4
#SRC = $(BASE) \
#  clock/spotfire.c clock/barchart.c clock/cascade.c clock/crosstable.c \
#  clock/linechart.c clock/piechart.c clock/speeddial.c clock/spiderplot.c \
#  clock/thermometer.c clock/trafficlight.c

# Monochron clocks - 5
#SRC = $(BASE) \
#  clock/dali.c clock/example.c

# Monochron clocks - 6 - glcd performance test (that uses analog clock code)
#SRC = $(BASE) \
#  clock/perftest.c clock/analog.c

# Monochron clocks - 7 - all clocks
# Note: This clock set will (attempt to) build all clocks. You will see linker
# failures though to indicate that the text object size is too large. Ignore
# these errors as this build serves only as a test to verify whether all
# individual clock modules will build cleanly.
#SRC = $(BASE) \
#  clock/analog.c clock/bigdigit.c clock/dali.c clock/digital.c \
#  clock/example.c clock/marioworld.c clock/mosquito.c clock/nerd.c \
#  clock/perftest.c clock/pong.c clock/puzzle.c clock/qr.c clock/qrencode.c \
#  clock/slider.c clock/wave.c clock/spotfire.c clock/barchart.c \
#  clock/cascade.c clock/crosstable.c clock/linechart.c clock/piechart.c \
#  clock/speeddial.c clock/spiderplot.c clock/thermometer.c \
#  clock/trafficlight.c

# List Assembler source files here.
# Make them always end in a capital .S. Files ending in a lowercase .s will not
# be considered source files but generated files (assembler output from the
# compiler), and will be deleted upon "make clean"! Even though the DOS/Win*
# filesystem matches both .s and .S the same, it will preserve the spelling of
# the filenames, and gcc itself does care about how the name is spelled on its
# command-line.
ASRC =

# List any extra directories to look for include files here. Each directory
# must be seperated by a space.
EXTRAINCDIRS =

# Optional compiler flags:
#  -g:        generate debugging information (for gdb, or for COFF conversion)
#  -O*:       optimization level
#  -MMD:      generate *.d dependency files
#  -f...:     tuning, see gcc manual and avr-libc documentation
#  -Wall...:  warning level
#  -Wa,...:   tell gcc to pass this to the assembler
#  -ahlms:    create assembler listing
CFLAGS = -g -O$(OPT) -MMD \
-funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums \
-Wall -Wstrict-prototypes -DF_CPU=$(F_CPU) \
-Wa,-adhlns=$(<:.c=.lst) $(patsubst %,-I%,$(EXTRAINCDIRS))

# Set a "language standard" compiler flag:
#   gnu99 = C99 + GNU extensions. See gcc manual for more information.
CFLAGS += -std=gnu99

# Optional assembler flags:
#  -Wa,...:   tell gcc to pass this to the assembler
#  -ahlms:    create listing
#  -gstabs:   have the assembler create line number information; note that for
#             use in COFF files, additional information about filenames and
#             function names needs to be present in the assembler source files
ASFLAGS = -Wa,-adhlns=$(<:.S=.lst),-gstabs

# Optional linker flags:
#  -Wl,...:   tell gcc to pass this to linker
#  -Map:      create map file
#  --cref:    add cross reference to map file
LDFLAGS = -Wl,-Map=$(TARGET).map,--cref

# Additional libraries

# Minimalistic printf version
#LDFLAGS += -Wl,-u,vfprintf -lprintf_min

# Floating point printf version (requires -lm below)
#LDFLAGS += -Wl,-u,vfprintf -lprintf_flt

# Standard math library
LDFLAGS += -lm

# ---------------------------------------------------------------------------

# Programming support using avrdude. For a full listing type: avrdude -c ?
AVRDUDE_PROGRAMMER = arduino
AVRDUDE_PORT = /dev/ttyUSB0
AVRDUDE_WRITE_FLASH = -U flash:w:$(TARGET).hex
AVRDUDE_BAUD = 57600
AVRDUDE_FLAGS = -b $(AVRDUDE_BAUD) -p $(MCU) -P $(AVRDUDE_PORT) -c $(AVRDUDE_PROGRAMMER)

# Uncomment the following if you want avrdude's erase cycle counter.
# Note that this counter needs to be initialized first using -Yn, see avrdude
# manual.
#AVRDUDE_ERASE += -y

# Uncomment the following if you do /not/ wish a verification to be performed
# after programming the device.
#AVRDUDE_FLAGS += -V

# Increase verbosity level. Please use this when submitting bug reports about
# avrdude.
# See <http://savannah.nongnu.org/projects/avrdude> to submit bug reports.
#AVRDUDE_FLAGS += -v

# ---------------------------------------------------------------------------

# Define programs and commands
SHELL = sh
CC = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
SIZE = avr-size
REMOVE = rm --force
COPY = cp

# Programming support using avrdude
AVRDUDE = avrdude

# Target sizes
HEXSIZE = $(SIZE) --target=$(FORMAT) $(TARGET).hex
ELFSIZE = $(SIZE) -A $(TARGET).elf

# Define Messages
MSG_ERRORS_NONE = Errors: none
MSG_BEGIN = -------- begin --------
MSG_END = --------  end  --------
MSG_SIZE_BEFORE = Size before:
MSG_SIZE_AFTER = Size after:
MSG_COFF = Converting to avr coff:
MSG_EXTENDED_COFF = Converting to avr extended coff:
MSG_FLASH = Creating load file for flash:
MSG_EEPROM = Creating load file for eeprom:
MSG_EXTENDED_LISTING = Creating extended listing:
MSG_SYMBOL_TABLE = Creating symbol table:
MSG_LINKING = Linking:
MSG_COMPILING = Compiling:
MSG_ASSEMBLING = Assembling:
MSG_CLEANING = Cleaning project:

# Define all object files
OBJ = $(SRC:.c=.o) $(ASRC:.S=.o)

# Define all listing files
LST = $(SRC:.c=.lst) $(ASRC:.S=.lst)

# Combine all necessary flags and optional flags.
# Add target processor to flags.
ALL_CFLAGS = -mmcu=$(MCU) -I. $(CFLAGS)
ALL_ASFLAGS = -mmcu=$(MCU) -I. -x assembler-with-cpp $(ASFLAGS)

# ---------------------------------------------------------------------------

# Build project (= monochron firmware)
all: begin gccversion sizebefore $(TARGET).elf $(TARGET).hex $(TARGET).eep \
	$(TARGET).lss $(TARGET).sym sizeafter finished end

# Clean project
clean: begin clean_list finished end

# Rebuild project
rebuild: begin gccversion sizebefore clean_list $(TARGET).elf $(TARGET).hex \
        $(TARGET).eep $(TARGET).lss $(TARGET).sym sizeafter finished end

# ---------------------------------------------------------------------------

# Upload firmware to monochron
program: $(TARGET).hex $(TARGET).eep
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_WRITE_FLASH)

# Verify connection to monochron
connect:
	$(AVRDUDE) $(AVRDUDE_FLAGS)

# Burn fuses in monochron
#burn-fuse:
#	$(AVRDUDE) $(AVRDUDE_FLAGS) -u -U lfuse:w:0xE2:m -u -U hfuse:w:0xD7:m

# ---------------------------------------------------------------------------

# Eye candy.
# AVR Studio 3.x does not check make's exit code but relies on the following
# magic strings to be generated by the compile job.
begin:
	@echo $(MSG_BEGIN)

finished:
	@echo $(MSG_ERRORS_NONE)

end:
	@echo $(MSG_END)

# Display size of file
sizebefore:
	@if [ -f $(TARGET).elf ]; then echo $(MSG_SIZE_BEFORE); $(ELFSIZE); fi

sizeafter:
	@if [ -f $(TARGET).elf ]; then echo; echo $(MSG_SIZE_AFTER); $(ELFSIZE); fi

# Display compiler version information
gccversion:
	@$(CC) --version

# ---------------------------------------------------------------------------

# Convert ELF to COFF for use in debugging/simulating in AVR Studio or VMLAB
COFFCONVERT=$(OBJCOPY) --debugging \
	--change-section-address .data-0x800000 \
	--change-section-address .bss-0x800000 \
	--change-section-address .noinit-0x800000 \
	--change-section-address .eeprom-0x810000

coff: $(TARGET).elf
	@echo
	@echo $(MSG_COFF) $(TARGET).cof
	$(COFFCONVERT) -O coff-avr $< $(TARGET).cof

extcoff: $(TARGET).elf
	@echo
	@echo $(MSG_EXTENDED_COFF) $(TARGET).cof
	$(COFFCONVERT) -O coff-ext-avr $< $(TARGET).cof

# Create final output files (.hex, .eep) from ELF output file
%.hex: %.elf
	@echo
	@echo $(MSG_FLASH) $@
	$(OBJCOPY) -O $(FORMAT) -R .eeprom $< $@

%.eep: %.elf
	@echo
	@echo $(MSG_EEPROM) $@
	-$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" \
	--change-section-lma .eeprom=0 -O $(FORMAT) $< $@

# Create extended listing file from ELF output file
%.lss: %.elf
	@echo
	@echo $(MSG_EXTENDED_LISTING) $@
	$(OBJDUMP) -h -S $< > $@

# Create a symbol table from ELF output file
%.sym: %.elf
	@echo
	@echo $(MSG_SYMBOL_TABLE) $@
	avr-nm -n $< > $@

# Link: create ELF output file from object files
.SECONDARY: $(TARGET).elf
.PRECIOUS: $(OBJ)
%.elf: $(OBJ)
	@echo
	@echo $(MSG_LINKING) $@
	$(CC) --output $@ $(OBJ) -mmcu=$(MCU) $(LDFLAGS)

# Compile: create object files from C source files
%.o: %.c
	@echo
	@echo $(MSG_COMPILING) $<
	$(CC) -c $(ALL_CFLAGS) -DTXMODE=$(TXMODE) $< -o $@

# Compile: create assembler files from C source files
%.s: %.c
	$(CC) -S $(ALL_CFLAGS) $< -o $@

# Assemble: create object files from assembler source files
%.o: %.S
	@echo
	@echo $(MSG_ASSEMBLING) $<
	$(CC) -c $(ALL_ASFLAGS) $< -o $@

# Cleanup: remove project target and intermediate products
clean_list:
	@echo $(MSG_CLEANING)
	$(REMOVE) $(TARGET_FILES)
	$(REMOVE) $(OBJ)
	$(REMOVE) $(LST)
	$(REMOVE) $(SRC:.c=.s)
	$(REMOVE) $(SRC:.c=.d)

# Include the dependency files generated by gcc flag -MMD
-include $(SRC:.c=.d)

# Listing of phony targets
.PHONY: all begin finish end sizebefore sizeafter gccversion coff extcoff \
	clean clean_list rebuild connect program

