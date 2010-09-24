/*
 *  Guest-side implementation of GL/GLX API. Replacement of standard libGL.so
 *
 *  Copyright (c) 2006,2007 Even Rouault
 *  Copyright (c) 2010 Intel
 *    Parts written by Ian Molton <ian.molton@collabora.co.uk>
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

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/mman.h>
#include <fcntl.h>

#include "opengl_func.h"
#include "common.h"

#include "opengl_utils.h"
#include "client_glx.h"
#include "client_gl.h"
#include "lock.h"
#include "log.h"

//#define DEBUG_GLIO

#ifdef DEBUG_GLIO
#define SIZE_OUT_HEADER (4*4)
#define SIZE_IN_HEADER (4*2)
#else
#define SIZE_OUT_HEADER (4*3)
#define SIZE_IN_HEADER 4
#endif

/* Locking globals... FIXMEIM find a better place */
pthread_mutex_t global_mutex         = PTHREAD_MUTEX_INITIALIZER;
pthread_key_t   key_current_gl_state;

pthread_t last_current_thread = 0;
GLState* _mono_threaded_current_gl_state = NULL;

/* The access to the following global variables shoud be done under the global lock */
int nbGLStates = 0;
GLState** glstates = NULL;
GLState* default_gl_state = NULL;

static char *do_init(void);
static inline int get_args_buffer_size(int func_number, Signature *s, long *args, int *args_size_opt);
static inline int get_ret_buffer_size(int func_number, Signature *s, long *args, int *args_size_opt);
static inline int decode_ret_buffer(int func_number, Signature *s, long *args, char *buffer, void *ret_ptr);
static inline void buffer_args(int func_number, Signature *s, long *args, int *args_size_opt, char **args_buf);

static const char* interestingEnvVars[] =
{
    "GL_ERR_FILE",             /* default is stderr */
    "DEBUG_GL",                /* default : not set */
    "DEBUG_ARRAY_PTR",         /* default : not set */
    "DISABLE_OPTIM",           /* default : not set */
    "LIMIT_FPS",               /* default : not set */ /* unsupported for Win32 guest */
    "DISABLE_GL_BUFFERING",     /* default : set if ??? detected */
};

/**/

int debug_gl = 0;
int debug_array_ptr = 0;
int disable_optim = 0;

static int glfd;

static char* xfer_buffer = NULL;

static inline int call_opengl(char *buffer, int args_len, int ret_len, char *ret_buffer)
{
    volatile int *i = (volatile int*)xfer_buffer;
    char *ret_buf = ret_buffer?ret_buffer:buffer;
    int remain;

#ifdef DEBUG_GLIO
    int j, sum = 0;
    for(j = SIZE_OUT_HEADER; j < args_len ; j++)
    sum += buffer[j];
    i[3] = sum;
#endif

    /* i[0] = pid; ...is filled in by the kernel module for virtio GL */
    i[1] = args_len;
    i[2] = ret_len;

    if(buffer == xfer_buffer) {
        // All data fits within one buffer
        fsync(glfd);
    } else {
        // Data split across multiple in or out (or both) buffers.
        char *ptr = buffer+SIZE_OUT_HEADER;
        int len = args_len > SIZE_BUFFER_COMMAND-SIZE_OUT_HEADER ?
                             SIZE_BUFFER_COMMAND-SIZE_OUT_HEADER : args_len;

        while(args_len) {
            memcpy(xfer_buffer + SIZE_OUT_HEADER, ptr, len);

            fsync(glfd); // Make magic happen

            args_len -= len;
            ptr += len;
            len = args_len > SIZE_BUFFER_COMMAND-SIZE_OUT_HEADER ?
                             SIZE_BUFFER_COMMAND-SIZE_OUT_HEADER : args_len;

            i[1] = len + SIZE_OUT_HEADER;
            i[2] = 0;
        }

        ptr = ret_buf;
        remain = ret_len;
        while(remain) {
            int len = remain > SIZE_BUFFER_COMMAND ? SIZE_BUFFER_COMMAND
                                                   : remain;

            i[1] = 0;
            i[2] = len;

            fsync(glfd); // Make magic happen

            memcpy(ptr, xfer_buffer, len);
            remain -= len;
            ptr += len;
        }
    }

    if(!((int*)ret_buf)[0]) {
        /* Kernel decided to kill the process */
        *(int*)0 = 0;
        exit(1); /* Just in case */
    }

#ifdef DEBUG_GLIO
    sum = 0;
    for(j = SIZE_IN_HEADER; j < ret_len ; j++)
        sum += ret_buf[j];
    if(sum != *(int*)(ret_buf + 4))
        fprintf(stderr, "Read checksum error (was: %d should be: %d)\n", sum,
                        *(int*)(ret_buf + 4));
#endif

    return 0;
}

