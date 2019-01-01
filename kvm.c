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
#include "early_printk.h"

static void usage(char *argv[])
{
	fprintf(stderr, " usage: %s [--kernel=]<kernel-image-path>\n" \
			"        or %s ./kernel-image-path\n" \
			"        or %s --test \n"            \
			"notice: you can use %s ./tests/pit/tick.bin to test\n",
			argv[0], argv[0], argv[0], argv[0]);
	exit(1);
}

static inline uint32_t segment_to_flat(uint16_t selector, uint16_t offset)
{
	return ((uint32_t)selector << 4) + (uint32_t)offset;
}

static inline void *guest_flat_to_host(struct kvm *self, unsigned long offset)
{
	return self->mem + offset;
}

static inline void *guest_real_to_host(struct kvm *self, uint16_t selector, uint16_t offset)
{
	unsigned long flat = segment_to_flat(selector, offset);

	return guest_flat_to_host(self, flat);
}



void vm_init(struct kvm *vm , size_t mem_size)
{
	int api_ver;
	struct kvm_userspace_memory_region memreg;
	struct kvm_pit_config pit_config = { .flags = 0, };

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

	if (ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
		perror("KVM_SET_TSS_ADDR");
		exit(1);
	}

	if (ioctl(vm->fd, KVM_CREATE_IRQCHIP) < 0) {
		perror("KVM_CREATE_IRQCHIP ioctl");
		exit(1);
	}

	if (ioctl(vm->fd, KVM_CREATE_PIT2, &pit_config) < 0) {
		perror("KVM_CREATE_PIT2 ioctl");
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
	memreg.guest_phys_addr = 0x00;
	memreg.memory_size = mem_size;
	memreg.userspace_addr = (unsigned long)vm->mem;
	if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &memreg) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
		exit(1);
	}
}



void vcpu_init(struct kvm *vm, struct vcpu *vcpu)
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

int run_vm(struct kvm *vm, struct vcpu *vcpu, size_t sz)
{
	struct kvm_regs regs;
	u_int64_t memval = 0;
	bool ret;
	
	for (;;) {
		if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
			perror("KVM_RUN");
			exit(1);
		}

		switch (vcpu->kvm_run->exit_reason) {
		case KVM_EXIT_HLT:
			printf("\nKVM_EXIT_HLT(%d)\n", KVM_EXIT_HLT);
			kvm_show_regs(vcpu);
			goto check;

		case KVM_EXIT_IO:
			ret = kvm__emulate_io(vm,
					vcpu->kvm_run->io.port,
					(uint8_t*)vcpu->kvm_run + vcpu->kvm_run->io.data_offset,
					vcpu->kvm_run->io.direction,
					vcpu->kvm_run->io.size,
					vcpu->kvm_run->io.count);
			if (!ret)
				goto fail_exit;
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
	
fail_exit:
	kvm_show_regs(vcpu);
	return 1;
}

extern const unsigned char guest16_start[],guest16_end[];

#define BOOT_LOADER_SELECTOR	0x1000
#define BOOT_LOADER_IP		0x0000
#define BOOT_LOADER_SP		0x8000

int run_real_mode(struct kvm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;
	void *p;


	if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	sregs.cs.selector = vm->boot_selector; 

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
	regs.rip = vm->boot_ip;    
	regs.rax = 0;    /*for ax + bx to verify the result(1)*/
	regs.rbx = 1;
	regs.rsp = vm->boot_sp;
	regs.rbp = vm->boot_sp;

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}
	return run_vm(vm, vcpu, 2);
}

static int load_flat_binary(struct kvm *self, int fd)
{
	void *p;
	int nr;

	if (lseek(fd, 0, SEEK_SET) < 0) {
		perror("lseek");
		exit(1);
	}

	p = guest_real_to_host(self, BOOT_LOADER_SELECTOR, BOOT_LOADER_IP);

	while ((nr = read(fd, p, 65536)) > 0)
		p += nr;

	self->boot_selector = BOOT_LOADER_SELECTOR;
	self->boot_ip = BOOT_LOADER_IP;

	return true;
}

static void load_test_flat_binary(struct kvm *self)
{
	void *p;

	p = guest_real_to_host(self, BOOT_LOADER_SELECTOR, BOOT_LOADER_IP);
	memcpy(p, guest16_start, guest16_end-guest16_start);
	self->boot_selector = BOOT_LOADER_SELECTOR;
	self->boot_ip = BOOT_LOADER_IP;
	self->boot_sp = BOOT_LOADER_SP; 
}

bool kvm__load_kernel(struct kvm *kvm, const char *kernel_filename)
{
	bool ret;
	int fd;

	fd = open(kernel_filename, O_RDONLY);
	if (fd < 0)
		perror("unable to open kernel");

	ret = load_flat_binary(kvm, fd);
	if (ret)
		goto found_kernel;

	fprintf("%s is not a valid flat binary", kernel_filename);

found_kernel:
	return ret;
}

int main(int argc, char *argv[])
{
	struct kvm kvm;
	const char *kernel_filename = NULL;
	int i;

	for (i = 1; i < argc; i++) {
		if (!strncmp("--kernel=", argv[i], 9)) {
			kernel_filename = &argv[i][9];
			continue;
		}else if (!strncmp("--kernel= ", argv[i], 10)) {
			kernel_filename = &argv[i][10];
			continue;
		} else if (!strncmp("--test", argv[i], 6)) {
			kernel_filename = "--test";
			continue;
		} else {
			if (argv[i][0] != '-')
				kernel_filename = argv[i];
			else 
				fprintf(stderr, "unknown option:%s\n", argv[i]);
		}
	}

	if (!kernel_filename)
		usage(argv);

	vm_init(&kvm, 0x200000);

	vcpu_init(&kvm, &kvm.vcpu);

	early_printk__init();

	printf("kernel_filename=%s\n", kernel_filename);
	if (!strncmp(kernel_filename, "--test", 6)) {
		printf("start to test flat binary\n");
		load_test_flat_binary(&kvm);
	}
		
	else
		kvm__load_kernel(&kvm, kernel_filename);


	run_real_mode(&kvm, &kvm.vcpu);

	return 0;
}






