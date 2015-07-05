all: flow flow.1.gz

flow: flow.c
	gcc -o flow flow.c -lm

clean:
	rm -f *.o *~ flow flow.1.gz

flow.1.gz:
	gzip <flow.1 >flow.1.gz

install:
	rm -rf $(RPM_BUILD_ROOT)
	mkdir -p $(RPM_BUILD_ROOT)/usr/bin
	mkdir -p $(RPM_BUILD_ROOT)/usr/share/man/man1
	install -s -m 755 flow $(RPM_BUILD_ROOT)/usr/bin/flow
	install -m 644 flow.1.gz $(RPM_BUILD_ROOT)/usr/share/man/man1/flow.1.gz
