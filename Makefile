
.PHONY: run

kvm: kvm.o libkvm.o payload.o
	$(CC) $^ -o $@

payload.o: payload.ld guest16.o
	$(LD) -T $<  -o $@

run:
	./kvm
clean:
	rm  *.o kvm 
