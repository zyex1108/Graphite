SIM_ROOT ?= $(CURDIR)

MPDS ?= 1

CLEAN=$(findstring clean,$(MAKECMDGOALS))

all: $(SIM_ROOT)/lib/libcarbon_sim.a $(SIM_ROOT)/pin/../lib/pin_sim.so

ifeq ($(CLEAN),)
include common/Makefile
include tests/apps/Makefile
include tests/unit/Makefile
include tests/benchmarks/Makefile
endif

$(SIM_ROOT)/pin/../lib/pin_sim.so:
	$(MAKE) -C $(SIM_ROOT)/pin $@

clean: empty_logs
	$(MAKE) -C pin clean
	$(MAKE) -C common clean
	$(MAKE) -C tests/unit clean
	$(MAKE) -C tests/apps clean
	$(MAKE) -C tests/benchmarks clean

regress_quick: clean $(TEST_APP_LIST) $(TEST_UNIT_LIST)

empty_logs :
	rm output_files/* ; true

run_mpd:
	$(MPI_DIR)/bin/mpdboot -n $(MPDS)

stop_mpd:
	$(MPI_DIR)/bin/mpdallexit

love:
	@echo "not war!"

out:
	@echo "I think we should just be friends..."