static inline char *map_buffer(void) {
    char *buffer;

    buffer = mmap(NULL, SIZE_BUFFER_COMMAND, PROT_READ | PROT_WRITE, MAP_SHARED, glfd, 0);

    if(buffer == MAP_FAILED) {
        fprintf(stderr, "Failed to map buffer - dying horribly\n");
        exit(1);
    }

    return buffer;
}

static int exists_on_server_side[GL_N_CALLS];
static char* cur_args_buffer = NULL;
static int enable_gl_buffering = 0;

/* Must only be called if the global lock has already been taken ! */
void do_opengl_call_no_lock(int func_number, void* ret_ptr, long* args, int* args_size_opt)
{
    Signature *signature;
    static char *command_buffer;
    char *ret_buf = NULL;
    int again, req_args_buffer, req_ret_buffer, req_total_buffer;
    int current_thread;
    int ret_int = 0;
    static int init, nr_serial, cur_ret_buf;

    if( ! (func_number >= 0 && func_number < GL_N_CALLS) )
    {
        log_gl("func_number >= 0 && func_number < GL_N_CALLS failed\n");
        return;
    }

    current_thread = pthread_self();
    signature = (Signature*)tab_opengl_calls[func_number];

    if (!init) {
        do_init();
        init = 1;
    }

    if (exists_on_server_side[func_number] == -1)
    {
        if (strchr(tab_opengl_calls_name[func_number], '_'))
        {
            exists_on_server_side[func_number] = 1;
        }
        else
        {
            exists_on_server_side[func_number] =
               glXGetProcAddress_no_lock(
                  (const GLubyte *)tab_opengl_calls_name[func_number]) != NULL;
        }
        if (exists_on_server_side[func_number] == 0)
        {
            log_gl("Symbol %s not available in server libGL. Shouldn't have reach that point...\n",
                   tab_opengl_calls_name[func_number]);
            return;
        }
    }
    else if (exists_on_server_side[func_number] == 0)
    {
        return;
    }

    GET_CURRENT_STATE();

#ifdef ENABLE_THREAD_SAFETY
    if (last_current_thread != current_thread)
    {
        fprintf(stderr, "MULTI-THREADED!----------------------\n");
        last_current_thread = current_thread;
        if (debug_gl)
            log_gl("gl thread switch\n");
        glXMakeCurrent_no_lock(state->display, state->current_drawable,
                               state->context);
    }
#endif

    if ((func_number >= glRasterPos2d_func &&
         func_number <= glRasterPos4sv_func) ||
        (func_number >= glWindowPos2d_func &&
         func_number <= glWindowPos3sv_func) ||
        (func_number >= glWindowPos2dARB_func &&
         func_number <= glWindowPos3svARB_func) ||
        (func_number >= glWindowPos2dMESA_func &&
         func_number <= glWindowPos4svMESA_func))
    {
        state->currentRasterPosKnown = 0;
    }

    /* Try to buffer the calls.
     * If there isnt enough room to buffer the call empty the buffer and retry.
     * If the call is > COMMAND_BUFFER_SIZE in length, pass it in a seperate
     * buffer. call_opengl() will detect this and split the call up for us.
     */

    again = 0;
    do {
        if(!command_buffer)
            command_buffer = xfer_buffer;

        if(!cur_args_buffer) {
            cur_args_buffer = command_buffer + SIZE_OUT_HEADER;
            nr_serial = 0;
        }

        /* Check buffer sizes */
        req_args_buffer = get_args_buffer_size(func_number, signature,
                                               args, args_size_opt);

        // Only the last command in the buffer can return data so no need to
        // increment the size of the required return buffer. Just set it if
        // necessary.

        req_ret_buffer = get_ret_buffer_size(func_number, signature,
                                             args, args_size_opt);

        if(((SIZE_BUFFER_COMMAND + command_buffer - cur_args_buffer) >=
            req_args_buffer) && req_ret_buffer <= SIZE_BUFFER_COMMAND)
        {   
            cur_ret_buf = req_ret_buffer;
            buffer_args(func_number, signature, args, args_size_opt, &cur_args_buffer);
            if(again > 0)
                again--;
            nr_serial++;
        }
        else {
            again++;
            if(again == 2) {
                // FIXME - this shouldnt be needed.
                req_total_buffer = MAX(req_args_buffer + SIZE_OUT_HEADER,
                                       req_ret_buffer);

                if(req_total_buffer > MAX_SIZE_BUFFER_COMMAND) {
                    fprintf(stderr, "Attempt to allocate buffer over %d bytes",
                                    MAX_SIZE_BUFFER_COMMAND);
                    *(int*)0 = 0;
                }

                // Allocate temporary large buffer
                command_buffer = malloc(req_total_buffer);
                ret_buf = malloc(req_ret_buffer);

                cur_args_buffer = command_buffer + SIZE_OUT_HEADER;
                cur_ret_buf = req_ret_buffer;
                buffer_args(func_number, signature, args, args_size_opt, &cur_args_buffer);
                nr_serial = 1;
           }
        }

        /* If call is not bufferable or the buffer is full. */
        if (nr_serial &&
            (again || !enable_gl_buffering || 
             !(signature->ret_type == TYPE_NONE &&
               signature->has_out_parameters == 0 &&
               !(func_number == glXSwapBuffers_func ||
                 func_number == glFlush_func ||
                 func_number == glFinish_func ||
                 func_number == _resize_surface_func ||
                 func_number == _render_surface_func))))
        {
            ret_int = call_opengl(command_buffer, cur_args_buffer - command_buffer, cur_ret_buf, ret_buf);
            decode_ret_buffer(func_number, signature, args, ret_buf?ret_buf:command_buffer, ret_ptr);

            cur_args_buffer = NULL; // Reset pointers.

            if(again == 2) {
                free(command_buffer);
                free(ret_buf);
                command_buffer = NULL;
                again = 0;
            }
        }
    } while (again);
}

