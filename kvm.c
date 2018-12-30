#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#include "kvm.h"

void vm_init(struct vm *vm , size_t mem_size)
{
	int api_ver;
	struct kvm_userspace_memory_region memreg;

	vm->sys_fd = open("/dev/kvm", O_RDWR);
	if (vm->sys_fd < 0) {
		perror("open /dev/kvm");
		exit(1);
	}

	api_ver = ioctl(vm->sys_fd, KVM_GET_API_VERSION, 0);
	if (api_ver < 0) {
		perror("KVM_GET_API_VERSION");
		exit(1);
	}
	printf("Get api version %d KVM_API_VERSION %d \n",api_ver,KVM_API_VERSION);
	
	if (api_ver != KVM_API_VERSION) {
		fprintf(stderr , "Got KVM api version %d, expected %d\n",
				api_ver, KVM_GET_API_VERSION);
		exit(1);
	}

	vm->fd = ioctl(vm->sys_fd, KVM_CREATE_VM, 0);
	if (vm->fd < 0) {
		perror("KVM_CREATE_VM");
		exit(1);
	}

	if (ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xfffdd000) < 0) {
		perror("KVM_SET_TSS_ADDR");
		exit(1);
	}

	vm->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1 , 0);
	if (vm->mem == MAP_FAILED) {
		perror("mmap mem");
		exit(1);
	}

	madvise(vm->mem, mem_size, MADV_MERGEABLE);

	memreg.slot = 0;
	memreg.flags = 0;
	memreg.guest_phys_addr = 0x0;
	memreg.memory_size = mem_size;
	memreg.userspace_addr = (unsigned long)vm->mem;
	if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &memreg) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
		exit(1);
	}
}



void vcpu_init(struct vm *vm, struct vcpu *vcpu)
{
	int vcpu_mmap_size;

	vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
        if (vcpu->fd < 0) {
		perror("KVM_CREATE_VCPU");
                exit(1);
	}

	vcpu_mmap_size = ioctl(vm->sys_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
        if (vcpu_mmap_size <= 0) {
		perror("KVM_GET_VCPU_MMAP_SIZE");
                exit(1);
	}

	/*get to data from kernel about struct kvm_run with much status messages about vcpu*/
	vcpu->kvm_run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE,
			     MAP_SHARED, vcpu->fd, 0);
	if (vcpu->kvm_run == MAP_FAILED) {
		perror("mmap kvm_run");
		exit(1);
	}
}

#define READ_REGS()				 \
	do{					\
		if (ioctl(vcpu->fd, KVM_GET_REGS, &regs) < 0) { \
			perror("KVM_GET_REGS");   		\
			exit(1);    				\
		}                     				\
	}while(0);              				

int run_vm(struct vm *vm, struct vcpu *vcpu, size_t sz)
{
	struct kvm_regs regs;
	u_int64_t memval = 0;
	
	for (;;) {
		if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
			perror("KVM_RUN");
			exit(1);
		}

		switch (vcpu->kvm_run->exit_reason) {
		case KVM_EXIT_HLT:
			printf("KVM_EXIT_HLT(%d)\n", KVM_EXIT_HLT);
			kvm_show_regs(vcpu);
			goto check;

		case KVM_EXIT_IO:
			if(vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT &&
					vcpu->kvm_run->io.size == 1 &&
					vcpu->kvm_run->io.port == 0x3f8 &&
					vcpu->kvm_run->io.count == 1)
				printf("KVM_EXIT_IO IO output: %c \n", 
					*(((char*)vcpu->kvm_run) + vcpu->kvm_run->io.data_offset));
			else if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_IN &&
					vcpu->kvm_run->io.size == 1 &&
					vcpu->kvm_run->io.port == 0x3f8 &&
					vcpu->kvm_run->io.count == 1)
				((char*)vcpu->kvm_run + vcpu->kvm_run->io.data_offset)[0] = 'X';
		
			//kvm_show_regs(vcpu);
			break;

		case KVM_EXIT_MMIO:
			printf("KVM_EXIT_MMIO(%d)\n", KVM_EXIT_MMIO);
			kvm_show_regs(vcpu);
			break;

		case KVM_EXIT_INTR:
			printf("KVM_EXIT_INTR(%d)\n", KVM_EXIT_INTR);
			kvm_show_regs(vcpu);
			break;

		case KVM_EXIT_INTERNAL_ERROR:
			printf("KVM_EXIT_INTERNAL_ERROR(%d)  0x%x\n",
				    KVM_EXIT_INTERNAL_ERROR, vcpu->kvm_run->internal.suberror);
			kvm_show_regs(vcpu);
			exit(1);

		case KVM_EXIT_FAIL_ENTRY:
			printf("KVM_EXIT_FAIL_ENTRY(%d) hardware_entry_failure_reason 0x%llx\n",
				    KVM_EXIT_FAIL_ENTRY, 
				    (unsigned long long )vcpu->kvm_run->fail_entry.hardware_entry_failure_reason);
			kvm_show_regs(vcpu);
			exit(1);

		default:
			fprintf(stderr,"Got exit reason %d\n", vcpu->kvm_run->exit_reason);
			kvm_show_regs(vcpu);
			break;
		}
	}
	return 0;

check:
	READ_REGS();	
	if (regs.rax != 99) {
		printf("wrong result: AX is %lld\n", regs.rax);
		return 1;
	}

	memcpy(&memval, &vm->mem[0x502], sz);
	if(memval != 99) {
		printf("wrong result :memory at 0x502 is %lld\n",
			(unsigned long long)memval);
		return 1;
	}
	kvm_show_regs(vcpu);

	printf("The right AX is %lld memval is %lld\n", regs.rax, (unsigned long long )memval);
				

	return 0;
}

extern const unsigned char guest16_start[],guest16_end[];

int run_real_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;

	printf("testing real mode\n");

	if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	sregs.cs.selector = 0x1000; 
	/*
	 *KVM on Intel requires 'base' to be 'selector * 16' in real mode - by jeff
	 */
	sregs.cs.base = sregs.cs.selector << 4;

	if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	regs.rflags = 2; /*for kvm .must set*/
	regs.rip = 0x200;    
	regs.rax = 4;    /*for ax + bx to verify the result(9)*/
	regs.rbx = 5;

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	memcpy(vm->mem + (0x1000 << 4) + 0x200, guest16_start, guest16_end-guest16_start);
	return run_vm(vm, vcpu, 2);
}

int main(void)
{
	struct vm vm;
	struct vcpu vcpu;

	vm_init(&vm, 0x200000);
	vcpu_init(&vm, &vcpu);
	run_real_mode(&vm, &vcpu);

	return 0;
}






