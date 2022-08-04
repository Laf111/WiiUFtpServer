/****************************************************************************
 * Copyright (C) 2015 Dimok
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#ifndef __MEMORY_H_
#define __MEMORY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <malloc.h>

#define FlushBlock(addr)   asm volatile("dcbf %0, %1\n"                                \
                                        "icbi %0, %1\n"                                \
                                        "sync\n"                                       \
                                        "eieio\n"                                      \
                                        "isync\n"                                      \
                                        :                                              \
                                        :"r"(0), "r"(((addr) & ~31))                   \
                                        :"memory", "ctr", "lr", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"     \
                                        );

#define LIMIT(x, min, max)                                                                    \
    ({                                                                                        \
        typeof( x ) _x = x;                                                                    \
        typeof( min ) _min = min;                                                            \
        typeof( max ) _max = max;                                                            \
        ( ( ( _x ) < ( _min ) ) ? ( _min ) : ( ( _x ) > ( _max ) ) ? ( _max) : ( _x ) );    \
    })

#define DegToRad(a)   ( (a) *  0.01745329252f )
#define RadToDeg(a)   ( (a) * 57.29577951f )

#define ALIGN4(x)           (((x) + 3) & ~3)
#define ALIGN32(x)          (((x) + 31) & ~31)

// those work only in powers of 2
#define ROUNDDOWN(val, align)   ((val) & ~(align-1))
#define ROUNDUP(val, align)     ROUNDDOWN(((val) + (align-1)), align)

#define le16(i)         ((((u16) ((i) & 0xFF)) << 8) | ((u16) (((i) & 0xFF00) >> 8)))
#define le32(i)         ((((u32)le16((i) & 0xFFFF)) << 16) | ((u32)le16(((i) & 0xFFFF0000) >> 16)))
#define le64(i)         ((((u64)le32((i) & 0xFFFFFFFFLL)) << 32) | ((u64)le32(((i) & 0xFFFFFFFF00000000LL) >> 32)))

void memoryInitialize(void);
void memoryRelease(void);

void * MEM2_alloc(unsigned int size, unsigned int align);
void MEM2_free(void *ptr);

void * MEM1_alloc(unsigned int size, unsigned int align);
void MEM1_free(void *ptr);

void * MEMBucket_alloc(unsigned int size, unsigned int align);
void MEMBucket_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif // __MEMORY_H_
