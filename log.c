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

