all: logtail

%.o: %.cc
	g++ -c --std=c++11 -Wall -o $@  $<

logtail: logtail.o
	g++ -o $@ $^

clean:
	rm -f logtail *.o
