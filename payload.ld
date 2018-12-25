SECTIONS
{
	.payload 0 : {
		guest16_start = .;
		guest16.o(.text)
		guest16_end = .;
	}
}

