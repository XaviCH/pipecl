.PHONY: all clean clean-all

all:
	$(MAKE) -C apps
	$(MAKE) -C backend
	$(MAKE) -C egl
	$(MAKE) -C frontend

clean:
	$(MAKE) -C apps clean
	$(MAKE) -C backend clean
	$(MAKE) -C egl clean
	$(MAKE) -C frontend clean

clean-all:
	$(MAKE) -C apps clean-all
	$(MAKE) -C backend clean-all
	$(MAKE) -C egl clean-all
	$(MAKE) -C frontend clean-all