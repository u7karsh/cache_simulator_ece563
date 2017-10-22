CC = gcc
#OPT = -g
OPT = -O3 -m32 -std=c99
WARN = -Wall
CFLAGS = $(OPT) $(WARN) $(INC) $(LIB)

# List all your .cc files here (source files, excluding header files)
SIM_SRC = main.c cache.c

# List corresponding compiled object files here (.o files)
SIM_OBJ = main.o cache.o
 
#################################

# default rule
all: sim_cache
	@echo "my work is done here..."


# rule for making sim_cache
sim_cache: $(SIM_OBJ)
	$(CC) -o sim_cache $(CFLAGS) $(SIM_OBJ) -lm
	@echo "-----------DONE WITH SIM_CACHE-----------"


%.o:
	$(CC) $(CFLAGS) -c $*.c


clean:
	rm -f *.o sim_cache


clobber:
	rm -f *.o

