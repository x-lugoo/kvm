	.code16
guest16:
	movw $99, %ax
	movw %ax, 0x502
	hlt

