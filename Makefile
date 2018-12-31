

all: kvm test

OBJS += kvm.o
OBJS += libkvm.o
OBJS += early_printk.o
OBJS += ioport.o
OBJS += payload.o

kvm: $(OBJS)
	$(CC) $^ -o $@

payload.o: payload.ld guest16.o
	$(LD) -T $<  -o $@

ioport.o: ioport.c ioport.h
	$(CC) $< -c 

early_printk.o: early_printk.c early_printk.h
	$(CC) $< -c 
test:
	make -C ./tests
run:
	./kvm
clean:
	rm  *.o kvm 
.PHONY: run clean test
