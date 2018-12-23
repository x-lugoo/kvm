all:
	gcc kvm.c -o kvm
run:
	./kvm
clean:
	rm  kvm 
