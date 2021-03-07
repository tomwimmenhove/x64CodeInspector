void flags_or(uint64_t mask)
{
	asm(	"pushf\n"
		"or %0, 0x0(%%rsp)\n"
		"popf\n"
		: : "r" (mask)
		);
}

void flags_and(uint64_t mask)
{
	asm(	"pushf\n"
		"and %0, 0x0(%%rsp)\n"
		"popf\n"
		: : "r" (mask)
		);
}

void flags_set(uint64_t mask)
{
	asm(	"push %0\n"
		"popf\n"
		: : "r" (mask)
		);
}

uint64_t flags_get()
{
	uint64_t flags;

	asm(	"pushf\n"
		"mov 0x0(%%rsp), %0\n"
		"popf\n"
		: "=r" (flags) :
		);

	return flags;
}