#define GLINIT_FAIL_ABI 3
#define GLINIT_QUEUE 2
#define GLINIT_NOQUEUE 1

struct init_packet {
    short func_number;
    int version_major;
    int version_minor;
} __attribute__((__packed__));

static char *do_init(void)
{
    int current_thread = pthread_self();
    FILE *f;
    int i, init_ret;
    struct init_packet *pkt;

    /* Sanity checks */
    assert(tab_args_type_length[TYPE_OUT_128UCHAR] == 128 * sizeof(char));
    assert(tab_args_type_length[TYPE_OUT_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS] == sizeof(double));

    memset(exists_on_server_side, 255, sizeof(exists_on_server_side));
    exists_on_server_side[glXGetProcAddress_fake_func] = 1;
    exists_on_server_side[glXGetProcAddress_global_fake_func] = 1;

    f = fopen("/tmp/.opengl_client", "rb");

    if (!f)
        f = fopen("opengl_client.txt", "rb");

    if (f)
    {
        char buffer[80];
        while (fgets(buffer, 80, f))
        {
            for(i = 0 ;
                i < sizeof(interestingEnvVars) / sizeof(interestingEnvVars[0]) ;
                i++)
            {
                char tmp[256];
                strcpy(tmp, interestingEnvVars[i]);
                strcat(tmp, "=");
                if (strncmp(buffer, tmp, strlen(tmp)) == 0)
                {
                    char* c = strdup(buffer + strlen(tmp));
                    char* c2 = strchr(c, '\n');
                    if (c2)
                        *c2 = 0;
                    setenv(interestingEnvVars[i], c, 1);
                    break;
                }
            }
        }
        fclose(f);
    }

    last_current_thread = current_thread;

    debug_gl = getenv("DEBUG_GL") != NULL;
    debug_array_ptr = getenv("DEBUG_ARRAY_PTR") != NULL;
    disable_optim = getenv("DISABLE_OPTIM") != NULL;

    glfd = open("/dev/glmem", O_RDWR | O_NOCTTY | O_SYNC | O_CLOEXEC);

    if(glfd == -1) {
        fprintf(stderr, "Failed to open GL device\n");
        exit(1);
    }

    xfer_buffer = map_buffer();

    /* special case init function - nothings set up yet... */
    pkt = (struct init_packet*)(xfer_buffer + SIZE_OUT_HEADER);
    pkt->func_number = _init_func;
    pkt->version_major = 1;
    pkt->version_minor = 1;

    call_opengl(xfer_buffer,
                SIZE_OUT_HEADER + sizeof(*pkt),
                SIZE_IN_HEADER + sizeof(int), NULL);

    init_ret = *(int*)(xfer_buffer + SIZE_IN_HEADER);

    switch(init_ret)
    {
        case GLINIT_QUEUE:
        case GLINIT_NOQUEUE:
            enable_gl_buffering = (init_ret == GLINIT_QUEUE) &&
                                  !(getenv("DISABLE_GL_BUFFERING"));
            break;
        case GLINIT_FAIL_ABI:
            log_gl("Incompatible ABI version\n");
            log_gl("exiting !\n");
            exit(-1);
    }
    if(debug_gl)
        fprintf(stderr, "Enable buffering: %s (%d)\n", enable_gl_buffering?"yes":"no", init_ret);

    return NULL;
}

