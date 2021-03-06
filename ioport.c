#include <linux/kvm.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "ioport.h"
#include "kvm.h"

static uint8_t ioport_to_uint8(void *data)
{
	uint8_t *p = data;

	return *p;
}



static bool dummy_io_in(struct kvm *self, uint16_t port, void *data, int size, uint32_t count)
{
	return true;
}

static bool dummy_io_out(struct kvm *self, uint16_t port, void *data, int size, uint32_t count)
{
	return true;
}

static struct ioport_operations dummy_read_write_ioport_ops = {
	.io_in		= dummy_io_in,
	.io_out		= dummy_io_out,
};

static struct ioport_operations dummy_write_only_ioport_ops = {
	.io_out		= dummy_io_out,
};


static struct ioport_operations *ioport_ops[USHRT_MAX] = {
	/* PORT 0070-007F - CMOS RAM/RTC (REAL TIME CLOCK) */
	[0x71]		= &dummy_read_write_ioport_ops,

	/* 0x0020 - 0x003F - 8259A PIC 1 */
	[0x20]		= &dummy_read_write_ioport_ops,
	[0x21]		= &dummy_read_write_ioport_ops,

	/* PORT 0040-005F - PIT - PROGRAMMABLE INTERVAL TIMER (8253, 8254) */
	[0x0040]	= &dummy_read_write_ioport_ops,	/* Ch 0 */
	[0x0041]	= &dummy_read_write_ioport_ops,	/* Ch 1 */
	[0x0042]	= &dummy_read_write_ioport_ops,	/* Ch 2 */
	[0x0043]	= &dummy_read_write_ioport_ops,	/* Mod/Cmd */

	/* PORT 0060-006F - KEYBOARD CONTROLLER 804x (8041, 8042) (or PPI (8255) on PC,XT) */
	[0x0060]	= &dummy_read_write_ioport_ops,
	[0x0061]	= &dummy_read_write_ioport_ops,

	/* 0x00A0 - 0x00AF - 8259A PIC 2 */
	[0xA0]		= &dummy_read_write_ioport_ops,
	[0xA1]		= &dummy_read_write_ioport_ops,

	/* PORT 00ED - DUMMY PORT FOR DELAY??? */
	[0xED]		= &dummy_write_only_ioport_ops,

	/* 0x00F0 - 0x00FF - Math co-processor */
	[0xF0]		= &dummy_write_only_ioport_ops,
	[0xF1]		= &dummy_write_only_ioport_ops,

	/* PORT 03D4-03D5 - COLOR VIDEO - CRT CONTROL REGISTERS */
	[0x3D4]		= &dummy_read_write_ioport_ops,
	[0x3D5]		= &dummy_write_only_ioport_ops,

	/* PORT 03F8-03FF - Serial port (8250,8250A,8251,16450,16550,16550A,etc.) COM1 */
	[0x03F9]	= &dummy_read_write_ioport_ops,
	[0x03FA]	= &dummy_read_write_ioport_ops,
	[0x03FB]	= &dummy_read_write_ioport_ops,
	[0x03FC]	= &dummy_read_write_ioport_ops,

	/* PORT 0CF8-0CFF - PCI Configuration Mechanism 1 - Configuration Registers */
	[0x0CF8]	= &dummy_write_only_ioport_ops,
	[0x0CFC]	= &dummy_read_write_ioport_ops,
	[0x0CFE]	= &dummy_read_write_ioport_ops,
};

void ioport__register(uint16_t port, struct ioport_operations *ops)
{
	ioport_ops[port]	= ops;
}

static const char *to_direction(int direction)
{
	if (direction == KVM_EXIT_IO_IN)
		return "IN";
	else
		return "OUT";
}

static void ioport_error(uint16_t port, void *data, int direction, int size, uint32_t count)
{
	fprintf(stderr, "IO error: %s port=%x, size=%d, count=%d \n", to_direction(direction), port, size, count);
}

bool kvm__emulate_io(struct kvm *self, uint16_t port, void *data, int direction, int size, uint32_t count)
{
	struct ioport_operations *ops = ioport_ops[port];
	bool ret;

	if (!ops)
		goto error;

	if (direction == KVM_EXIT_IO_IN) {
		if (!ops->io_in)
			goto error;

		ret = ops->io_in(self, port, data, size, count);
		if (!ret)
			goto error;
	} else {
		if (!ops->io_out)
			goto error;

		ret = ops->io_out(self, port, data, size, count);
		if (!ret)
			goto error;
	}
	return true;
error:
	ioport_error(port, data, direction, size, count);
	return false;
}

