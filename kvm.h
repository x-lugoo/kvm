#ifndef _KVM_H_
#define _KVM_H_

struct vm {
	int sys_fd;
	int fd;
	char *mem;
};

struct vcpu {
	int fd;
	struct kvm_run *kvm_run;
};

extern void  kvm_show_regs(struct vcpu *cpu);

#endif
