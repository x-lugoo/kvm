	.code16
guest16:
	mov $0x3f8, %dx

    	add %bl, %al
    	add $'0', %al
    	out %al, (%dx)

    	mov $'J', %al
    	out %al, (%dx)

    	mov $'e', %al
    	out %al, (%dx)

    	mov $'f', %al
    	out %al, (%dx)

    	mov $'f', %al
    	out %al, (%dx)

	movw $99, %ax
	movw %ax, 0x502

	hlt