static inline void buffer_args(int func_number, Signature *s, long *args, int *args_size_opt, char **cur_args)
{
    int i;
    char *args_buf = *cur_args;
    int *args_size_buf;

    /* Store function number */
    *(short*)(args_buf) = func_number;
    args_buf += sizeof(short);


    for(i=0;i<s->nb_args;i++)
    {
        args_size_buf = (int*)args_buf;
        args_buf += sizeof(int);

        switch(s->args_type[i])
        {
            case TYPE_UNSIGNED_INT:
            case TYPE_INT:
            case TYPE_UNSIGNED_CHAR:
            case TYPE_CHAR:
            case TYPE_UNSIGNED_SHORT:
            case TYPE_SHORT:
            case TYPE_FLOAT:
                *args_size_buf = sizeof(int);
                memcpy(args_buf, &args[i], *args_size_buf);
                args_buf += *args_size_buf;
                args_size_buf++;
                break;

            CASE_IN_UNKNOWN_SIZE_POINTERS:
               	memcpy(args_buf, (void *)&args[i], sizeof(int));
                args_buf += sizeof(int);
                *args_size_buf = args_size_opt[i];
                if (*args_size_buf < 0)
                {
                    log_gl("size < 0 : func=%s, param=%d\n", tab_opengl_calls_name[func_number], i);
                    exit(1);
                }
                memcpy(args_buf, (void *)args[i], *args_size_buf);
                args_buf += *args_size_buf;
                args_size_buf++;
                break;

            case TYPE_NULL_TERMINATED_STRING:
                memcpy(args_buf, (void *)&args[i], sizeof(int));
                args_buf += sizeof(int);
                *args_size_buf = strlen((const char*)args[i]) + 1;
                memcpy(args_buf, (void *)args[i], *args_size_buf);
                args_buf += *args_size_buf;
                args_size_buf++;
                break;

            CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
                memcpy(args_buf, (void *)&args[i], sizeof(int));
                args_buf += sizeof(int);
                *args_size_buf = compute_arg_length(func_number, s, i, args);
                args_size_buf++;
                break;

            CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
                memcpy(args_buf, (void *)&args[i], sizeof(int));
                args_buf += sizeof(int);
                *args_size_buf = compute_arg_length(func_number, s, i, args);
                memcpy(args_buf, (void *)args[i], *args_size_buf);
                args_buf += *args_size_buf;
                args_size_buf++;
                break;

            case TYPE_IN_IGNORED_POINTER:
                *args_size_buf = 0;
                args_size_buf++;
                break;

            CASE_OUT_UNKNOWN_SIZE_POINTERS:
                memcpy(args_buf, (void *)&args[i], sizeof(int));
                args_buf += sizeof(int);
                *args_size_buf = args_size_opt[i];
                if (*args_size_buf < 0)
                {
                    log_gl("size < 0 : func=%s, param=%d\n", tab_opengl_calls_name[func_number], i);
                    exit(1);
                }
                args_size_buf++;
                break;

            CASE_OUT_KNOWN_SIZE_POINTERS:
                memcpy(args_buf, (void *)&args[i], sizeof(int));
                args_buf += sizeof(int);
                *args_size_buf = tab_args_type_length[s->args_type[i]];
                args_size_buf++;
                break;

            case TYPE_DOUBLE:
            CASE_IN_KNOWN_SIZE_POINTERS:
                memcpy(args_buf, (void *)&args[i], sizeof(int));
                args_buf += sizeof(int);
                *args_size_buf = tab_args_type_length[s->args_type[i]];
                memcpy(args_buf, (void *)args[i], *args_size_buf);
                args_buf += *args_size_buf;
                args_size_buf++;
                break;

            default:
                log_gl("(1) unexpected arg type %d at i=%d\n", s->args_type[i], i);
                exit(-1);
                break;
        }
    }
	
    *cur_args = args_buf;

}

