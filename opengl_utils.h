/*
 *  Functions used by host & client sides
 *
 *  Copyright (c) 2007 Even Rouault
 *
 *  Modified by Ian Molton 2010
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#ifndef _OPENGL_UTILS
#define _OPENGL_UTILS

typedef struct
{
  unsigned int* values;
  int nbValues;
} RangeAllocator;

/*
extern void print_range(RangeAllocator* range);
*/

extern void alloc_value(RangeAllocator* range, unsigned int value);
extern unsigned int alloc_range(RangeAllocator* range, int n, unsigned int* values);
extern void delete_value(RangeAllocator* range, unsigned int value);
extern void delete_range(RangeAllocator* range, int n, const unsigned int* values);
extern void delete_consecutive_values(RangeAllocator* range, unsigned int first, int n);
extern unsigned int calc_checksum(const void* _ptr, int length, unsigned int seed);
extern void display_gl_call(FILE* f, int func_number, long* args, int* args_size);
extern void removeUnwantedExtensions(char* ret);

extern int compute_arg_length(int func_number, Signature *s, int arg_i, long* args);

#endif
