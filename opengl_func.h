/*
 *  Main header for both host and guest sides
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

#include "mesa_gl.h"
#include "enumtype.h"

#include <stdio.h>

#define CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS \
  case TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_ARRAY_FLOAT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS

#define CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS \
  case TYPE_OUT_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_OUT_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_OUT_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_OUT_ARRAY_FLOAT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_OUT_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS

#define CASE_IN_UNKNOWN_SIZE_POINTERS \
  case TYPE_ARRAY_CHAR: \
  case TYPE_ARRAY_SHORT: \
  case TYPE_ARRAY_INT: \
  case TYPE_ARRAY_FLOAT: \
  case TYPE_ARRAY_DOUBLE

#define CASE_IN_KNOWN_SIZE_POINTERS \
  case TYPE_1CHAR:\
  case TYPE_2CHAR:\
  case TYPE_3CHAR:\
  case TYPE_4CHAR:\
  case TYPE_128UCHAR:\
  case TYPE_1SHORT:\
  case TYPE_2SHORT:\
  case TYPE_3SHORT:\
  case TYPE_4SHORT:\
  case TYPE_1INT:\
  case TYPE_2INT:\
  case TYPE_3INT:\
  case TYPE_4INT:\
  case TYPE_1FLOAT:\
  case TYPE_2FLOAT:\
  case TYPE_3FLOAT:\
  case TYPE_4FLOAT:\
  case TYPE_16FLOAT:\
  case TYPE_1DOUBLE:\
  case TYPE_2DOUBLE:\
  case TYPE_3DOUBLE:\
  case TYPE_4DOUBLE:\
  case TYPE_16DOUBLE

#define CASE_OUT_UNKNOWN_SIZE_POINTERS \
  case TYPE_OUT_ARRAY_CHAR: \
  case TYPE_OUT_ARRAY_SHORT: \
  case TYPE_OUT_ARRAY_INT: \
  case TYPE_OUT_ARRAY_FLOAT: \
  case TYPE_OUT_ARRAY_DOUBLE

#define CASE_OUT_KNOWN_SIZE_POINTERS \
  case TYPE_OUT_1INT: \
  case TYPE_OUT_1FLOAT: \
  case TYPE_OUT_4CHAR: \
  case TYPE_OUT_4INT: \
  case TYPE_OUT_4FLOAT: \
  case TYPE_OUT_4DOUBLE: \
  case TYPE_OUT_128UCHAR \

#define CASE_IN_POINTERS CASE_IN_UNKNOWN_SIZE_POINTERS: CASE_IN_KNOWN_SIZE_POINTERS: CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS
#define CASE_OUT_POINTERS CASE_OUT_UNKNOWN_SIZE_POINTERS: CASE_OUT_KNOWN_SIZE_POINTERS: CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS

#define CASE_POINTERS CASE_IN_POINTERS: CASE_OUT_POINTERS
#define CASE_KNOWN_SIZE_POINTERS CASE_IN_KNOWN_SIZE_POINTERS: CASE_OUT_KNOWN_SIZE_POINTERS


#define IS_ARRAY_CHAR(type)  (type == TYPE_ARRAY_CHAR || type == TYPE_1CHAR || type == TYPE_2CHAR || type == TYPE_3CHAR || type == TYPE_4CHAR || type == TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS)
#define IS_ARRAY_SHORT(type)  (type == TYPE_ARRAY_SHORT || type == TYPE_1SHORT || type == TYPE_2SHORT || type == TYPE_3SHORT || type == TYPE_4SHORT || type == TYPE_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS)
#define IS_ARRAY_INT(type)  (type == TYPE_ARRAY_INT || type == TYPE_1INT || type == TYPE_2INT || type == TYPE_3INT || type == TYPE_4INT || type == TYPE_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS)
#define IS_ARRAY_FLOAT(type)  (type == TYPE_ARRAY_FLOAT || type == TYPE_1FLOAT || type == TYPE_2FLOAT || type == TYPE_3FLOAT || type == TYPE_4FLOAT || type == TYPE_16FLOAT || type == TYPE_ARRAY_FLOAT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS)
#define IS_ARRAY_DOUBLE(type)  (type == TYPE_ARRAY_DOUBLE || type == TYPE_1DOUBLE || type == TYPE_2DOUBLE || type == TYPE_3DOUBLE || type == TYPE_4DOUBLE || type == TYPE_16DOUBLE || type == TYPE_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS)

typedef struct
{
  int ret_type;
  int has_out_parameters;
  int nb_args;
  int args_type[0];
} Signature;

extern int tab_args_type_length[];

static const int _init32_signature[] = { TYPE_INT, 0, 0 };
static const int _init64_signature[] = { TYPE_INT, 0, 0 };

static const int _synchronize_signature[] = { TYPE_INT, 0, 0 };

static const int _serialized_calls_signature[] = { TYPE_NONE, 0, 1, TYPE_ARRAY_CHAR };

static const int _changeWindowState_signature[] = {TYPE_NONE, 0, 2, TYPE_INT, TYPE_INT};

static const int _moveResizeWindow_signature[] = {TYPE_NONE, 1, 4, TYPE_INT, TYPE_3INT, TYPE_INT, TYPE_OUT_ARRAY_CHAR};

static const int _send_cursor_signature[] = {TYPE_NONE, 0, 7, TYPE_INT, TYPE_INT,
                                                              TYPE_INT, TYPE_INT,
                                                              TYPE_INT, TYPE_INT,
                                                              TYPE_ARRAY_INT };

/* XVisualInfo* glXChooseVisual( Display *dpy, int screen, int *attribList ) */
static const int glXChooseVisual_signature[] = {TYPE_INT, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_ARRAY_INT };

