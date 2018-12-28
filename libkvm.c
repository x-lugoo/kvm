#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <linux/kvm.h>

#include "kvm.h"


static void print_dt(FILE *file, const char *name, struct kvm_dtable *dt)
{
    	fprintf(stderr, "%s %llx/%x\n", name, dt->base, dt->limit);
}


static void print_seg(FILE *file, const char *name, struct kvm_segment *seg)
{
    	fprintf(stderr,
		"%s %04x (%08llx/%08x p %d dpl %d db %d s %d type %x l %d"
		" g %d avl %d)\n",
		name, seg->selector, seg->base, seg->limit, seg->present,
		seg->dpl, seg->db, seg->s, seg->type, seg->l, seg->g,
		seg->avl);
}


void kvm_show_regs(struct vcpu *vcpu)
{
	int fd = vcpu->fd;
	struct kvm_regs regs;
	struct kvm_sregs sregs;
	int r;

	r = ioctl(fd, KVM_GET_REGS, &regs);
	if (r == -1) {
		perror("KVM_GET_REGS");
		return;
	}
	fprintf(stderr,
		"rax %016llx rbx %016llx rcx %016llx rdx %016llx\n"
		"rsi %016llx rdi %016llx rsp %016llx rbp %016llx\n"
		"r8  %016llx r9  %016llx r10 %016llx r11 %016llx\n"
		"r12 %016llx r13 %016llx r14 %016llx r15 %016llx\n"
		"rip %016llx rflags %08llx\n",
		regs.rax, regs.rbx, regs.rcx, regs.rdx,
		regs.rsi, regs.rdi, regs.rsp, regs.rbp,
		regs.r8,  regs.r9,  regs.r10, regs.r11,
		regs.r12, regs.r13, regs.r14, regs.r15,
		regs.rip, regs.rflags);
	r = ioctl(fd, KVM_GET_SREGS, &sregs);
	if (r == -1) {
		perror("KVM_GET_SREGS");
		return;
	}
	print_seg(stderr, "cs", &sregs.cs);
	print_seg(stderr, "ds", &sregs.ds);
	print_seg(stderr, "es", &sregs.es);
	print_seg(stderr, "ss", &sregs.ss);
	print_seg(stderr, "fs", &sregs.fs);
	print_seg(stderr, "gs", &sregs.gs);
	print_seg(stderr, "tr", &sregs.tr);
	print_seg(stderr, "ldt", &sregs.ldt);
	print_dt(stderr, "gdt", &sregs.gdt);
	print_dt(stderr, "idt", &sregs.idt);
	fprintf(stderr, "cr0 %llx cr2 %llx cr3 %llx cr4 %llx cr8 %llx"
		" efer %llx\n",
		sregs.cr0, sregs.cr2, sregs.cr3, sregs.cr4, sregs.cr8,
		sregs.efer);
}
