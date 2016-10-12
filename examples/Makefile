target=main
GXX=gcc
INCLUDE=-I..
CXXFLAGS=-g $(INCLUDE) 
LDFLAGS=

$(target): coroutine.o main.o
	$(GXX) $(CXXFLAGS) -o $(target) $^ $(LDFLAGS)

main.o: main.c
	$(GXX) $(CXXFLAGS) -o $@ -c $<

coroutine.o: ../coroutine.c ../coroutine.h
	$(GXX) $(CXXFLAGS) -o $@ -c $<

clean:
	rm -fr *.o $(target)
