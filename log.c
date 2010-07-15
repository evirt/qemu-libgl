/*
 *  Guest-side implementation of GL/GLX API. Replacement of standard libGL.so
 *
 *  Copyright (c) 2006,2007 Even Rouault
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

FILE* get_err_file()
{
  static FILE* err_file = NULL;
  if (err_file == NULL)
  {
    err_file = getenv("GL_ERR_FILE") ? fopen(getenv("GL_ERR_FILE"), "wt") : NULL;
    if (err_file == NULL)
      err_file = stderr;
  }
  return err_file;
}


void log_gl(const char* format, ...)
{
  va_list list;
  va_start(list, format);
#ifdef ENABLE_THREAD_SAFETY
  if (IS_MT())
    fprintf(get_err_file(), "[thread %p] : ", (void*)GET_CURRENT_THREAD());
#endif
  vfprintf(get_err_file(), format, list);
  va_end(list);
}

