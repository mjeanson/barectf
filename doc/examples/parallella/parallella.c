#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <e_lib.h>

#include "barectf.h"
#include "barectf-platform-parallella.h"

#define WAND_BIT	(1 << 3)

static void __attribute__((interrupt)) wand_trace_isr(int signum)
{
	(void) signum;
}

static void sync(void)
{
	uint32_t irq_state;

	/* enable WAND interrupt */
	e_irq_global_mask(E_FALSE);
	e_irq_attach(WAND_BIT, wand_trace_isr);
	e_irq_mask(WAND_BIT, E_FALSE);

	/* WAND + IDLE */
	__asm__ __volatile__("wand");
	__asm__ __volatile__("idle");

	/* acknowledge interrupt */
	irq_state = e_reg_read(E_REG_STATUS);
	irq_state &= ~WAND_BIT;
	e_reg_write(E_REG_STATUS, irq_state);
}

int main(void)
{
	struct barectf_default_ctx *barectf_ctx;
	static const char *strings[] = {
		"calories",
		"fat",
		"carbohydrate",
		"protein",
	};
	uint8_t at = 0;

	/* initialize tracing platform */
	if (tracing_init()) {
		/* init. error: do not trace */
		return 1;
	}

	barectf_ctx = tracing_get_barectf_ctx();

	/* synchronize all cores */
	sync();

	/* reset tracing clock value */
	tracing_reset_clock();

	/* trace */
	for (;;) {
		int8_t b = (int8_t) at;
		size_t wait_count;

		barectf_default_trace_bit_packed_integers(barectf_ctx,
			at, -b, at * 2, -b * 2, at * 3, -b * 3);

		for (wait_count = 0; wait_count < 1000; ++wait_count) {
			__asm__ __volatile__("nop");
		}

		barectf_default_trace_string_and_float(barectf_ctx,
			strings[at & 3], 0.1234 * (float) at);
		at++;

#ifdef LOW_THROUGHPUT
		for (wait_count = 0; wait_count < 25000000; ++wait_count) {
			__asm__ __volatile__("nop");
		}
#endif /* LOW_THROUGHPUT */
	}

	/* never executed here, but this is where this would normally go */
	tracing_fini();

	return 0;
}
