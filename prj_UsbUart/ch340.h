#ifndef __CH340_H__
#define __CH340_H__

#define BIT(bi) (1<<bi)
#define CH341_QUIRK_LIMITED_PRESCALER	BIT(0)
#define EINVAL 123

typedef struct ch341_private {
	int quirks;
} _ch341_private;

#define speed_t unsigned

#define clamp_val(val, lo, hi)	val

#define CH341_CLKRATE		48000000
#define CH341_CLK_DIV(ps, fact)	(1 << (12 - 3 * (ps) - (fact)))
#define CH341_MIN_RATE(ps)	(CH341_CLKRATE / (CH341_CLK_DIV((ps), 1) * 512))

#define CH341_REQ_WRITE_REG    0x9A
#define CH341_REG_LCR          0x18
#define CH341_REG_LCR2         0x25

#define CH341_REG_PRESCALER    0x12
#define CH341_REG_DIVISOR      0x13

#define CH341_LCR_ENABLE_RX    0x80
#define CH341_LCR_ENABLE_TX    0x40
#define CH341_LCR_MARK_SPACE   0x20
#define CH341_LCR_PAR_EVEN     0x10
#define CH341_LCR_ENABLE_PAR   0x08
#define CH341_LCR_STOP_BITS_2  0x04
#define CH341_LCR_CS8          0x03
#define CH341_LCR_CS7          0x02
#define CH341_LCR_CS6          0x01
#define CH341_LCR_CS5          0x00

int MY_ch341_get_divisor (int speed);

#endif // #ifndef __CH340_H__ ...
