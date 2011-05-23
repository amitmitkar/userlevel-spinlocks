CPPFLAGS += -fPIC -ggdb -Werror -Wall -O3
LIB_SRCS := spinlock.cpp
LIB_OBJS := $(patsubst %.cpp,%.o, $(LIB_SRCS))
LIB_ASM  := $(patsubst %.o,%.s, $(LIB_OBJS))


all: libspin test_lock

libspin: $(LIB_OBJS)
	g++ -shared -ggdb -o libspin.so $(LIB_OBJS) 

test_lock: libspin test_lock.o
	g++ -L. -o test_lock test_lock.o -lpthread -lrt -lspin

%.so.s : %.so
	objdump -S $< > $@

disas: libspin test_lock
	objdump -d libspin.so > libspin.so.s
	objdump -S -d test_lock > test_lock.s

clean:
	rm -f $(LIB_OBJS) libspin.so test_lock test_lock.o $(LIB_ASM) *.s numbers.t*

test: test_lock
	./test.sh 50 3
