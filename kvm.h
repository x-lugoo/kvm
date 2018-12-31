#ifndef _KVM_H_
#define _KVM_H_

#include <stdint.h>
#include <stdbool.h>

struct kvm {
	int sys_fd;
	int fd;
	char *mem;
};

struct vcpu {
	int fd;
	struct kvm_run *kvm_run;
};
bool kvm__emulate_io(struct kvm *self, uint16_t port, void *data, int direction, int size, uint32_t count);

extern void  kvm_show_regs(struct vcpu *cpu);

#endif
