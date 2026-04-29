CFLAGS +=

%.so: %.c force
	g++ -shared -o $@ $< -fPIC $(CFLAGS)

force: ;