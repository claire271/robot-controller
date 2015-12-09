#
# Execute 'make' to create pru.bin and main
# Other options:
# make clean
# make all
# make pru
# make project
# make pru
# make clean
#

pru = pru
project = main

LIB_PATH = .
LIBRARIES = pthread prussdrv ncurses
INCLUDES = -I. ${LIB_PATH}

SOURCES =  main.c

EXTRA_DEFINE =
CCCFLAGS = $(EXTRA_DEFINE)
CC = gcc
CFLAGS = $(EXTRA_DEFINE)
PASM = pasm

all : $(pru) $(project)
pru : $(pru)
project: $(project)

$(project) : $(project:%=%.c)
	$(CC) $(CFLAGS) -c -o $@.o $@.c
	$(CC) $@.o $(LIB_PATH:%=-L%) $(LIBRARIES:%=-l%) -o $@

clean :
	rm -rf *.o *.bin $(project) core *~

$(pru) : $(pru:%=%.p)
	$(PASM) -b $@.p

.SUFFIXES: .c.d

%.d: %.c
	$(SHELL) -ec "$(CC) -M $(CPPFLAGS) $< | sed 's/$*\\.o[ :]*/$@ &/g' > $@" -include $(SOURCES:.c=.d)

