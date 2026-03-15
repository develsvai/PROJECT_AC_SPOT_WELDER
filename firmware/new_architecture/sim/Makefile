CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic
INCLUDES = -I../project/ACSpotWelderNewArch

CORE_SRCS = \
	../project/ACSpotWelderNewArch/app.c \
	../project/ACSpotWelderNewArch/input.c \
	../project/ACSpotWelderNewArch/ui.c \
	../project/ACSpotWelderNewArch/process.c \
	../project/ACSpotWelderNewArch/auto_detect.c

SIM_SRCS = \
	hardware_sim.c \
	settings_sim.c \
	render_sim.c \
	sim_main.c

TARGET = acspot_sim
LIB_TARGET = libacspot_sim.dylib

all: $(TARGET) $(LIB_TARGET)

$(TARGET): $(CORE_SRCS) $(SIM_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(CORE_SRCS) $(SIM_SRCS)

$(LIB_TARGET): $(CORE_SRCS) hardware_sim.c settings_sim.c render_sim.c sim_api.c
	$(CC) $(CFLAGS) $(INCLUDES) -shared -fPIC -o $@ \
		../project/ACSpotWelderNewArch/app.c \
		../project/ACSpotWelderNewArch/input.c \
		../project/ACSpotWelderNewArch/ui.c \
		../project/ACSpotWelderNewArch/process.c \
		../project/ACSpotWelderNewArch/auto_detect.c \
		hardware_sim.c settings_sim.c render_sim.c sim_api.c

clean:
	rm -f $(TARGET) $(LIB_TARGET)
