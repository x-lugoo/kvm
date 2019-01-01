	.code16
guest16:
	mov $0x3f8, %dx

    	add %bl, %al
    	add $'0', %al
    	out %al, (%dx)

test_ok:
	cs lea msg, %si
	mov $(msg_end-msg), %cx
	cs rep/outsb

	in (%dx), %al   #input 'G'
    	out %al, (%dx)

	in (%dx), %al   #input 'G'
    	out %al, (%dx)

	movw $99, %ax
	movw %ax, 0x1502
	hlt

msg:
	.asciz "\nJeff is testing kvm\n"
msg_end:
