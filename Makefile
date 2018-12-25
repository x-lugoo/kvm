
.PHONY: run

kvm: kvm.o payload.o
	$(CC) $^ -o $@

payload.o: payload.ld guest16.o
	$(LD) -T $<  -o $@

run:
	./kvm
clean:
	rm  *.o kvm 
