#ifndef __BIT_H__
#define __BIT_H__

#define setBit(byte,bit)	(byte) |= (1<<(bit))
#define clrBit(byte,bit)	(byte) &= ~(1<<(bit))
#define checkBit(byte,bit)	((byte) & (1<<(bit)))

#endif