/*GLXContext glXCreateContext( Display *dpy, XVisualInfo *vis,
                             GLXContext shareList, Bool direct )*/
static const int glXCreateContext_signature[] = {TYPE_INT, 0, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_INT};

static const int glXCopyContext_signature[] = {TYPE_NONE, 0, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_INT};

/* void glXDestroyContext( Display *dpy, GLXContext ctx ) */
static const int glXDestroyContext_signature[] = {TYPE_NONE, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT};

/* Bool glXMakeCurrent( Display *dpy, GLXDrawable drawable, GLXContext ctx) */
//static const int glXMakeCurrent_signature[] = {TYPE_INT, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT};
/* making it asynchronous */
static const int glXMakeCurrent_signature[] = {TYPE_NONE, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT};

/*int glXGetConfig( Display *dpy, XVisualInfo *visual,
                  int attrib, int *value )*/
static const int glXGetConfig_signature[] = {TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_OUT_1INT};

/* "glXGetConfig_extended"(dpy, visual_id, int n, int* attribs, int* values, int* rets) */
static const int glXGetConfig_extended_signature[] = {TYPE_NONE, 1, 6, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_ARRAY_INT, TYPE_OUT_ARRAY_INT, TYPE_OUT_ARRAY_INT};

/* void glXSwapBuffers( Display *dpy, GLXDrawable drawable ); */
static const int glXSwapBuffers_signature[] = {TYPE_NONE, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT};

/* Bool glXQueryVersion( Display *dpy, int *maj, int *min ) */
static const int glXQueryVersion_signature[] = {TYPE_INT, 1, 3, TYPE_IN_IGNORED_POINTER, TYPE_OUT_1INT, TYPE_OUT_1INT};

/* Bool glXQueryExtension( Display *dpy, int *errorBase, int *eventBase ) */
static const int glXQueryExtension_signature[] = {TYPE_INT, 1, 3, TYPE_IN_IGNORED_POINTER, TYPE_OUT_1INT, TYPE_OUT_1INT};

static const int glXWaitGL_signature[] = { TYPE_INT, 0, 0 };
static const int glXWaitX_signature[] = { TYPE_INT, 0, 0 };

/* GLX 1.1 and later */

/* const char *glXGetClientString( Display *dpy, int name ) */
static const int glXGetClientString_signature[] = {TYPE_CONST_CHAR, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT};

/*const char *glXQueryExtensionsString( Display *dpy, int screen ) */
static const int glXQueryExtensionsString_signature[] = {TYPE_CONST_CHAR, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT};

/* const char *glXQueryServerString( Display *dpy, int screen, int name ) */
static const int glXQueryServerString_signature[] = {TYPE_CONST_CHAR, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT};


static const int glXGetProcAddress_fake_signature[] = {TYPE_INT, 0, 1, TYPE_NULL_TERMINATED_STRING};

static const int glXGetProcAddress_global_fake_signature[] = {TYPE_NONE, 1, 3, TYPE_INT, TYPE_ARRAY_CHAR, TYPE_OUT_ARRAY_CHAR};


/* GLX 1.3 and later */

/*
GLXFBConfig *glXChooseFBConfig( Display *dpy, int screen,
                                       const int *attribList, int *nitems ); */
static const int glXChooseFBConfig_signature[] = {TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_ARRAY_INT, TYPE_OUT_1INT};

static const int glXChooseFBConfigSGIX_signature[] = {TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_ARRAY_INT, TYPE_OUT_1INT};

static const int glXGetFBConfigs_signature[] = {TYPE_INT, 1, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_OUT_1INT};

