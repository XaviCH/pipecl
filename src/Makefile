TARGETS = \
	backend \
	compiler \
	egl \
	frontend \
	middleware

COMMANDS = \
	clean \
	clean-all

.PHONY: all $(COMMANDS) $(TARGETS)

all: TARGETS

$(TARGETS):
	$(MAKE) -C $@

$(COMMANDS):
	for TARGET in $(TARGETS); do $(MAKE) -C $$TARGET $@; done
