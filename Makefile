
all %:
	$(MAKE) -C build $@
clean:
	rm -f Makefile
	rm -rf build