/* "glXGetFBConfigAttrib_extended"(dpy, fbconfig, int n, int* attribs, int* values, int* rets) */
static const int glXGetFBConfigAttrib_extended_signature[] = {TYPE_NONE, 1, 6, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_ARRAY_INT, TYPE_OUT_ARRAY_INT, TYPE_OUT_ARRAY_INT};


/* GLXPbuffer glXCreatePbuffer( Display *dpy, GLXFBConfig config,
                             const int *attribList ) */
static const int glXCreatePbuffer_signature[] = {TYPE_INT, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_ARRAY_INT};

static const int glXCreateGLXPbufferSGIX_signature[] = {TYPE_INT, 0, 5, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_INT};

static const int glXDestroyPbuffer_signature[] = {TYPE_NONE, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT};

static const int glXDestroyGLXPbufferSGIX_signature[] = {TYPE_NONE, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT};

/* GLXContext glXCreateNewContext(Display * dpy
                               GLXFBConfig  config
                               int  renderType
                               GLXContext  ShareList
                               Bool  Direct) */
static const int glXCreateNewContext_signature[] = {TYPE_INT, 0, 5, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};

static const int glXCreateContextWithConfigSGIX_signature[] = {TYPE_INT, 0, 5, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};

/*XVisualInfo *glXGetVisualFromFBConfig( Display *dpy, GLXFBConfig config ) */
static const int glXGetVisualFromFBConfig_signature[] = {TYPE_INT, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT};

/*int glXGetFBConfigAttrib(Display *dpy, GLXFBConfig  config, int attribute, int *value)*/
static const int glXGetFBConfigAttrib_signature[] = {TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_OUT_1INT};

static const int glXGetFBConfigAttribSGIX_signature[] = {TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_OUT_1INT};

static const int glXQueryContext_signature[] = {TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_OUT_1INT};

static const int glXQueryGLXPbufferSGIX_signature[] = {TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_OUT_1INT};

static const int glXQueryDrawable_signature[] = {TYPE_NONE, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_OUT_1INT};

/* void glXUseXFont( Font font, int first, int count, int list ) */
static const int glXUseXFont_signature[] = {TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};

/* Bool glXIsDirect( Display *dpy, GLXContext ctx ) */
static const int glXIsDirect_signature[] = {TYPE_CHAR, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

static const int glXGetScreenDriver_signature[] = { TYPE_CONST_CHAR, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

static const int glXGetDriverConfig_signature[] = { TYPE_CONST_CHAR, 0, 1, TYPE_NULL_TERMINATED_STRING };


static const int glXWaitVideoSyncSGI_signature[] = { TYPE_INT, 1, 3, TYPE_INT, TYPE_INT, TYPE_OUT_1INT };

static const int glXGetVideoSyncSGI_signature[] = { TYPE_INT, 1, 1, TYPE_OUT_1INT };

static const int glXSwapIntervalSGI_signature[] = { TYPE_INT, 0, 1, TYPE_INT };

static const int glXBindTexImageATI_signature[] = { TYPE_NONE, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT };
static const int glXReleaseTexImageATI_signature[] = { TYPE_NONE, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT };
static const int glXBindTexImageARB_signature[] = { TYPE_INT, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT };
static const int glXReleaseTexImageARB_signature[] = { TYPE_INT, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT };

/* const GLubyte * glGetString( GLenum name ) */
static const int glGetString_signature[] = {TYPE_CONST_CHAR, 0, 1, TYPE_INT};

/* void glShaderSourceARB (GLhandleARB handle , GLsizei size, const GLcharARB* *p_tab_prog, const GLint * tab_length) */
/* --> void glShaderSourceARB (GLhandleARB handle , GLsizei size, const GLcharARB* all_progs, const GLint * tab_length) */
static const int glShaderSourceARB_fake_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR, TYPE_ARRAY_INT};
static const int glShaderSource_fake_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR, TYPE_ARRAY_INT};

