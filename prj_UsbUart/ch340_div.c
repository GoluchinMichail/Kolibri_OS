#include <stdbool.h>
#include "ch340.h"

//	2022.06.11
//		9600   -> B282
//		115200 -> CC83

static const speed_t ch341_min_rates[] = {
	CH341_MIN_RATE(0),
	CH341_MIN_RATE(1),
	CH341_MIN_RATE(2),
	CH341_MIN_RATE(3),
};

/*
 * The device line speed is given by the following equation:
 *
 *	baudrate = 48000000 / (2^(12 - 3 * ps - fact) * div), where
 *
 *		0 <= ps <= 3,
 *		0 <= fact <= 1,
 *		2 <= div <= 256 if fact = 0, or
 *		9 <= div <= 256 if fact = 1
 */
static int ch341_get_divisor(struct ch341_private *priv, speed_t speed)
{
	unsigned int fact, div, clk_div;
	bool force_fact0 = false;
	int ps;

	/*
	 * Clamp to supported range, this makes the (ps < 0) and (div < 2)
	 * sanity checks below redundant.
	 */
	speed = clamp_val(speed, CH341_MIN_BPS, CH341_MAX_BPS);

	/*
	 * Start with highest possible base clock (fact = 1) that will give a
	 * divisor strictly less than 512.
	 */
	fact = 1;
	for (ps = 3; ps >= 0; ps--) {
		if (speed > ch341_min_rates[ps])
			break;
	}

	if (ps < 0)
		return -EINVAL;

	/* Determine corresponding divisor, rounding down. */
	clk_div = CH341_CLK_DIV(ps, fact);
	div = CH341_CLKRATE / (clk_div * speed);

	/* Some devices require a lower base clock if ps < 3. */
	if (ps < 3 && (priv->quirks & CH341_QUIRK_LIMITED_PRESCALER))
		force_fact0 = true;

	/* Halve base clock (fact = 0) if required. */
	if (div < 9 || div > 255 || force_fact0) {
		div /= 2;
		clk_div *= 2;
		fact = 0;
	}

	if (div < 2)
		return -EINVAL;

	/*
	 * Pick next divisor if resulting rate is closer to the requested one,
	 * scale up to avoid rounding errors on low rates.
	 */
	if (16 * CH341_CLKRATE / (clk_div * div) - 16 * speed >=
			16 * speed - 16 * CH341_CLKRATE / (clk_div * (div + 1)))
		div++;

	/*
	 * Prefer lower base clock (fact = 0) if even divisor.
	 *
	 * Note that this makes the receiver more tolerant to errors.
	 */
	if (fact == 1 && div % 2 == 0) {
		div /= 2;
		fact = 0;
	}

	return (0x100 - div) << 8 | fact << 2 | ps;
}

int MY_ch341_get_divisor (int speed) {
	struct ch341_private Priv = {0};
	// всё равно ведь при трассировке - priv->quirks == 0
	// kernel ver. 5.15.19
	int Result = ch341_get_divisor (&Priv, speed);
	return Result | BIT(7);
}