static inline int get_args_buffer_size(int func_number, Signature *s,
                                       long *args, int *args_size_opt)
{
    int this_func_args_size = sizeof(short);
    int i;

    for(i=0;i<s->nb_args;i++)
    {
        this_func_args_size += sizeof(int);
        switch(s->args_type[i])
        {
            case TYPE_UNSIGNED_INT:
            case TYPE_INT:
            case TYPE_UNSIGNED_CHAR:
            case TYPE_CHAR:
            case TYPE_UNSIGNED_SHORT:
            case TYPE_SHORT:
            case TYPE_FLOAT:
                this_func_args_size += sizeof(int);
                break;

            CASE_IN_UNKNOWN_SIZE_POINTERS:
                this_func_args_size += sizeof(int);
                this_func_args_size += args_size_opt[i];
                break;

            case TYPE_NULL_TERMINATED_STRING:
                this_func_args_size += sizeof(int);
                this_func_args_size += strlen((const char*)args[i]) + 1;
                break;

            CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
                this_func_args_size += sizeof(int);
                break;

            CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
                this_func_args_size += sizeof(int);
                this_func_args_size += compute_arg_length(func_number, s, i, args);
                break;

            case TYPE_IN_IGNORED_POINTER:
                break;

            CASE_OUT_UNKNOWN_SIZE_POINTERS:
                this_func_args_size += sizeof(int);
                break;

            CASE_OUT_KNOWN_SIZE_POINTERS:
                this_func_args_size += sizeof(int);
                break;

            case TYPE_DOUBLE:
            CASE_IN_KNOWN_SIZE_POINTERS:
                this_func_args_size += sizeof(int);
                this_func_args_size += tab_args_type_length[s->args_type[i]];
                break;

            default:
                log_gl("(1) unexpected arg type %d at i=%d\n", s->args_type[i], i);
                exit(-1);
                break;
        }
    }

    return this_func_args_size;
}

int get_ret_buffer_size(int func_number, Signature *s, long *args,
                        int *args_size_opt)
{
    int this_func_ret_size = SIZE_IN_HEADER; // return status
    int i;

    for(i=0;i<s->nb_args;i++)
    {
        switch(s->args_type[i])
        {
            CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
                this_func_ret_size += sizeof(int);
                this_func_ret_size += compute_arg_length(func_number, s, i,
                                                         args);
                break;

            CASE_OUT_UNKNOWN_SIZE_POINTERS:
                this_func_ret_size += sizeof(int);
                this_func_ret_size += args_size_opt[i] + sizeof(int);
                break;

            CASE_OUT_KNOWN_SIZE_POINTERS:
                this_func_ret_size += sizeof(int);
                this_func_ret_size += tab_args_type_length[s->args_type[i]];
                break;
        }
    }

    switch(s->ret_type) {
        case TYPE_CONST_CHAR:
            this_func_ret_size += 32768;
            break;
        case TYPE_INT:
            this_func_ret_size += 4;
            break;
        case TYPE_CHAR:
            this_func_ret_size += 1;
            break;
    }

    return this_func_ret_size;
}

int decode_ret_buffer(int func_number, Signature *s, long *args, char *buffer, void *ret_ptr)
{
    char *cur_ptr = buffer;
    int i;
    int ret = *(int*)cur_ptr;

    cur_ptr += SIZE_IN_HEADER;

    for(i=0;i<s->nb_args;i++)
    {
        switch(s->args_type[i])
        {
            int len;
            CASE_OUT_POINTERS:
                len = *(int*)cur_ptr; cur_ptr += sizeof(int);
                if(args[i])
                    memcpy((char*)args[i], cur_ptr, len);
                cur_ptr += len;
                break;
        }
    }

    if(ret_ptr) {
        switch(s->ret_type) {
            case TYPE_CONST_CHAR:
                *(char**)ret_ptr = cur_ptr;
                break;
            case TYPE_INT:
                memcpy(ret_ptr, cur_ptr, 4);
                break;
            case TYPE_CHAR:
                *(char*)ret_ptr = *cur_ptr;
                break;
        }
    }
    return ret;
}