static const int glVertexPointer_fake_signature[] = { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glNormalPointer_fake_signature[] = { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glColorPointer_fake_signature[] = { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glSecondaryColorPointer_fake_signature[] = { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glIndexPointer_fake_signature[] = { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glTexCoordPointer_fake_signature[] = { TYPE_NONE, 0, 7, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glEdgeFlagPointer_fake_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexAttribPointerARB_fake_signature[] = { TYPE_NONE, 0, 8, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexAttribPointerNV_fake_signature[] = { TYPE_NONE, 0, 7, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glWeightPointerARB_fake_signature[] = { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glMatrixIndexPointerARB_fake_signature[] = { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glFogCoordPointer_fake_signature[] = { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glInterleavedArrays_fake_signature[] = { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glElementPointerATI_fake_signature[] = { TYPE_NONE, 0, 3, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVariantPointerEXT_fake_signature[] = { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glTuxRacerDrawElements_fake_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexAndNormalPointer_fake_signature[] = { TYPE_NONE, 0, 7, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glTexCoordPointer01_fake_signature[] = { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glTexCoordPointer012_fake_signature[] = { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalPointerInterlaced_fake_signature[] = {TYPE_NONE, 0, 8, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalColorPointerInterlaced_fake_signature[] = {TYPE_NONE, 0, 11, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexColorTexCoord0PointerInterlaced_fake_signature[] = {TYPE_NONE, 0, 12, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalTexCoord0PointerInterlaced_fake_signature[] = {TYPE_NONE, 0, 11, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalTexCoord01PointerInterlaced_fake_signature[] = {TYPE_NONE, 0, 14, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,  TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalTexCoord012PointerInterlaced_fake_signature[] = {TYPE_NONE, 0, 17, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,  TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalColorTexCoord0PointerInterlaced_fake_signature[] = {TYPE_NONE, 0, 14, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,  TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalColorTexCoord01PointerInterlaced_fake_signature[] = {TYPE_NONE, 0, 17, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,  TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalColorTexCoord012PointerInterlaced_fake_signature[] = {TYPE_NONE, 0, 20, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,  TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };

static const int glGenTextures_fake_signature[] = { TYPE_NONE, 0, 1, TYPE_INT};
static const int glGenBuffersARB_fake_signature[] = { TYPE_NONE, 0, 1, TYPE_INT};
static const int glGenLists_fake_signature[] = { TYPE_NONE, 0, 1, TYPE_INT};

static const int _glDrawElements_buffer_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glDrawRangeElements_buffer_signature[] = { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glMultiDrawElements_buffer_signature[] = { TYPE_NONE, 0, 5, TYPE_INT, TYPE_ARRAY_INT, TYPE_INT, TYPE_ARRAY_INT, TYPE_INT };

static const int _glVertexPointer_buffer_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glNormalPointer_buffer_signature[] = { TYPE_NONE, 0, 3, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glColorPointer_buffer_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glSecondaryColorPointer_buffer_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glIndexPointer_buffer_signature[] = { TYPE_NONE, 0, 3, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glTexCoordPointer_buffer_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glEdgeFlagPointer_buffer_signature[] = { TYPE_NONE, 0, 2, TYPE_INT, TYPE_INT};
static const int _glVertexAttribPointerARB_buffer_signature[] = { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glWeightPointerARB_buffer_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glMatrixIndexPointerARB_buffer_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glFogCoordPointer_buffer_signature[] = { TYPE_NONE, 0, 3, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glVariantPointerEXT_buffer_signature[] = { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};

static const int _glReadPixels_pbo_signature[] = { TYPE_INT, 0, 7, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glDrawPixels_pbo_signature[] = { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT};
static const int _glMapBufferARB_fake_signature[] = { TYPE_INT, 1, 3, TYPE_INT, TYPE_INT, TYPE_OUT_ARRAY_CHAR };

static const int _glSelectBuffer_fake_signature[] = { TYPE_NONE, 0, 1, TYPE_INT };
static const int _glGetSelectBuffer_fake_signature[] = { TYPE_NONE, 1, 1, TYPE_ARRAY_CHAR };
static const int _glFeedbackBuffer_fake_signature[] = { TYPE_NONE, 0, 2, TYPE_INT, TYPE_INT };
static const int _glGetFeedbackBuffer_fake_signature[] = { TYPE_NONE, 1, 1, TYPE_ARRAY_CHAR };

static const int _glGetError_fake_signature[] = { TYPE_NONE, 0, 0 };

#define timesynchro_func    -1
#define memorize_array_func -2
#define reuse_array_func    -3

#include "gl_func.h"

extern const int* tab_opengl_calls[GL_N_CALLS];
extern const char* tab_opengl_calls_name[GL_N_CALLS];

extern GLint __glTexParameter_size(FILE* err_file, GLenum pname);
extern int __glLight_size(FILE* err_file, GLenum pname);
extern int __glMaterial_size(FILE* err_file, GLenum pname);

#define IS_NULL_POINTER_OK_FOR_FUNC(func_number) \
                 (func_number == glBitmap_func || \
                  func_number == _send_cursor_func || \
                  func_number == glTexImage1D_func || \
                  func_number == glTexImage2D_func || \
                  func_number == glTexImage3D_func || \
                  func_number == glBufferDataARB_func || \
                  func_number == glNewObjectBufferATI_func)

#ifdef __amd64__
#define _init_func _init64_func
#else
#ifdef __i386__
#define _init_func _init32_func
#else
#error Unsupported ABI
#endif
#endif
