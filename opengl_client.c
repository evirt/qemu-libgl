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

/* gcc -Wall -g -O2 opengl_client.c -shared -o libGL.so.1 */

/* Windows compilations instructions */
/* After building qemu, cd to target-i386 */
/* i586-mingw32msvc-gcc -O2 -Wall opengl_client.c -c -I../i386-softmmu -I. -DBUILD_GL32 */
/* i586-mingw32msvc-dllwrap -o opengl32.dll --def opengl32.def -Wl,-enable-stdcall-fixup opengl_client.o -lws2_32
  */


/* objdump -T ../i386-softmmu/libGL.so | grep Base | awk '{print $7}' | grep gl | sort > opengl32_temp.def */


#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <termios.h>
#include <sys/io.h>

#include <dlfcn.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pthread.h>
#include <fcntl.h>

#include "opengl_func.h"
#include "common.h"

#include "opengl_utils.h"
#include "client_glx.h"
#include "client_gl.h"
#include "lock.h"
#include "log.h"

/* Locking globals... FIXMEIM find a better place */
pthread_mutex_t global_mutex         = PTHREAD_MUTEX_INITIALIZER;
pthread_key_t   key_current_gl_state;

pthread_t last_current_thread = 0;
GLState* _mono_threaded_current_gl_state = NULL;

/* The access to the following global variables shoud be done under the global lock */
int nbGLStates = 0;
GLState** glstates = NULL;
GLState* default_gl_state = NULL;



static const char* interestingEnvVars[] =
{
  "GET_IMG_FROM_SERVER",     /* default : not set */ /* unsupported for Win32 guest */
  "GL_SERVER",               /* default is localhost */
  "GL_SERVER_PORT",          /* default is 5555 */
  "GL_ERR_FILE",             /* default is stderr */
  "HACK_XGL",                /* default : not set */ /* unsupported for Win32 guest */
  "DEBUG_GL",                /* default : not set */
  "DEBUG_ARRAY_PTR",         /* default : not set */
  "DISABLE_OPTIM",           /* default : not set */
  "LIMIT_FPS",               /* default : not set */ /* unsupported for Win32 guest */
  "ENABLE_GL_BUFFERING",     /* default : set if ??? detected */
};

/**/

static int pagesize = 0;

int debug_gl = 0;
int debug_array_ptr = 0;
int disable_optim = 0;
int limit_fps = 0;

static char *glbuffer;
static int glfd;

static inline int call_opengl(int func_number, void* ret_string, void* args, void* args_size)
{
	volatile int *i = (volatile int*)glbuffer;
	i[0] = func_number;
	/* i[1] = pid; ...is filled in by the kernel module for virtio GL */
	i[2] = (int)ret_string;
	i[3] = (int)args;
	i[4] = (int)args_size;
	i[5] = (int)glbuffer;
	i[6] = 0;

	fsync(glfd); // Make magic happen

	if(i[6] == 0xdeadbeef) {
		/* Kernel decided to kill the process */
		fprintf(stderr, "Call failed: func %d   ret: %d\n", func_number, i[0]);
		*(int*)0 = 0;
		exit(1); /* Just in case */
	}
	
	return i[0];
}

static void do_init(void)
{
	glfd = open("/dev/vimem", O_RDWR | O_NOCTTY | O_SYNC | O_CLOEXEC);

	if(glfd == -1) {
		fprintf(stderr, "Failed to open device\n");
		exit(1);
	}

	glbuffer = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, glfd, 0);

	if(glbuffer == MAP_FAILED) {
		fprintf(stderr, "Failed to map buffer - dying horribly\n");
		exit(1);
	}
}

static int try_to_put_into_phys_memory(void* addr, int len)
{
  if (addr == NULL || len == 0) return 0;
  int i;
  void* aligned_addr = (void*)((long)addr & (~(pagesize - 1)));
  int to_end_page = (long)aligned_addr + pagesize - (long)addr;
  char c = 0;
  if (aligned_addr != addr)
  {
    c += ((char*)addr)[0];
    if (len <= to_end_page)
    {
      return c;
    }
    len -= to_end_page;
    addr = aligned_addr + pagesize;
  }
  for(i=0;i<len;i+=pagesize)
  {
    c += ((char*)addr)[0];
    addr += pagesize;
  }
  return c;
}

static int disable_warning_for_gl_read_pixels = 0;

/* Must only be called if the global lock has already been taken ! */
void do_opengl_call_no_lock(int func_number, void* ret_ptr, long* args, int* args_size_opt)
{
  if( ! (func_number >= 0 && func_number < GL_N_CALLS) )
  {
    log_gl("func_number >= 0 && func_number < GL_N_CALLS failed\n");
    goto end_do_opengl_call;
  }

  Signature* signature = (Signature*)tab_opengl_calls[func_number];
  int nb_args = signature->nb_args;
  int* args_type = signature->args_type;
  int args_size[100];
  int ret_int = 0;
  int i;

  static char* command_buffer = NULL;
  static int command_buffer_size = 0;
  static int last_command_buffer_size = -1;
  static char* ret_string = NULL;
  static int enable_gl_buffering = 0;
  static int last_current_thread = -1;
  static int exists_on_server_side[GL_N_CALLS];

  int current_thread = pthread_self();

  if (ret_string == NULL)
  {
    /* Sanity checks */
    assert(tab_args_type_length[TYPE_OUT_128UCHAR] == 128 * sizeof(char));
    assert(tab_args_type_length[TYPE_OUT_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS] == sizeof(double));

//FIXMEIM - breaks compilation    assert(sizeof(tab_args_type_length)/sizeof(tab_args_type_length[0]) == TYPE_LAST);

    memset(exists_on_server_side, 255, sizeof(exists_on_server_side));
    exists_on_server_side[glXGetProcAddress_fake_func] = 1;
    exists_on_server_side[glXGetProcAddress_global_fake_func] = 1;

    FILE* f = fopen("/tmp/.opengl_client", "rb");
    if (!f)
      f = fopen("opengl_client.txt", "rb");
    if (f)
    {
      char buffer[80];
      while (fgets(buffer, 80, f))
      {
        for(i=0;i<sizeof(interestingEnvVars)/sizeof(interestingEnvVars[0]);i++)
        {
          char tmp[256];
          strcpy(tmp, interestingEnvVars[i]);
          strcat(tmp, "=");
          if (strncmp(buffer, tmp, strlen(tmp)) == 0)
          {
            char* c= strdup(buffer+strlen(tmp));
            char* c2 = strchr(c, '\n');
            if (c2) *c2 = 0;
            setenv(interestingEnvVars[i], c,1);
            break;
          }
        }
      }
      fclose(f);
    }

    last_current_thread = current_thread;

    do_init();

    pagesize = getpagesize();
    debug_gl = getenv("DEBUG_GL") != NULL;
    debug_array_ptr = getenv("DEBUG_ARRAY_PTR") != NULL;
    disable_optim = getenv("DISABLE_OPTIM") != NULL;
    limit_fps = getenv("LIMIT_FPS") ? atoi(getenv("LIMIT_FPS")) : 0;

    posix_memalign((void**)&ret_string, pagesize, RET_STRING_SIZE);
    memset(ret_string, 0, RET_STRING_SIZE);
    mlock(ret_string, RET_STRING_SIZE);

    posix_memalign((void**)&command_buffer, pagesize, SIZE_BUFFER_COMMAND);
    mlock(command_buffer, SIZE_BUFFER_COMMAND);

    int init_ret = 0;
    init_ret = call_opengl(_init_func, NULL, NULL, NULL);
    if (init_ret == 0)
    {
      log_gl("You maybe forgot to launch QEMU with -enable-gl argument.\n");
      log_gl("exiting !\n");
      exit(-1);
    }
    enable_gl_buffering = (init_ret == 2) && (getenv("ENABLE_GL_BUFFERING") != NULL);
  }

  if (exists_on_server_side[func_number] == -1)
  {
    if (strchr(tab_opengl_calls_name[func_number], '_'))
    {
      exists_on_server_side[func_number] = 1;
    }
    else
    {
      exists_on_server_side[func_number] = glXGetProcAddress_no_lock((const GLubyte *)tab_opengl_calls_name[func_number]) != NULL;
    }
    if (exists_on_server_side[func_number] == 0)
    {
      log_gl("Symbol %s not available in server libGL. Shouldn't have reach that point...\n",
             tab_opengl_calls_name[func_number]);
      goto end_do_opengl_call;
    }
  }
  else if (exists_on_server_side[func_number] == 0)
  {
    goto end_do_opengl_call;
  }

  GET_CURRENT_STATE();

#ifdef ENABLE_THREAD_SAFETY
  if (last_current_thread != current_thread)
  {
    last_current_thread = current_thread;
    if (debug_gl) log_gl("gl thread switch\n");
    glXMakeCurrent_no_lock(state->display, state->current_drawable, state->context);
   }
#endif

  if (func_number == glFlush_func && getenv("HACK_XGL"))
  {
    glXSwapBuffers_no_lock(state->display, state->current_drawable);
    goto end_do_opengl_call;
  }
  else if ((func_number == glDrawBuffer_func || func_number == glReadBuffer_func) && getenv("HACK_XGL"))
  {
    goto end_do_opengl_call;
  }

  if ((func_number >= glRasterPos2d_func && func_number <= glRasterPos4sv_func) ||
      (func_number >= glWindowPos2d_func && func_number <= glWindowPos3sv_func) ||
      (func_number >= glWindowPos2dARB_func && func_number <= glWindowPos3svARB_func) ||
      (func_number >= glWindowPos2dMESA_func && func_number <= glWindowPos4svMESA_func))

  {
    state->currentRasterPosKnown = 0;
  }

  int this_func_parameter_size = sizeof(short); /* func_number */

  /* Compress consecutive glEnableClientState calls */
  if (!disable_optim &&
      last_command_buffer_size != -1 &&
      func_number == glClientActiveTexture_func &&
      *(short*)(command_buffer + last_command_buffer_size) == glClientActiveTexture_func)
  {
    *(int*)(command_buffer + last_command_buffer_size + sizeof(short)) = args[0];
    goto end_do_opengl_call;
  }

  for(i=0;i<nb_args;i++)
  {
    switch(args_type[i])
    {
      case TYPE_UNSIGNED_INT:
      case TYPE_INT:
      case TYPE_UNSIGNED_CHAR:
      case TYPE_CHAR:
      case TYPE_UNSIGNED_SHORT:
      case TYPE_SHORT:
      case TYPE_FLOAT:
      {
        this_func_parameter_size += sizeof(int);
        break;
      }

      CASE_IN_UNKNOWN_SIZE_POINTERS:
      {
        args_size[i] = args_size_opt[i];
        if (args_size[i] < 0)
        {
          log_gl("size < 0 : func=%s, param=%d\n", tab_opengl_calls_name[func_number], i);
          goto end_do_opengl_call;
        }
        try_to_put_into_phys_memory((void*)args[i], args_size[i]);
        this_func_parameter_size += sizeof(int) + args_size[i];
        break;
      }

      case TYPE_NULL_TERMINATED_STRING:
      {
        args_size[i] = strlen((const char*)args[i]) + 1;
        this_func_parameter_size += sizeof(int) + args_size[i];
        break;
      }

      CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
      CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
      {
        args_size[i] = compute_arg_length(get_err_file(), func_number, i, args);
        this_func_parameter_size += args_size[i];
        break;
      }

      case TYPE_IN_IGNORED_POINTER:
      {
        args_size[i] = 0;
        break;
      }

      CASE_OUT_UNKNOWN_SIZE_POINTERS:
      {
        args_size[i] = args_size_opt[i];
        if (args_size[i] < 0)
        {
          log_gl("size < 0 : func=%s, param=%d\n", tab_opengl_calls_name[func_number], i);
          goto end_do_opengl_call;
        }
        try_to_put_into_phys_memory((void*)args[i], args_size[i]);
        break;
      }

      case TYPE_DOUBLE:
      CASE_KNOWN_SIZE_POINTERS:
      {
        args_size[i] = tab_args_type_length[args_type[i]];
        try_to_put_into_phys_memory((void*)args[i], args_size[i]);
        this_func_parameter_size += args_size[i];
        break;
      }

      default:
      {
        log_gl("(1) unexpected arg type %d at i=%d\n", args_type[i], i);
        exit(-1);
        break;
      }
    }
  }

  if (debug_gl) display_gl_call(get_err_file(), func_number, args, args_size);

  if (enable_gl_buffering && signature->ret_type == TYPE_NONE && signature->has_out_parameters == 0 &&
      !(func_number == glXSwapBuffers_func || func_number == glFlush_func || func_number == glFinish_func))
  {
    assert(ret_ptr == NULL);
    if (this_func_parameter_size + command_buffer_size >= SIZE_BUFFER_COMMAND)
    {
      if (debug_gl)
        log_gl("flush pending opengl calls...\n");
      try_to_put_into_phys_memory(command_buffer, command_buffer_size);
      call_opengl(_serialized_calls_func, NULL, &command_buffer, &command_buffer_size);
      command_buffer_size = 0;
      last_command_buffer_size = -1;
    }

    if (this_func_parameter_size + command_buffer_size < SIZE_BUFFER_COMMAND)
    {
      /*if (debug_gl)
        log_gl("serializable opengl call.\n");*/
      last_command_buffer_size = command_buffer_size;
      *(short*)(command_buffer + command_buffer_size) = func_number;
      command_buffer_size += sizeof(short);

      for(i=0;i<nb_args;i++)
      {
        switch(args_type[i])
        {
          case TYPE_UNSIGNED_INT:
          case TYPE_INT:
          case TYPE_UNSIGNED_CHAR:
          case TYPE_CHAR:
          case TYPE_UNSIGNED_SHORT:
          case TYPE_SHORT:
          case TYPE_FLOAT:
          {
            *(int*)(command_buffer + command_buffer_size) = args[i];
            command_buffer_size += sizeof(int);
            break;
          }

          CASE_IN_UNKNOWN_SIZE_POINTERS:
          case TYPE_NULL_TERMINATED_STRING:
          {
            *(int*)(command_buffer + command_buffer_size) = args_size[i];
            command_buffer_size += sizeof(int);

            if (args_size[i])
            {
              memcpy(command_buffer + command_buffer_size, (void*)args[i], args_size[i]);
              command_buffer_size += args_size[i];
            }
            break;
          }

          CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
          {
            memcpy(command_buffer + command_buffer_size, (void*)args[i], args_size[i]);
            command_buffer_size += args_size[i];
            break;
          }

          case TYPE_IN_IGNORED_POINTER:
          {
            break;
          }

          case TYPE_DOUBLE:
          CASE_IN_KNOWN_SIZE_POINTERS:
          {
            memcpy(command_buffer + command_buffer_size, (void*)args[i], args_size[i]);
            command_buffer_size += args_size[i];
            break;
          }

          default:
          {
            log_gl("(2) unexpected arg type %d at i=%d\n", args_type[i], i);
            exit(-1);
            break;
          }
        }
      }
    }
    else
    {
      if (debug_gl)
        log_gl("too large opengl call.\n");
      call_opengl(func_number, NULL, args, args_size);
    }
  }
  else
  {
    if (command_buffer_size)
    {
      if (debug_gl)
        log_gl("flush pending opengl calls...\n");
      try_to_put_into_phys_memory(command_buffer, command_buffer_size);
      call_opengl(_serialized_calls_func, NULL, &command_buffer, &command_buffer_size);
      command_buffer_size = 0;
      last_command_buffer_size = -1;
    }

    if (!(func_number == glXSwapBuffers_func || func_number == glFlush_func  || func_number == glFinish_func || (func_number == glReadPixels_func && disable_warning_for_gl_read_pixels)) && enable_gl_buffering)
      log_gl("synchronous opengl call : %s.\n", tab_opengl_calls_name[func_number]);
    if (signature->ret_type == TYPE_CONST_CHAR)
    {
      try_to_put_into_phys_memory(ret_string, RET_STRING_SIZE);
    }

      if (getenv("GET_IMG_FROM_SERVER") != NULL && func_number == glXSwapBuffers_func)
      { // FIXMEIM ???
      }
      else
        ret_int = call_opengl(func_number, (signature->ret_type == TYPE_CONST_CHAR) ? ret_string : NULL, args, args_size);
    if (func_number == glXCreateContext_func)
    {
      if (debug_gl)
        log_gl("ret_int = %d\n", ret_int);
    }
    else if (func_number == glXSwapBuffers_func || func_number == glFinish_func || func_number == glFlush_func)
    {
      if (getenv("GET_IMG_FROM_SERVER"))
      {
        XWindowAttributes window_attributes_return;
        XGetWindowAttributes(state->display, state->current_drawable, &window_attributes_return);
        state->drawable_width = window_attributes_return.width;
        state->drawable_height = window_attributes_return.height;

        char* buf = malloc(4 * state->drawable_width * state->drawable_height);
        char* bufGL = malloc(4 * state->drawable_width * state->drawable_height);
        XImage* img = XCreateImage(state->display, 0, 24, ZPixmap, 0, buf, state->drawable_width, state->drawable_height, 8, 0);
        disable_warning_for_gl_read_pixels = 1;

        int pack_row_length, pack_alignment, pack_skip_rows, pack_skip_pixels;
        int pack_pbo = state->pixelPackBuffer;
        glGetIntegerv_no_lock(GL_PACK_ROW_LENGTH, &pack_row_length);
        glGetIntegerv_no_lock(GL_PACK_ALIGNMENT, &pack_alignment);
        glGetIntegerv_no_lock(GL_PACK_SKIP_ROWS, &pack_skip_rows);
        glGetIntegerv_no_lock(GL_PACK_SKIP_PIXELS, &pack_skip_pixels);
        glPixelStorei_no_lock(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei_no_lock(GL_PACK_ALIGNMENT, 4);
        glPixelStorei_no_lock(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei_no_lock(GL_PACK_SKIP_PIXELS, 0);
        if (pack_pbo)
          glBindBufferARB_no_lock(GL_PIXEL_PACK_BUFFER_EXT, 0);
        glReadPixels_no_lock(0, 0, state->drawable_width, state->drawable_height, GL_BGRA, GL_UNSIGNED_BYTE, bufGL);
        glPixelStorei_no_lock(GL_PACK_ROW_LENGTH, pack_row_length);
        glPixelStorei_no_lock(GL_PACK_ALIGNMENT, pack_alignment);
        glPixelStorei_no_lock(GL_PACK_SKIP_ROWS, pack_skip_rows);
        glPixelStorei_no_lock(GL_PACK_SKIP_PIXELS, pack_skip_pixels);
        if (pack_pbo)
          glBindBufferARB_no_lock(GL_PIXEL_PACK_BUFFER_EXT, pack_pbo);
        disable_warning_for_gl_read_pixels = 0;
        for(i=0;i<state->drawable_height;i++)
        {
          memcpy(buf + i * 4 * state->drawable_width,
                bufGL + (state->drawable_height-1-i) * 4 * state->drawable_width,
                4 * state->drawable_width);
        }
        free(bufGL);
        GC gc = DefaultGC(state->display, DefaultScreen(state->display));
        XPutImage(state->display, state->current_drawable, gc, img, 0, 0, 0, 0, state->drawable_width, state->drawable_height);
        XDestroyImage(img);
      }
      else
        call_opengl(_synchronize_func, NULL, NULL, NULL);
    }

    if (signature->ret_type == TYPE_UNSIGNED_INT ||
        signature->ret_type == TYPE_INT)
    {
      *(int*)ret_ptr = ret_int;
    }
    else if (signature->ret_type == TYPE_UNSIGNED_CHAR ||
            signature->ret_type == TYPE_CHAR)
    {
      *(char*)ret_ptr = ret_int;
    }
    else if (signature->ret_type == TYPE_CONST_CHAR)
    {
      *(char**)(ret_ptr) = ret_string;
    }
  }

end_do_opengl_call:
  (void)0;
}

