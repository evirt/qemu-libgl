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
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h>
#include <sys/time.h>

#include "opengl_func.h"
#include "common.h"
#include "lock.h"
#include "call.h"
#include "log.h"


GLState* new_gl_state()
{
  int i, j;
  GLState* state;

  state = malloc(sizeof(GLState));
  memset(state, 0, sizeof(GLState));
  state->activeTexture = GL_TEXTURE0_ARB;
  state->client_state.pack.alignment = 4;
  state->client_state.unpack.alignment = 4;
  state->current_server_state.matrixMode = GL_MODELVIEW;
  state->current_server_state.depthFunc = GL_LESS;
  state->current_server_state.fog.mode = GL_EXP;
  state->current_server_state.fog.density = 1;
  state->current_server_state.fog.start = 0;
  state->current_server_state.fog.end = 1;
  state->current_server_state.fog.index = 0;
  state->textureAllocator = &state->ownTextureAllocator;
  state->bufferAllocator = &state->ownBufferAllocator;
  state->listAllocator = &state->ownListAllocator;
  state->currentRasterPosKnown = 1;
  state->currentRasterPos[0] = 0;
  state->currentRasterPos[1] = 0;
  state->currentRasterPos[2] = 0;
  state->currentRasterPos[3] = 1;
  for(i=0;i<NB_GL_MATRIX;i++)
  {
    state->matrix[i].sp = 0;
    for(j=0;j<16;j++)
    {
      state->matrix[i].current.val[j] = (j == 0 || j == 5 || j == 10 || j == 15);
    }
  }
  return state;
}


typedef struct
{
  int attrib;
  int value;
  int ret;
} _glXGetConfigAttribs;

typedef struct
{
  int val;
  char* name;
} glXAttrib;

#define VAL_AND_NAME(x)  { x, #x }

static const glXAttrib tabRequestedAttribsPair[] =
{
  VAL_AND_NAME(GLX_USE_GL),
  VAL_AND_NAME(GLX_BUFFER_SIZE),
  VAL_AND_NAME(GLX_LEVEL),
  VAL_AND_NAME(GLX_RGBA),
  VAL_AND_NAME(GLX_DOUBLEBUFFER),
  VAL_AND_NAME(GLX_STEREO),
  VAL_AND_NAME(GLX_AUX_BUFFERS),
  VAL_AND_NAME(GLX_RED_SIZE),
  VAL_AND_NAME(GLX_GREEN_SIZE),
  VAL_AND_NAME(GLX_BLUE_SIZE),
  VAL_AND_NAME(GLX_ALPHA_SIZE),
  VAL_AND_NAME(GLX_DEPTH_SIZE),
  VAL_AND_NAME(GLX_STENCIL_SIZE),
  VAL_AND_NAME(GLX_ACCUM_RED_SIZE),
  VAL_AND_NAME(GLX_ACCUM_GREEN_SIZE),
  VAL_AND_NAME(GLX_ACCUM_BLUE_SIZE),
  VAL_AND_NAME(GLX_ACCUM_ALPHA_SIZE),
  VAL_AND_NAME(GLX_CONFIG_CAVEAT),
  VAL_AND_NAME(GLX_X_VISUAL_TYPE),
  VAL_AND_NAME(GLX_TRANSPARENT_TYPE),
  VAL_AND_NAME(GLX_TRANSPARENT_INDEX_VALUE),
  VAL_AND_NAME(GLX_TRANSPARENT_RED_VALUE),
  VAL_AND_NAME(GLX_TRANSPARENT_GREEN_VALUE),
  VAL_AND_NAME(GLX_TRANSPARENT_BLUE_VALUE),
  VAL_AND_NAME(GLX_TRANSPARENT_ALPHA_VALUE),
  VAL_AND_NAME(GLX_SLOW_CONFIG),
  VAL_AND_NAME(GLX_TRUE_COLOR),
  VAL_AND_NAME(GLX_DIRECT_COLOR),
  VAL_AND_NAME(GLX_PSEUDO_COLOR),
  VAL_AND_NAME(GLX_STATIC_COLOR),
  VAL_AND_NAME(GLX_GRAY_SCALE),
  VAL_AND_NAME(GLX_STATIC_GRAY),
  VAL_AND_NAME(GLX_TRANSPARENT_RGB),
  VAL_AND_NAME(GLX_TRANSPARENT_INDEX),
  VAL_AND_NAME(GLX_VISUAL_ID),
  VAL_AND_NAME(GLX_DRAWABLE_TYPE),
  VAL_AND_NAME(GLX_RENDER_TYPE),
  VAL_AND_NAME(GLX_X_RENDERABLE),
  VAL_AND_NAME(GLX_FBCONFIG_ID),
  VAL_AND_NAME(GLX_RGBA_TYPE),
  VAL_AND_NAME(GLX_COLOR_INDEX_TYPE),
  VAL_AND_NAME(GLX_MAX_PBUFFER_WIDTH),
  VAL_AND_NAME(GLX_MAX_PBUFFER_HEIGHT),
  VAL_AND_NAME(GLX_MAX_PBUFFER_PIXELS),
  VAL_AND_NAME(GLX_PRESERVED_CONTENTS),
  VAL_AND_NAME(GLX_FLOAT_COMPONENTS_NV),
  VAL_AND_NAME(GLX_SAMPLE_BUFFERS),
  VAL_AND_NAME(GLX_SAMPLES)
};
#define N_REQUESTED_ATTRIBS (sizeof(tabRequestedAttribsPair)/sizeof(tabRequestedAttribsPair[0]))

static int* getTabRequestedAttribsInt()
{
  static int tabRequestedAttribsInt[N_REQUESTED_ATTRIBS] = { 0 };
  if (tabRequestedAttribsInt[0] == 0)
  {
    int i;
    for(i=0;i<N_REQUESTED_ATTRIBS;i++)
      tabRequestedAttribsInt[i] = tabRequestedAttribsPair[i].val;
  }
  return tabRequestedAttribsInt;
}

#define N_MAX_ATTRIBS N_REQUESTED_ATTRIBS+10
typedef struct
{
  int                  visualid;
  _glXGetConfigAttribs attribs[N_MAX_ATTRIBS];
  int                  nbAttribs;
} _glXConfigs;
#define N_MAX_CONFIGS 80
static _glXConfigs configs[N_MAX_CONFIGS];
static int nbConfigs = 0;

typedef struct
{
  GLXFBConfig config;
  _glXGetConfigAttribs attribs[N_MAX_ATTRIBS];
  int nbAttribs;
} _glXFBConfig;

static _glXFBConfig fbconfigs[N_MAX_CONFIGS];
static int nbFBConfigs = 0;

const char *glXQueryExtensionsString( Display *dpy, int screen )
{
  LOCK(glXQueryExtensionsString_func);
  static char* ret = NULL;
  if (ret == NULL)
  {
    long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen) };
    do_opengl_call_no_lock(glXQueryExtensionsString_func, &ret, args, NULL);
    ret = strdup(ret);
  }
  UNLOCK(glXQueryExtensionsString_func);
  return ret;
}


typedef struct
{
  XVisualInfo* vis;
  int visualid;
  GLXFBConfig fbconfig;
} AssocVisualInfoVisualId;

#define MAX_SIZE_TAB_ASSOC_VISUALINFO_VISUALID  100
AssocVisualInfoVisualId tabAssocVisualInfoVisualId[MAX_SIZE_TAB_ASSOC_VISUALINFO_VISUALID];
int nEltTabAssocVisualInfoVisualId = 0;

static const char* _getAttribNameFromValue(int val)
{
  int i;
  static char buffer[80];
  for(i=0;i<N_REQUESTED_ATTRIBS;i++)
  {
    if (tabRequestedAttribsPair[i].val == val)
      return tabRequestedAttribsPair[i].name;
  }
  sprintf(buffer, "(unknown name = %d, 0x%X)", val, val);
  return buffer;
}

static int _compute_length_of_attrib_list_including_zero(const int* attribList, int booleanMustHaveValue)
{
  int i = 0;
  debug_gl = getenv("DEBUG_GL") != NULL;
  if (debug_gl) log_gl("attribList = \n");
  while(attribList[i])
  {
    if (booleanMustHaveValue ||
        !(attribList[i] == GLX_USE_GL ||
          attribList[i] == GLX_RGBA ||
          attribList[i] == GLX_DOUBLEBUFFER ||
          attribList[i] == GLX_STEREO))
    {
      if (debug_gl) log_gl("%s = %d\n", _getAttribNameFromValue(attribList[i]), attribList[i+1]);
      i+=2;
    }
    else
    {
      if (debug_gl) log_gl("%s\n", _getAttribNameFromValue(attribList[i]));
      i++;
    }
  }
  if (debug_gl) log_gl("\n");
  return i + 1;
}


XVisualInfo* glXChooseVisual( Display *dpy, int screen,
                              int *attribList )
{
  XVisualInfo temp, *vis;
  long mask;
  int n;
  int i;

  int visualid = 0;
  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen), POINTER_TO_ARG(attribList) };
  int args_size[] = { 0, 0, sizeof(int) * _compute_length_of_attrib_list_including_zero(attribList, 0) };
  do_opengl_call(glXChooseVisual_func, &visualid, CHECK_ARGS(args, args_size));

  if (visualid)
  {
    mask = VisualScreenMask | VisualDepthMask | VisualClassMask | VisualIDMask;
    temp.screen = screen;
    temp.depth = DefaultDepth(dpy,screen);
    temp.class = DefaultVisual(dpy,screen)->class;
    temp.visualid = DefaultVisual(dpy,screen)->visualid;

    vis = XGetVisualInfo( dpy, mask, &temp, &n );
    if (vis == NULL)
      log_gl("cannot get visual from client side\n");

    assert (nEltTabAssocVisualInfoVisualId < MAX_SIZE_TAB_ASSOC_VISUALINFO_VISUALID);
    for(i=0;i<nEltTabAssocVisualInfoVisualId;i++)
    {
      if (tabAssocVisualInfoVisualId[i].vis == vis) break;
    }
    if (i == nEltTabAssocVisualInfoVisualId)
      nEltTabAssocVisualInfoVisualId++;
    tabAssocVisualInfoVisualId[i].vis = vis;
    tabAssocVisualInfoVisualId[i].fbconfig = 0;
    tabAssocVisualInfoVisualId[i].visualid = visualid;
  }
  else
  {
    vis = NULL;
  }

  if (debug_gl) log_gl("glXChooseVisual returning vis %p (visualid=%d, 0x%X)\n", vis, visualid, visualid);

  return vis;
}

const char *glXQueryServerString( Display *dpy, int screen, int name )
{
  LOCK(glXQueryServerString_func);
  static char* glXQueryServerString_ret[100] = {NULL};
  assert(name >= 0 && name < 100);
  if (glXQueryServerString_ret[name] == NULL)
  {
    long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen), INT_TO_ARG(name) };
    do_opengl_call_no_lock(glXQueryServerString_func, &glXQueryServerString_ret[name], args, NULL);
    glXQueryServerString_ret[name] = strdup(glXQueryServerString_ret[name]);
  }
  UNLOCK(glXQueryServerString_func);
  return glXQueryServerString_ret[name];
}

const char *glXGetClientString( Display *dpy, int name )
{
  LOCK(glXGetClientString_func);
  static char* glXGetClientString_ret[100] = {NULL};
  assert(name >= 0 && name < 100);
  if (glXGetClientString_ret[name] == NULL)
  {
    long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(name) };
    do_opengl_call_no_lock(glXGetClientString_func, &glXGetClientString_ret[name], args, NULL);
    if (getenv("GLX_VENDOR") && name == GLX_VENDOR)
    {
      glXGetClientString_ret[name] = getenv("GLX_VENDOR");
    }
    else
      glXGetClientString_ret[name] = strdup(glXGetClientString_ret[name]);
  }
  UNLOCK(glXGetClientString_func);
  return glXGetClientString_ret[name];
}

static void _create_context(GLXContext context, GLXContext shareList)
{
  int i;
  glstates = realloc(glstates, (nbGLStates+1) * sizeof(GLState*));
  glstates[nbGLStates] = new_gl_state();
  glstates[nbGLStates]->ref = 1;
  glstates[nbGLStates]->context = context;
  glstates[nbGLStates]->shareList = shareList;
  glstates[nbGLStates]->viewport.width = 0;
  if (shareList)
  {
    for(i=0; i<nbGLStates; i++)
    {
      if (glstates[i]->context == shareList)
      {
        glstates[i]->ref++;
        glstates[nbGLStates]->textureAllocator = glstates[i]->textureAllocator;
        glstates[nbGLStates]->bufferAllocator = glstates[i]->bufferAllocator;
        glstates[nbGLStates]->listAllocator = glstates[i]->listAllocator;
        break;
      }
    }
    if (i == nbGLStates)
    {
      log_gl("unknown shareList %p\n", (void*)shareList);
    }
  }
  nbGLStates++;
}

static GLXFBConfig* glXChooseFBConfig_no_lock( Display *dpy, int screen,
                                               const int *attribList, int *nitems );
static XVisualInfo* glXGetVisualFromFBConfig_no_lock( Display *dpy, GLXFBConfig config );

GLXContext glXCreateContext( Display *dpy, XVisualInfo *vis,
                             GLXContext shareList, Bool direct )
{
  LOCK(glXCreateContext_func);
  int isFbConfigVisual = 0;
  int i;
  int visualid = 0;

  for(i=0;i<nEltTabAssocVisualInfoVisualId;i++)
  {
    if (tabAssocVisualInfoVisualId[i].vis == vis)
    {
      if (tabAssocVisualInfoVisualId[i].fbconfig != NULL)
      {
        log_gl("isFbConfigVisual = 1\n");
        isFbConfigVisual = 1;
      }
      visualid = tabAssocVisualInfoVisualId[i].visualid;
      if (debug_gl) log_gl("found visualid %d corresponding to vis %p\n", visualid, vis);
      break;
    }
  }

  GLXContext ctxt = NULL;
  if (i == nEltTabAssocVisualInfoVisualId)
  {
    visualid = vis->visualid;
    if (debug_gl) log_gl("not found vis %p in table, visualid=%d\n", vis, visualid);
  }
  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(visualid), INT_TO_ARG(shareList), INT_TO_ARG(direct) };
  do_opengl_call_no_lock(glXCreateContext_func, &ctxt, args, NULL);

  if (ctxt)
  {
    _create_context(ctxt, shareList);
    glstates[nbGLStates-1]->isAssociatedToFBConfigVisual = isFbConfigVisual;
  }

  UNLOCK(glXCreateContext_func);
  return ctxt;
}

GLXContext glXGetCurrentContext (void)
{
  GET_CURRENT_STATE();
  if (debug_gl) log_gl("glXGetCurrentContext() -> %p\n", state->context);
  return state->context;
}

GLXDrawable glXGetCurrentDrawable (void)
{
  GET_CURRENT_STATE();
  if (debug_gl) log_gl("glXGetCurrentDrawable() -> %p\n", (void*)state->current_drawable);
  return state->current_drawable;
}

static void _free_context(Display* dpy, int i, GLState* state)
{
  free(state->last_cursor.pixels);
  free(state->ownTextureAllocator.values);
  free(state->ownBufferAllocator.values);
  free(state->ownListAllocator.values);
  free(state);
  memmove(&state, &glstates[i+1], (nbGLStates-i-1) * sizeof(GLState*));
  nbGLStates--;
}

GLAPI void APIENTRY glXDestroyContext( Display *dpy, GLXContext ctx )
{
  int i;
  LOCK(glXDestroyContext_func);
  GET_CURRENT_STATE();
  for(i=0;i<nbGLStates;i++)
  {
    if (glstates[i]->context == ctx)
    {
      long args[] = { POINTER_TO_ARG(dpy), POINTER_TO_ARG(ctx) };
      do_opengl_call_no_lock(glXDestroyContext_func, NULL, args, NULL);
      if (ctx == state->context)
      {
        SET_CURRENT_STATE(NULL);
      }

      GLXContext shareList = glstates[i]->shareList;

      glstates[i]->ref --;
      if (glstates[i]->ref == 0)
      {
        _free_context(dpy, i, glstates[i]);
      }

      if (shareList)
      {
        for(i=0; i<nbGLStates; i++)
        {
          if (glstates[i]->context == shareList)
          {
            glstates[i]->ref--;
            if (glstates[i]->ref == 0)
            {
              _free_context(dpy, i, glstates[i]);
            }
            break;
          }
        }
      }
      break;
    }
  }
  UNLOCK(glXDestroyContext_func);
}

Bool glXQueryVersion( Display *dpy, int *maj, int *min )
{
  LOCK(glXQueryVersion_func);
  static Bool ret = -1;
  static int l_maj, l_min;
  if (ret == -1)
  {
    long args[] = { POINTER_TO_ARG(dpy), POINTER_TO_ARG(&l_maj), POINTER_TO_ARG(&l_min) };
    do_opengl_call_no_lock(glXQueryVersion_func, &ret, args, NULL);
  }
  if (maj) *maj = l_maj;
  if (min) *min = l_min;
  UNLOCK(glXQueryVersion_func);
  return ret;
}


static void _get_window_info(Display *dpy, Window win, WindowInfoStruct* info)
{
  XWindowAttributes attr;

  XGetWindowAttributes(dpy, win, &attr);

  info->width     = attr.width;
  info->height    = attr.height;
  info->map_state = attr.map_state;
}

static RendererData *renderer_create_image(Display *dpy, int w, int h)
{
  RendererData *rdata = calloc(1, sizeof(*rdata));

  if(!rdata)
    goto out;

  rdata->w = w;
  rdata->h = h;

  rdata->image = XShmCreateImage(dpy, DefaultVisual(dpy, 0), 24, ZPixmap, NULL,
                                 &rdata->shminfo, w, h);
  
  if(!rdata->image)
    goto out_try_non_shm;

  rdata->shminfo.shmid = shmget(IPC_PRIVATE, rdata->image->bytes_per_line * h,
                                IPC_CREAT | 0777);
  if(rdata->shminfo.shmid == -1) {
    goto out_destroy_img;
  }

  rdata->shminfo.shmaddr = shmat(rdata->shminfo.shmid, NULL,0);
  if(rdata->shminfo.shmaddr == (void*)-1)
    goto out_shmput;

  rdata->buffer = rdata->shminfo.shmaddr;
  rdata->image->data = rdata->buffer;
  rdata->shminfo.readOnly = False;
  rdata->use_shm = 1;

  XShmAttach(dpy, &rdata->shminfo);
  XSync(dpy, 0);

  memset(rdata->buffer, 0, rdata->image->bytes_per_line * h);

  return rdata;

out_shmput:
  shmctl(rdata->shminfo.shmid, IPC_RMID, NULL);
out_destroy_img:
  XDestroyImage(rdata->image);
out_try_non_shm:
  // Fallback path
  rdata->image = XCreateImage(dpy, DefaultVisual(dpy, 0), 24, ZPixmap, 0, NULL, w, h, 32, 0);
  if(rdata->image) {
    rdata->buffer = calloc(1, (rdata->image->bytes_per_line) * h);
    if(rdata->buffer) {
      rdata->image->data = rdata->buffer;
      return rdata;
    }
  }
  XDestroyImage(rdata->image);
  free(rdata);
out:
  return NULL;
}

static void renderer_destroy_image(Display *dpy, RendererData *rdata) {
  if(!rdata) {
    return;
  }
  if(rdata->use_shm)
    XShmDetach(dpy, &rdata->shminfo);
  else
    free(rdata->image->data);

  rdata->image->data = NULL;
  XDestroyImage(rdata->image);

  if(rdata->use_shm) {
    shmdt(rdata->shminfo.shmaddr);
    shmctl(rdata->shminfo.shmid, IPC_RMID, NULL);
  }
  free(rdata);
}

#define MAX_PBUFFERS 100

/* Do this only with lock held */
static inline void _request_host_resize(Display *dpy, Window win, GLState *state, int w, int h)
{
    if(state->renderer_data)
      renderer_destroy_image(dpy, state->renderer_data);

    state->renderer_data = renderer_create_image(dpy, w, h);

    long args[] = { INT_TO_ARG(win), INT_TO_ARG(w), INT_TO_ARG(h)};
    do_opengl_call_no_lock(_resize_surface_func, NULL, args, 0);
}

static void _update_renderer(Display *dpy, Window win) {
  GET_CURRENT_STATE();
  WindowInfoStruct info;
  WindowInfoStruct *old_info = &state->last_win_state;

  if(!win)
    return;

  _get_window_info(dpy, win, &info);

  if(info.map_state != IsViewable) {
    if(state->renderer_data);
      renderer_destroy_image(dpy, state->renderer_data);
    state->renderer_data = NULL;
    goto out;
  }

  if(!state->renderer_data) {
    state->renderer_data = renderer_create_image(dpy, info.width, info.height);
    _request_host_resize(dpy, win, state, info.width, info.height);
    goto out;
  }

  if ((info.width != old_info->width) || (info.height != old_info->height)) {
    _request_host_resize(dpy, win, state, info.width, info.height);
    goto out;
  }

  //fprintf(stderr, "render: win: %08x w: %d h: %d stride: %d\n", win, state->renderer_data->w, state->renderer_data->h, state->renderer_data->image->bytes_per_line);

// Actually render stuff
  long args[] = { INT_TO_ARG(win), INT_TO_ARG(state->renderer_data->image->bits_per_pixel), INT_TO_ARG(state->renderer_data->image->bytes_per_line), POINTER_TO_ARG(state->renderer_data->buffer)};
  int args_size[] = {0, 0, 0, state->renderer_data->image->bytes_per_line*state->renderer_data->h};
  do_opengl_call_no_lock(_render_surface_func, NULL, args, args_size);

  /* draw into window */
  if(state->renderer_data->use_shm)
    XShmPutImage(dpy, win, DefaultGC(dpy, 0), state->renderer_data->image,
                 0, 0, 0, 0, state->renderer_data->w, state->renderer_data->h,
                 False);
  else 
    XPutImage(dpy, win, DefaultGC(dpy, 0), state->renderer_data->image, 0, 0,
              0, 0, state->renderer_data->w, state->renderer_data->h);

  XFlush(dpy);

#if 0
  do {
    char filename[50];
    FILE *f;

    sprintf(filename, "/home/ian/dump_%08x.rgb", win);
    f = fopen(filename, "wb");
    fwrite(state->renderer_data->buffer, state->renderer_data->image->bytes_per_line*state->renderer_data->h, 1, f);
    fclose(f);
  } while(0);
#endif

out:
  memcpy(old_info, &info, sizeof(info));
}

Bool glXMakeCurrent_no_lock( Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
  Bool ret = False;
  int i;
#if 0
  if (drawable == 0 && ctx == 0)
    return True;
  if (current_drawable == drawable && current_context == ctx) /* optimization */
    return True;
#endif
  GET_CURRENT_STATE();

  if (!(drawable < MAX_PBUFFERS)) /* FIXME */
  {
    for(i=0; i<nbGLStates; i++)
    {
      if (glstates[i]->context == ctx && glstates[i]->viewport.width == 0)
      {
        XWindowAttributes window_attributes_return;
        XGetWindowAttributes(dpy, drawable, &window_attributes_return);
        glstates[i]->viewport.width = window_attributes_return.width;
        glstates[i]->viewport.height = window_attributes_return.height;
        if (debug_gl)
        log_gl("drawable 0x%X dim : %d x %d\n", (int)drawable, window_attributes_return.width,
                window_attributes_return.height);
        break;
      }
    }
  }

  {
    //log_gl("glXMakeCurrent %d %d\n", drawable, ctx);
    long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(drawable), INT_TO_ARG(ctx) };
    do_opengl_call_no_lock(glXMakeCurrent_func, NULL /*&ret*/, args, NULL);
    ret = True;
    //_update_renderer(dpy, drawable);
  }

  if (ret)
  {
    int i;
    if (ctx == 0)
    {
      state->context = NULL;
    }
    else
    {
      for(i=0; i<nbGLStates; i++)
      {
        if (glstates[i]->context == ctx)
        {
          state = glstates[i];
          SET_CURRENT_STATE(state);
          break;
        }
      }
      if (i == nbGLStates)
      {
        log_gl("cannot set current_gl_state\n");
      }
    }

    state->display = dpy;
    state->context = ctx;
    state->current_drawable = drawable;
    state->current_read_drawable = drawable;
  }
  return ret;
}

GLAPI Bool APIENTRY glXMakeCurrent( Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
  Bool ret;
  LOCK(glXMakeCurrent_func);
  ret = glXMakeCurrent_no_lock(dpy, drawable, ctx);
  UNLOCK(glXMakeCurrent_func);
  return ret;
}

GLAPI void APIENTRY glXCopyContext( Display *dpy, GLXContext src, GLXContext dst, unsigned long mask )
{
  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(src), INT_TO_ARG(dst), INT_TO_ARG(mask) };
  do_opengl_call(glXCopyContext_func, NULL, args, NULL);
}

GLAPI Bool APIENTRY glXIsDirect( Display *dpy, GLXContext ctx )
{
  Bool ret = False;
  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(ctx) };
  do_opengl_call(glXIsDirect_func, &ret, args, NULL);
  return ret;
}

GLAPI int APIENTRY glXGetConfig( Display *dpy, XVisualInfo *vis,
                                 int attrib, int *value )
{
  int ret = 0;
  int i, j;
  if (vis == NULL || value == NULL) return 0;
  LOCK(glXGetConfig_func);

  int visualid = 0;
  for(i=0;i<nEltTabAssocVisualInfoVisualId;i++)
  {
    if (vis == tabAssocVisualInfoVisualId[i].vis)
    {
      visualid = tabAssocVisualInfoVisualId[i].visualid;
      if (debug_gl) log_gl("found visualid %d corresponding to vis %p\n", visualid, vis);
      break;
    }
  }
  if (i == nEltTabAssocVisualInfoVisualId)
  {
    if (debug_gl) log_gl("not found vis %p in table\n", vis);
    visualid = vis->visualid;
  }

   /* Optimization */
  for(i=0;i<nbConfigs;i++)
  {
    if (visualid == configs[i].visualid)
    {
      for(j=0;j<configs[i].nbAttribs;j++)
      {
        if (configs[i].attribs[j].attrib == attrib)
        {
          *value = configs[i].attribs[j].value;
          ret = configs[i].attribs[j].ret;
          if (debug_gl) log_gl("glXGetConfig(%s)=%d (%d)\n", _getAttribNameFromValue(attrib), *value, ret);
          goto end_of_glx_get_config;
        }
      }
      break;
    }
  }

  if (i < N_MAX_CONFIGS)
  {
    if (i == nbConfigs)
    {
      configs[i].visualid = visualid;
      configs[i].nbAttribs = 0;
      int tabGottenValues[N_REQUESTED_ATTRIBS];
      int tabGottenRes[N_REQUESTED_ATTRIBS];
      if (debug_gl) log_gl("glXGetConfig_extended visual=%p\n", vis);
      long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(visualid), INT_TO_ARG(N_REQUESTED_ATTRIBS),
                      POINTER_TO_ARG(getTabRequestedAttribsInt()), POINTER_TO_ARG(tabGottenValues),
                      POINTER_TO_ARG(tabGottenRes) };
      int args_size[] = {0, 0, 0, N_REQUESTED_ATTRIBS*sizeof(int), N_REQUESTED_ATTRIBS*sizeof(int),
                         N_REQUESTED_ATTRIBS*sizeof(int) };
      do_opengl_call_no_lock(glXGetConfig_extended_func, NULL, CHECK_ARGS(args, args_size));

      int j;
      int found = 0;
      int  jDblBuffer = -1, jUseGL = -1;
      for(j=0;j<N_REQUESTED_ATTRIBS;j++)
      {
        if (GLX_USE_GL == tabRequestedAttribsPair[j].val)
          jUseGL = j;
        else if (GLX_DOUBLEBUFFER == tabRequestedAttribsPair[j].val)
          jDblBuffer = j;
        configs[i].attribs[j].attrib = tabRequestedAttribsPair[j].val;
        configs[i].attribs[j].value = tabGottenValues[j];
        configs[i].attribs[j].ret = tabGottenRes[j];
        configs[i].nbAttribs++;
        if (tabRequestedAttribsPair[j].val == attrib)
        {
          found = 1;
          *value = configs[i].attribs[j].value;
          ret = configs[i].attribs[j].ret;
          if (debug_gl) log_gl("glXGetConfig(%s)=%d (%d)\n", tabRequestedAttribsPair[j].name, *value, ret);
        }
      }

      if (getenv("DISABLE_DOUBLE_BUFFER"))
      {
        if (configs[i].attribs[jDblBuffer].value == 1)
        {
          if (attrib == GLX_USE_GL)
            *value = 0;
          configs[i].attribs[jUseGL].value = 0;
        }
      }

      nbConfigs++;
      if (found)
        goto end_of_glx_get_config;
    }

    {
      long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(visualid), INT_TO_ARG(attrib), POINTER_TO_ARG(value) };
      do_opengl_call_no_lock(glXGetConfig_func, &ret, args, NULL);
      if (debug_gl) log_gl("glXGetConfig visual=%p, attrib=%d -> %d\n", vis, attrib, *value);
      if (configs[i].nbAttribs < N_MAX_ATTRIBS)
      {
        configs[i].attribs[configs[i].nbAttribs].attrib = attrib;
        configs[i].attribs[configs[i].nbAttribs].value = *value;
        configs[i].attribs[configs[i].nbAttribs].ret = ret;
        configs[i].nbAttribs++;
      }
    }
  }
  else
  {
    long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(visualid), INT_TO_ARG(attrib), POINTER_TO_ARG(value) };
    do_opengl_call_no_lock(glXGetConfig_func, &ret, args, NULL);
    if (debug_gl) log_gl("glXGetConfig visual=%p, attrib=%d -> %d\n", vis, attrib, *value);
  }
end_of_glx_get_config:
  UNLOCK(glXGetConfig_func);
  return ret;
}

void glXSwapBuffers_no_lock( Display *dpy, GLXDrawable drawable )
{
  //log_gl("glXSwapBuffers %d\n", drawable);
  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(drawable) };
  do_opengl_call_no_lock(glXSwapBuffers_func, NULL, args, NULL);

  _update_renderer(dpy, drawable);
}

GLAPI void APIENTRY glXSwapBuffers( Display *dpy, GLXDrawable drawable )
{
  LOCK(glXSwapBuffers_func);
  glXSwapBuffers_no_lock(dpy, drawable);
  UNLOCK(glXSwapBuffers_func);
}

GLAPI Bool APIENTRY glXQueryExtension( Display *dpy, int *errorBase, int *eventBase )
{
  Bool ret;
  LOCK(glXQueryExtension_func);
  int fake_int;
  if (errorBase == NULL) errorBase = &fake_int;
  if (eventBase == NULL) eventBase = &fake_int;
  long args[] = { POINTER_TO_ARG(dpy), POINTER_TO_ARG(errorBase), POINTER_TO_ARG(eventBase) };
  do_opengl_call_no_lock(glXQueryExtension_func, &ret, args, NULL);
  UNLOCK(glXQueryExtension_func);
  return ret;
}

GLAPI void APIENTRY glXWaitGL (void)
{
  int ret;
  do_opengl_call(glXWaitGL_func, &ret, NULL, NULL);
}

GLAPI void APIENTRY glXWaitX (void)
{
  int ret;
  do_opengl_call(glXWaitX_func, &ret, NULL, NULL);
}

GLAPI Display* APIENTRY glXGetCurrentDisplay( void )
{
  GET_CURRENT_STATE();
  return state->display;
}

static GLXFBConfig* glXChooseFBConfig_no_lock( Display *dpy, int screen,
                                const int *attribList, int *nitems )
{
  CHECK_PROC_WITH_RET(glXChooseFBConfig);
  GLXFBConfig* fbConfig = NULL;
  if (debug_gl) log_gl("glXChooseFBConfig\n");
  int i=0;
  int ret = 0;
  int emptyAttribList = None;
  if (attribList == NULL) attribList = &emptyAttribList;
  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen), POINTER_TO_ARG(attribList), POINTER_TO_ARG(nitems) };
  int args_size[] = { 0, 0, sizeof(int) * _compute_length_of_attrib_list_including_zero(attribList, 1), 0 };
  do_opengl_call_no_lock(glXChooseFBConfig_func, &ret, args, args_size);
  if (debug_gl) log_gl("nitems = %d\n", *nitems);
  if (*nitems == 0)
    return NULL;
  fbConfig = malloc(sizeof(GLXFBConfig) * (*nitems));
  for(i=0;i<*nitems;i++)
  {
    fbConfig[i] = (GLXFBConfig)(long)(ret + i);
    if (debug_gl && (i == 0 || i == *nitems-1)) log_gl("config %d = %d\n", i, ret+i);
  }
  return fbConfig;
}

GLAPI GLXFBConfig* APIENTRY glXChooseFBConfig( Display *dpy, int screen,
                                const int *attribList, int *nitems )
{
  GLXFBConfig* fbconfig;
  LOCK(glXChooseFBConfig_func);
  fbconfig = glXChooseFBConfig_no_lock(dpy, screen, attribList, nitems);
  UNLOCK(glXChooseFBConfig_func);
  return fbconfig;
}

GLAPI GLXFBConfig* APIENTRY glXGetFBConfigs( Display *dpy, int screen, int *nitems )
{
  CHECK_PROC_WITH_RET(glXGetFBConfigs);
  if (debug_gl) log_gl("glXGetFBConfigs\n");
  int i = 0;
  GLXFBConfig* fbConfig;
  int ret = 0;
  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen), POINTER_TO_ARG(nitems) };
  do_opengl_call(glXGetFBConfigs_func, &ret, args, NULL);
  if (debug_gl) log_gl("nitems = %d\n", *nitems);
  fbConfig = malloc(sizeof(GLXFBConfig) * (*nitems));
  for(i=0;i<*nitems;i++)
  {
    fbConfig[i] = (GLXFBConfig)(long)(ret + i);
    if (debug_gl && (i == 0 || i == *nitems-1)) log_gl("config %d = %d\n", i, ret+i);
  }
  return fbConfig;
}


GLAPI int APIENTRY glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config, int attrib, int *value)
{
  CHECK_PROC_WITH_RET(glXGetFBConfigAttrib);
  LOCK(glXGetFBConfigAttrib_func);
  int ret = 0;
  int i, j;

   /* Optimization */
  for(i=0;i<nbFBConfigs;i++)
  {
    if (config == fbconfigs[i].config)
    {
      for(j=0;j<fbconfigs[i].nbAttribs;j++)
      {
        if (fbconfigs[i].attribs[j].attrib == attrib)
        {
          *value = fbconfigs[i].attribs[j].value;
          ret = fbconfigs[i].attribs[j].ret;
          if (debug_gl)
          {
            log_gl("glXGetFBConfigAttrib(config=%p,%s)=%d (%d)\n", config,
                    _getAttribNameFromValue(attrib), *value, ret);
          }
          goto end_of_glx_get_fb_config_attrib;
        }
      }
      break;
    }
  }

  if (i < N_MAX_CONFIGS)
  {
    if (i == nbFBConfigs)
    {
      fbconfigs[i].config = config;
      fbconfigs[i].nbAttribs = 0;
      int tabGottenValues[N_REQUESTED_ATTRIBS];
      int tabGottenRes[N_REQUESTED_ATTRIBS];
      if (debug_gl) log_gl("glXGetFBConfigAttrib_extended config=%p\n", config);
      long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config), INT_TO_ARG(N_REQUESTED_ATTRIBS),
                      POINTER_TO_ARG(getTabRequestedAttribsInt()), POINTER_TO_ARG(tabGottenValues),
                      POINTER_TO_ARG(tabGottenRes) };
      int args_size[] = {0, 0, 0, N_REQUESTED_ATTRIBS*sizeof(int), N_REQUESTED_ATTRIBS*sizeof(int),
                         N_REQUESTED_ATTRIBS*sizeof(int) };
      do_opengl_call_no_lock(glXGetFBConfigAttrib_extended_func, NULL, CHECK_ARGS(args, args_size));

      int j;
      int found = 0;
      for(j=0;j<N_REQUESTED_ATTRIBS;j++)
      {
        fbconfigs[i].attribs[j].attrib = tabRequestedAttribsPair[j].val;
        fbconfigs[i].attribs[j].value = tabGottenValues[j];
        fbconfigs[i].attribs[j].ret = tabGottenRes[j];
        fbconfigs[i].nbAttribs++;
        if (tabRequestedAttribsPair[j].val == attrib)
        {
          found = 1;
          *value = fbconfigs[i].attribs[j].value;
          ret = fbconfigs[i].attribs[j].ret;
          if (debug_gl) log_gl("glXGetFBConfigAttrib(config=%p, %s)=%d (%d)\n",
              config, tabRequestedAttribsPair[j].name, *value, ret);
        }
      }
      nbFBConfigs++;
      if (found)
        goto end_of_glx_get_fb_config_attrib;
    }

    {
      long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config), INT_TO_ARG(attrib), POINTER_TO_ARG(value) };
      do_opengl_call_no_lock(glXGetFBConfigAttrib_func, &ret, args, NULL);
      if (debug_gl) log_gl("glXGetFBConfigAttrib config=%p, attrib=%d -> %d\n", config, attrib, *value);
      if (fbconfigs[i].nbAttribs < N_MAX_ATTRIBS)
      {
        fbconfigs[i].attribs[fbconfigs[i].nbAttribs].attrib = attrib;
        fbconfigs[i].attribs[fbconfigs[i].nbAttribs].value = *value;
        fbconfigs[i].attribs[fbconfigs[i].nbAttribs].ret = ret;
        fbconfigs[i].nbAttribs++;
      }
    }
  }
  else
  {
    long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config), INT_TO_ARG(attrib), POINTER_TO_ARG(value) };
    do_opengl_call_no_lock(glXGetFBConfigAttrib_func, &ret, args, NULL);
    if (debug_gl) log_gl("glXGetFBConfigAttrib config=%p, attrib=%d -> %d\n", config, attrib, *value);
  }
end_of_glx_get_fb_config_attrib:
  UNLOCK(glXGetFBConfigAttrib_func);
  return ret;
}

GLAPI int APIENTRY glXQueryContext( Display *dpy, GLXContext ctx, int attribute, int *value )
{
  CHECK_PROC_WITH_RET(glXQueryContext);
  int ret = 0;
  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(ctx), INT_TO_ARG(attribute), POINTER_TO_ARG(value) };
  do_opengl_call(glXQueryContext_func, &ret, args, NULL);
  return ret;
}

GLAPI void APIENTRY glXQueryDrawable( Display *dpy, GLXDrawable draw, int attribute, unsigned int *value )
{
  CHECK_PROC(glXQueryDrawable);
  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(draw), INT_TO_ARG(attribute), POINTER_TO_ARG(value) };
  do_opengl_call(glXQueryDrawable_func, NULL, args, NULL);
}

static XVisualInfo* glXGetVisualFromFBConfig_no_lock( Display *dpy, GLXFBConfig config )
{
  CHECK_PROC_WITH_RET(glXGetVisualFromFBConfig);
  int screen = 0;

  if (debug_gl) log_gl("glXGetVisualFromFBConfig %p\n", (void*)config);

  XVisualInfo temp, *vis;
  long mask;
  int n;
  int i;

  mask = VisualScreenMask | VisualDepthMask | VisualClassMask;
  temp.screen = screen;
  temp.depth = DefaultDepth(dpy,screen);
  temp.class = DefaultVisual(dpy,screen)->class;
  temp.visualid = DefaultVisual(dpy,screen)->visualid;
  mask |= VisualIDMask;

  vis = XGetVisualInfo( dpy, mask, &temp, &n );

  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(config)};
  int visualid;
  do_opengl_call_no_lock(glXGetVisualFromFBConfig_func, &visualid, args, NULL);

  vis->visualid = visualid;

  assert (nEltTabAssocVisualInfoVisualId < MAX_SIZE_TAB_ASSOC_VISUALINFO_VISUALID);
  for(i=0;i<nEltTabAssocVisualInfoVisualId;i++)
  {
    if (tabAssocVisualInfoVisualId[i].vis == vis) break;
  }
  if (i == nEltTabAssocVisualInfoVisualId)
    nEltTabAssocVisualInfoVisualId++;
  tabAssocVisualInfoVisualId[i].vis = vis;
  tabAssocVisualInfoVisualId[i].fbconfig = config;
  tabAssocVisualInfoVisualId[i].visualid = visualid;

  if (debug_gl) log_gl("glXGetVisualFromFBConfig returning vis %p (visualid=%d, 0x%X)\n", vis, visualid, visualid);

  return vis;
}

GLAPI XVisualInfo* APIENTRY glXGetVisualFromFBConfig( Display *dpy, GLXFBConfig config )
{
  XVisualInfo* vis;
  LOCK(glXGetVisualFromFBConfig_func);
  vis = glXGetVisualFromFBConfig_no_lock(dpy, config);
  UNLOCK(glXGetVisualFromFBConfig_func);
  return vis;
}

GLAPI GLXContext APIENTRY glXCreateNewContext(Display * dpy,
                               GLXFBConfig  fbconfig,
                               int  renderType,
                               GLXContext  shareList,
                               Bool  direct)
{
  CHECK_PROC_WITH_RET(glXCreateNewContext);
  LOCK(glXCreateNewContext_func);
  if (debug_gl) log_gl("glXCreateNewContext %p\n", (void*)fbconfig);

  GLXContext ctxt;
  long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(fbconfig), INT_TO_ARG(renderType), INT_TO_ARG(shareList),
                    INT_TO_ARG(direct) };
  do_opengl_call_no_lock(glXCreateNewContext_func, &ctxt, args, NULL);
  if (ctxt)
  {
    _create_context(ctxt, shareList);
  }
  UNLOCK(glXCreateNewContext_func);
  return ctxt;
}

GLAPI Bool APIENTRY glXMakeContextCurrent( Display *dpy, GLXDrawable draw,
                                           GLXDrawable read, GLXContext ctx )
{
  Bool ret;
  GET_CURRENT_STATE();
  if (draw != read)
  {
    static int first_time = 1;
    if (first_time)
    {
      first_time = 0;
      log_gl("using glXMakeCurrent instead of real glXMakeContextCurrent... may help some program work...\n");
    }
  }
  ret = glXMakeCurrent(dpy, draw, ctx);
  if (ret)
    state->current_read_drawable = read;
  return ret;
}

GLAPI GLXWindow APIENTRY glXCreateWindow( Display *dpy, GLXFBConfig config, Window win, const int *attribList )
{
  CHECK_PROC_WITH_RET(glXCreateWindow);
  /* do nothing. Not sure about this implementation. FIXME */
  fprintf(stderr, "createwindow?\n");
  return win;
}

GLAPI void APIENTRY glXDestroyWindow( Display *dpy, GLXWindow window )
{
  CHECK_PROC(glXDestroyWindow);
  /* do nothing. Not sure about this implementation. FIXME */
}

GLAPI GLXPixmap APIENTRY glXCreateGLXPixmap( Display *dpy,
                              XVisualInfo *vis,
                              Pixmap pixmap )
{
  CHECK_PROC_WITH_RET(glXCreateGLXPixmap);
  /* FIXME */
  log_gl("glXCreateGLXPixmap : sorry, unsupported call and I don't really see how I could implement it...");
  return 0;
}

GLAPI void APIENTRY glXDestroyGLXPixmap( Display *dpy, GLXPixmap pixmap )
{
  CHECK_PROC(glXDestroyGLXPixmap);
  /* FIXME */
  log_gl("glXDestroyGLXPixmap : sorry, unsupported call and I don't really see how I could implement it...");
}

GLAPI GLXPixmap APIENTRY glXCreatePixmap( Display *dpy, GLXFBConfig config,
                                  Pixmap pixmap, const int *attribList )
{
  CHECK_PROC_WITH_RET(glXCreatePixmap);
  /* FIXME */
  log_gl("glXCreatePixmap : sorry, unsupported call and I don't really see how I could implement it...");
  return 0;
}

GLAPI void APIENTRY glXDestroyPixmap( Display *dpy, GLXPixmap pixmap )
{
  CHECK_PROC(glXDestroyPixmap);
  /* FIXME */
  log_gl("glXDestroyPixmap : sorry, unsupported call and I don't really see how I could implement it...");
}

GLAPI GLXDrawable APIENTRY glXGetCurrentReadDrawable( void )
{
  CHECK_PROC_WITH_RET(glXGetCurrentReadDrawable);
  GET_CURRENT_STATE();
  return state->current_read_drawable;
}

GLAPI void APIENTRY glXSelectEvent( Display *dpy, GLXDrawable drawable,
                            unsigned long mask )
{
  CHECK_PROC(glXSelectEvent);
  log_gl("glXSelectEvent : sorry, unsupported call");
}

GLAPI void APIENTRY glXGetSelectedEvent( Display *dpy, GLXDrawable drawable,
                                 unsigned long *mask )
{
  CHECK_PROC(glXGetSelectedEvent);
  log_gl("glXGetSelectedEvent : sorry, unsupported call");
}


#include "opengl_client_xfonts.c"

GLAPI const char * APIENTRY EXT_FUNC(glXGetScreenDriver) (Display *dpy, int screen)
{
  static const char* ret = NULL;
  LOCK(glXGetScreenDriver_func);
  CHECK_PROC_WITH_RET(glXGetScreenDriver);
  if (ret == NULL)
  {
    long args[] = { POINTER_TO_ARG(dpy), INT_TO_ARG(screen) };
    do_opengl_call_no_lock(glXGetScreenDriver_func, &ret, args, NULL);
    ret = strdup(ret);
  }
  UNLOCK(glXGetScreenDriver_func);
  return ret;
}

GLAPI const char * APIENTRY EXT_FUNC(glXGetDriverConfig) (const char *drivername)
{
  static const char* ret = NULL;
  CHECK_PROC_WITH_RET(glXGetDriverConfig);
  long args[] = { POINTER_TO_ARG(drivername) };
  if (ret) free((void*)ret);
  do_opengl_call(glXGetDriverConfig_func, &ret, args, NULL);
  ret = strdup(ret);
  return ret;
}

/* For googleearth */
static int counterSync = 0;

GLAPI int APIENTRY EXT_FUNC(glXWaitVideoSyncSGI) ( int divisor, int remainder, unsigned int *count )
{
  CHECK_PROC_WITH_RET(glXWaitVideoSyncSGI);
  //log_gl("glXWaitVideoSyncSGI %d %d\n", divisor, remainder);
  *count = counterSync++; // FIXME ?
  return 0;
}

GLAPI int APIENTRY EXT_FUNC(glXGetVideoSyncSGI)( unsigned int *count )
{
  CHECK_PROC_WITH_RET(glXGetVideoSyncSGI);
  //log_gl("glXGetVideoSyncSGI\n");
  *count = counterSync++; // FIXME ?
  return 0;
}

GLAPI int APIENTRY EXT_FUNC(glXSwapIntervalSGI) ( int interval )
{
  CHECK_PROC_WITH_RET(glXSwapIntervalSGI);
  long args[] = { INT_TO_ARG(interval) };
  int ret = 0;
  do_opengl_call(glXSwapIntervalSGI_func, &ret, args, NULL);
  //log_gl("glXSwapIntervalSGI(%d) = %d\n", interval, ret);
  return ret;
}

static unsigned int str_hash (const void* v)
{
  /* 31 bit hash function */
  const signed char *p = v;
  unsigned int h = *p;

  if (h)
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + *p;

  return h;
}

static const char* global_glXGetProcAddress_request =
{
"glAccum\0"
"glActiveStencilFaceEXT\0"
"glActiveTexture\0"
"glActiveTextureARB\0"
"glActiveVaryingNV\0"
"glAddSwapHintRectWIN\0"
"glAlphaFragmentOp1ATI\0"
"glAlphaFragmentOp2ATI\0"
"glAlphaFragmentOp3ATI\0"
"glAlphaFunc\0"
"glApplyTextureEXT\0"
"glAreProgramsResidentNV\0"
"glAreTexturesResident\0"
"glAreTexturesResidentEXT\0"
"glArrayElement\0"
"glArrayElementEXT\0"
"glArrayObjectATI\0"
"glAsyncMarkerSGIX\0"
"glAttachObjectARB\0"
"glAttachShader\0"
"glBegin\0"
"glBeginConditionalRenderNVX\0"
"glBeginDefineVisibilityQueryATI\0"
"glBeginFragmentShaderATI\0"
"glBeginOcclusionQuery\0"
"glBeginOcclusionQueryNV\0"
"glBeginQuery\0"
"glBeginQueryARB\0"
"glBeginSceneEXT\0"
"glBeginTransformFeedbackNV\0"
"glBeginUseVisibilityQueryATI\0"
"glBeginVertexShaderEXT\0"
"glBindArraySetARB\0"
"glBindAttribLocation\0"
"glBindAttribLocationARB\0"
"glBindBuffer\0"
"glBindBufferARB\0"
"glBindBufferBaseNV\0"
"glBindBufferOffsetNV\0"
"glBindBufferRangeNV\0"
"glBindFragDataLocationEXT\0"
"glBindFragmentShaderATI\0"
"glBindFramebufferEXT\0"
"glBindLightParameterEXT\0"
"glBindMaterialParameterEXT\0"
"glBindParameterEXT\0"
"glBindProgramARB\0"
"glBindProgramNV\0"
"glBindRenderbufferEXT\0"
"glBindTexGenParameterEXT\0"
"glBindTexture\0"
"glBindTextureEXT\0"
"glBindTextureUnitParameterEXT\0"
"glBindVertexArrayAPPLE\0"
"glBindVertexShaderEXT\0"
"glBinormal3bEXT\0"
"glBinormal3bvEXT\0"
"glBinormal3dEXT\0"
"glBinormal3dvEXT\0"
"glBinormal3fEXT\0"
"glBinormal3fvEXT\0"
"glBinormal3iEXT\0"
"glBinormal3ivEXT\0"
"glBinormal3sEXT\0"
"glBinormal3svEXT\0"
"glBinormalArrayEXT\0"
"glBitmap\0"
"glBlendColor\0"
"glBlendColorEXT\0"
"glBlendEquation\0"
"glBlendEquationEXT\0"
"glBlendEquationSeparate\0"
"glBlendEquationSeparateATI\0"
"glBlendEquationSeparateEXT\0"
"glBlendFunc\0"
"glBlendFuncSeparate\0"
"glBlendFuncSeparateEXT\0"
"glBlendFuncSeparateINGR\0"
"glBlitFramebufferEXT\0"
"glBufferData\0"
"glBufferDataARB\0"
"glBufferParameteriAPPLE\0"
"glBufferRegionEnabled\0"
"glBufferRegionEnabledEXT\0"
"glBufferSubData\0"
"glBufferSubDataARB\0"
"glCallList\0"
"glCallLists\0"
"glCheckFramebufferStatusEXT\0"
"glClampColorARB\0"
"glClear\0"
"glClearAccum\0"
"glClearColor\0"
"glClearColorIiEXT\0"
"glClearColorIuiEXT\0"
"glClearDepth\0"
"glClearDepthdNV\0"
"glClearIndex\0"
"glClearStencil\0"
"glClientActiveTexture\0"
"glClientActiveTextureARB\0"
"glClientActiveVertexStreamATI\0"
"glClipPlane\0"
"glColor3b\0"
"glColor3bv\0"
"glColor3d\0"
"glColor3dv\0"
"glColor3f\0"
"glColor3fv\0"
"glColor3fVertex3fSUN\0"
"glColor3fVertex3fvSUN\0"
"glColor3hNV\0"
"glColor3hvNV\0"
"glColor3i\0"
"glColor3iv\0"
"glColor3s\0"
"glColor3sv\0"
"glColor3ub\0"
"glColor3ubv\0"
"glColor3ui\0"
"glColor3uiv\0"
"glColor3us\0"
"glColor3usv\0"
"glColor4b\0"
"glColor4bv\0"
"glColor4d\0"
"glColor4dv\0"
"glColor4f\0"
"glColor4fNormal3fVertex3fSUN\0"
"glColor4fNormal3fVertex3fvSUN\0"
"glColor4fv\0"
"glColor4hNV\0"
"glColor4hvNV\0"
"glColor4i\0"
"glColor4iv\0"
"glColor4s\0"
"glColor4sv\0"
"glColor4ub\0"
"glColor4ubv\0"
"glColor4ubVertex2fSUN\0"
"glColor4ubVertex2fvSUN\0"
"glColor4ubVertex3fSUN\0"
"glColor4ubVertex3fvSUN\0"
"glColor4ui\0"
"glColor4uiv\0"
"glColor4us\0"
"glColor4usv\0"
"glColorFragmentOp1ATI\0"
"glColorFragmentOp2ATI\0"
"glColorFragmentOp3ATI\0"
"glColorMask\0"
"glColorMaskIndexedEXT\0"
"glColorMaterial\0"
"glColorPointer\0"
"glColorPointerEXT\0"
"glColorPointerListIBM\0"
"glColorPointervINTEL\0"
"glColorSubTable\0"
"glColorSubTableEXT\0"
"glColorTable\0"
"glColorTableEXT\0"
"glColorTableParameterfv\0"
"glColorTableParameterfvSGI\0"
"glColorTableParameteriv\0"
"glColorTableParameterivSGI\0"
"glColorTableSGI\0"
"glCombinerInputNV\0"
"glCombinerOutputNV\0"
"glCombinerParameterfNV\0"
"glCombinerParameterfvNV\0"
"glCombinerParameteriNV\0"
"glCombinerParameterivNV\0"
"glCombinerStageParameterfvNV\0"
"glCompileShader\0"
"glCompileShaderARB\0"
"glCompressedTexImage1D\0"
"glCompressedTexImage1DARB\0"
"glCompressedTexImage2D\0"
"glCompressedTexImage2DARB\0"
"glCompressedTexImage3D\0"
"glCompressedTexImage3DARB\0"
"glCompressedTexSubImage1D\0"
"glCompressedTexSubImage1DARB\0"
"glCompressedTexSubImage2D\0"
"glCompressedTexSubImage2DARB\0"
"glCompressedTexSubImage3D\0"
"glCompressedTexSubImage3DARB\0"
"glConvolutionFilter1D\0"
"glConvolutionFilter1DEXT\0"
"glConvolutionFilter2D\0"
"glConvolutionFilter2DEXT\0"
"glConvolutionParameterf\0"
"glConvolutionParameterfEXT\0"
"glConvolutionParameterfv\0"
"glConvolutionParameterfvEXT\0"
"glConvolutionParameteri\0"
"glConvolutionParameteriEXT\0"
"glConvolutionParameteriv\0"
"glConvolutionParameterivEXT\0"
"glCopyColorSubTable\0"
"glCopyColorSubTableEXT\0"
"glCopyColorTable\0"
"glCopyColorTableSGI\0"
"glCopyConvolutionFilter1D\0"
"glCopyConvolutionFilter1DEXT\0"
"glCopyConvolutionFilter2D\0"
"glCopyConvolutionFilter2DEXT\0"
"glCopyPixels\0"
"glCopyTexImage1D\0"
"glCopyTexImage1DEXT\0"
"glCopyTexImage2D\0"
"glCopyTexImage2DEXT\0"
"glCopyTexSubImage1D\0"
"glCopyTexSubImage1DEXT\0"
"glCopyTexSubImage2D\0"
"glCopyTexSubImage2DEXT\0"
"glCopyTexSubImage3D\0"
"glCopyTexSubImage3DEXT\0"
"glCreateProgram\0"
"glCreateProgramObjectARB\0"
"glCreateShader\0"
"glCreateShaderObjectARB\0"
"glCullFace\0"
"glCullParameterdvEXT\0"
"glCullParameterfvEXT\0"
"glCurrentPaletteMatrixARB\0"
"glDeformationMap3dSGIX\0"
"glDeformationMap3fSGIX\0"
"glDeformSGIX\0"
"glDeleteArraySetsARB\0"
"glDeleteAsyncMarkersSGIX\0"
"glDeleteBufferRegion\0"
"glDeleteBufferRegionEXT\0"
"glDeleteBuffers\0"
"glDeleteBuffersARB\0"
"glDeleteFencesAPPLE\0"
"glDeleteFencesNV\0"
"glDeleteFragmentShaderATI\0"
"glDeleteFramebuffersEXT\0"
"glDeleteLists\0"
"glDeleteObjectARB\0"
"glDeleteObjectBufferATI\0"
"glDeleteOcclusionQueries\0"
"glDeleteOcclusionQueriesNV\0"
"glDeleteProgram\0"
"glDeleteProgramsARB\0"
"glDeleteProgramsNV\0"
"glDeleteQueries\0"
"glDeleteQueriesARB\0"
"glDeleteRenderbuffersEXT\0"
"glDeleteShader\0"
"glDeleteTextures\0"
"glDeleteTexturesEXT\0"
"glDeleteVertexArraysAPPLE\0"
"glDeleteVertexShaderEXT\0"
"glDeleteVisibilityQueriesATI\0"
"glDepthBoundsdNV\0"
"glDepthBoundsEXT\0"
"glDepthFunc\0"
"glDepthMask\0"
"glDepthRange\0"
"glDepthRangedNV\0"
"glDetachObjectARB\0"
"glDetachShader\0"
"glDetailTexFuncSGIS\0"
"glDisable\0"
"glDisableClientState\0"
"glDisableIndexedEXT\0"
"glDisableVariantClientStateEXT\0"
"glDisableVertexAttribAPPLE\0"
"glDisableVertexAttribArray\0"
"glDisableVertexAttribArrayARB\0"
"glDrawArrays\0"
"glDrawArraysEXT\0"
"glDrawArraysInstancedEXT\0"
"glDrawBuffer\0"
"glDrawBufferRegion\0"
"glDrawBufferRegionEXT\0"
"glDrawBuffers\0"
"glDrawBuffersARB\0"
"glDrawBuffersATI\0"
"glDrawElementArrayAPPLE\0"
"glDrawElementArrayATI\0"
"glDrawElements\0"
"glDrawElementsFGL\0"
"glDrawElementsInstancedEXT\0"
"glDrawMeshArraysSUN\0"
"glDrawMeshNV\0"
"glDrawPixels\0"
"glDrawRangeElementArrayAPPLE\0"
"glDrawRangeElementArrayATI\0"
"glDrawRangeElements\0"
"glDrawRangeElementsEXT\0"
"glDrawWireTrianglesFGL\0"
"glEdgeFlag\0"
"glEdgeFlagPointer\0"
"glEdgeFlagPointerEXT\0"
"glEdgeFlagPointerListIBM\0"
"glEdgeFlagv\0"
"glElementPointerAPPLE\0"
"glElementPointerATI\0"
"glEnable\0"
"glEnableClientState\0"
"glEnableIndexedEXT\0"
"glEnableVariantClientStateEXT\0"
"glEnableVertexAttribAPPLE\0"
"glEnableVertexAttribArray\0"
"glEnableVertexAttribArrayARB\0"
"glEnd\0"
"glEndConditionalRenderNVX\0"
"glEndDefineVisibilityQueryATI\0"
"glEndFragmentShaderATI\0"
"glEndList\0"
"glEndOcclusionQuery\0"
"glEndOcclusionQueryNV\0"
"glEndQuery\0"
"glEndQueryARB\0"
"glEndSceneEXT\0"
"glEndTransformFeedbackNV\0"
"glEndUseVisibilityQueryATI\0"
"glEndVertexShaderEXT\0"
"glEvalCoord1d\0"
"glEvalCoord1dv\0"
"glEvalCoord1f\0"
"glEvalCoord1fv\0"
"glEvalCoord2d\0"
"glEvalCoord2dv\0"
"glEvalCoord2f\0"
"glEvalCoord2fv\0"
"glEvalMapsNV\0"
"glEvalMesh1\0"
"glEvalMesh2\0"
"glEvalPoint1\0"
"glEvalPoint2\0"
"glExecuteProgramNV\0"
"glExtractComponentEXT\0"
"glFeedbackBuffer\0"
"glFinalCombinerInputNV\0"
"glFinish\0"
"glFinishAsyncSGIX\0"
"glFinishFenceAPPLE\0"
"glFinishFenceNV\0"
"glFinishObjectAPPLE\0"
"glFinishRenderAPPLE\0"
"glFinishTextureSUNX\0"
"glFlush\0"
"glFlushMappedBufferRangeAPPLE\0"
"glFlushPixelDataRangeNV\0"
"glFlushRasterSGIX\0"
"glFlushRenderAPPLE\0"
"glFlushVertexArrayRangeAPPLE\0"
"glFlushVertexArrayRangeNV\0"
"glFogCoordd\0"
"glFogCoorddEXT\0"
"glFogCoorddv\0"
"glFogCoorddvEXT\0"
"glFogCoordf\0"
"glFogCoordfEXT\0"
"glFogCoordfv\0"
"glFogCoordfvEXT\0"
"glFogCoordhNV\0"
"glFogCoordhvNV\0"
"glFogCoordPointer\0"
"glFogCoordPointerEXT\0"
"glFogCoordPointerListIBM\0"
"glFogf\0"
"glFogFuncSGIS\0"
"glFogfv\0"
"glFogi\0"
"glFogiv\0"
"glFragmentColorMaterialEXT\0"
"glFragmentColorMaterialSGIX\0"
"glFragmentLightfEXT\0"
"glFragmentLightfSGIX\0"
"glFragmentLightfvEXT\0"
"glFragmentLightfvSGIX\0"
"glFragmentLightiEXT\0"
"glFragmentLightiSGIX\0"
"glFragmentLightivEXT\0"
"glFragmentLightivSGIX\0"
"glFragmentLightModelfEXT\0"
"glFragmentLightModelfSGIX\0"
"glFragmentLightModelfvEXT\0"
"glFragmentLightModelfvSGIX\0"
"glFragmentLightModeliEXT\0"
"glFragmentLightModeliSGIX\0"
"glFragmentLightModelivEXT\0"
"glFragmentLightModelivSGIX\0"
"glFragmentMaterialfEXT\0"
"glFragmentMaterialfSGIX\0"
"glFragmentMaterialfvEXT\0"
"glFragmentMaterialfvSGIX\0"
"glFragmentMaterialiEXT\0"
"glFragmentMaterialiSGIX\0"
"glFragmentMaterialivEXT\0"
"glFragmentMaterialivSGIX\0"
"glFramebufferRenderbufferEXT\0"
"glFramebufferTexture1DEXT\0"
"glFramebufferTexture2DEXT\0"
"glFramebufferTexture3DEXT\0"
"glFramebufferTextureEXT\0"
"glFramebufferTextureFaceEXT\0"
"glFramebufferTextureLayerEXT\0"
"glFrameZoomSGIX\0"
"glFreeObjectBufferATI\0"
"glFrontFace\0"
"glFrustum\0"
"glGenArraySetsARB\0"
"glGenAsyncMarkersSGIX\0"
"glGenBuffers\0"
"glGenBuffersARB\0"
"glGenerateMipmapEXT\0"
"glGenFencesAPPLE\0"
"glGenFencesNV\0"
"glGenFragmentShadersATI\0"
"glGenFramebuffersEXT\0"
"glGenLists\0"
"glGenOcclusionQueries\0"
"glGenOcclusionQueriesNV\0"
"glGenProgramsARB\0"
"glGenProgramsNV\0"
"glGenQueries\0"
"glGenQueriesARB\0"
"glGenRenderbuffersEXT\0"
"glGenSymbolsEXT\0"
"glGenTextures\0"
"glGenTexturesEXT\0"
"glGenVertexArraysAPPLE\0"
"glGenVertexShadersEXT\0"
"glGenVisibilityQueriesATI\0"
"glGetActiveAttrib\0"
"glGetActiveAttribARB\0"
"glGetActiveUniform\0"
"glGetActiveUniformARB\0"
"glGetActiveVaryingNV\0"
"glGetArrayObjectfvATI\0"
"glGetArrayObjectivATI\0"
"glGetAttachedObjectsARB\0"
"glGetAttachedShaders\0"
"glGetAttribLocation\0"
"glGetAttribLocationARB\0"
"glGetBooleanIndexedvEXT\0"
"glGetBooleanv\0"
"glGetBufferParameteriv\0"
"glGetBufferParameterivARB\0"
"glGetBufferPointerv\0"
"glGetBufferPointervARB\0"
"glGetBufferSubData\0"
"glGetBufferSubDataARB\0"
"glGetClipPlane\0"
"glGetColorTable\0"
"glGetColorTableEXT\0"
"glGetColorTableParameterfv\0"
"glGetColorTableParameterfvEXT\0"
"glGetColorTableParameterfvSGI\0"
"glGetColorTableParameteriv\0"
"glGetColorTableParameterivEXT\0"
"glGetColorTableParameterivSGI\0"
"glGetColorTableSGI\0"
"glGetCombinerInputParameterfvNV\0"
"glGetCombinerInputParameterivNV\0"
"glGetCombinerOutputParameterfvNV\0"
"glGetCombinerOutputParameterivNV\0"
"glGetCombinerStageParameterfvNV\0"
"glGetCompressedTexImage\0"
"glGetCompressedTexImageARB\0"
"glGetConvolutionFilter\0"
"glGetConvolutionFilterEXT\0"
"glGetConvolutionParameterfv\0"
"glGetConvolutionParameterfvEXT\0"
"glGetConvolutionParameteriv\0"
"glGetConvolutionParameterivEXT\0"
"glGetDetailTexFuncSGIS\0"
"glGetDoublev\0"
"glGetError\0"
"glGetFenceivNV\0"
"glGetFinalCombinerInputParameterfvNV\0"
"glGetFinalCombinerInputParameterivNV\0"
"glGetFloatv\0"
"glGetFogFuncSGIS\0"
"glGetFragDataLocationEXT\0"
"glGetFragmentLightfvEXT\0"
"glGetFragmentLightfvSGIX\0"
"glGetFragmentLightivEXT\0"
"glGetFragmentLightivSGIX\0"
"glGetFragmentMaterialfvEXT\0"
"glGetFragmentMaterialfvSGIX\0"
"glGetFragmentMaterialivEXT\0"
"glGetFragmentMaterialivSGIX\0"
"glGetFramebufferAttachmentParameterivEXT\0"
"glGetHandleARB\0"
"glGetHistogram\0"
"glGetHistogramEXT\0"
"glGetHistogramParameterfv\0"
"glGetHistogramParameterfvEXT\0"
"glGetHistogramParameteriv\0"
"glGetHistogramParameterivEXT\0"
"glGetImageTransformParameterfvHP\0"
"glGetImageTransformParameterivHP\0"
"glGetInfoLogARB\0"
"glGetInstrumentsSGIX\0"
"glGetIntegerIndexedvEXT\0"
"glGetIntegerv\0"
"glGetInvariantBooleanvEXT\0"
"glGetInvariantFloatvEXT\0"
"glGetInvariantIntegervEXT\0"
"glGetLightfv\0"
"glGetLightiv\0"
"glGetListParameterfvSGIX\0"
"glGetListParameterivSGIX\0"
"glGetLocalConstantBooleanvEXT\0"
"glGetLocalConstantFloatvEXT\0"
"glGetLocalConstantIntegervEXT\0"
"glGetMapAttribParameterfvNV\0"
"glGetMapAttribParameterivNV\0"
"glGetMapControlPointsNV\0"
"glGetMapdv\0"
"glGetMapfv\0"
"glGetMapiv\0"
"glGetMapParameterfvNV\0"
"glGetMapParameterivNV\0"
"glGetMaterialfv\0"
"glGetMaterialiv\0"
"glGetMinmax\0"
"glGetMinmaxEXT\0"
"glGetMinmaxParameterfv\0"
"glGetMinmaxParameterfvEXT\0"
"glGetMinmaxParameteriv\0"
"glGetMinmaxParameterivEXT\0"
"glGetObjectBufferfvATI\0"
"glGetObjectBufferivATI\0"
"glGetObjectParameterfvARB\0"
"glGetObjectParameterivARB\0"
"glGetOcclusionQueryiv\0"
"glGetOcclusionQueryivNV\0"
"glGetOcclusionQueryuiv\0"
"glGetOcclusionQueryuivNV\0"
"glGetPixelMapfv\0"
"glGetPixelMapuiv\0"
"glGetPixelMapusv\0"
"glGetPixelTexGenParameterfvSGIS\0"
"glGetPixelTexGenParameterivSGIS\0"
"glGetPixelTransformParameterfvEXT\0"
"glGetPixelTransformParameterivEXT\0"
"glGetPointerv\0"
"glGetPointervEXT\0"
"glGetPolygonStipple\0"
"glGetProgramEnvParameterdvARB\0"
"glGetProgramEnvParameterfvARB\0"
"glGetProgramEnvParameterIivNV\0"
"glGetProgramEnvParameterIuivNV\0"
"glGetProgramInfoLog\0"
"glGetProgramiv\0"
"glGetProgramivARB\0"
"glGetProgramivNV\0"
"glGetProgramLocalParameterdvARB\0"
"glGetProgramLocalParameterfvARB\0"
"glGetProgramLocalParameterIivNV\0"
"glGetProgramLocalParameterIuivNV\0"
"glGetProgramNamedParameterdvNV\0"
"glGetProgramNamedParameterfvNV\0"
"glGetProgramParameterdvNV\0"
"glGetProgramParameterfvNV\0"
"glGetProgramRegisterfvMESA\0"
"glGetProgramStringARB\0"
"glGetProgramStringNV\0"
"glGetQueryiv\0"
"glGetQueryivARB\0"
"glGetQueryObjecti64vEXT\0"
"glGetQueryObjectiv\0"
"glGetQueryObjectivARB\0"
"glGetQueryObjectui64vEXT\0"
"glGetQueryObjectuiv\0"
"glGetQueryObjectuivARB\0"
"glGetRenderbufferParameterivEXT\0"
"glGetSeparableFilter\0"
"glGetSeparableFilterEXT\0"
"glGetShaderInfoLog\0"
"glGetShaderiv\0"
"glGetShaderSource\0"
"glGetShaderSourceARB\0"
"glGetSharpenTexFuncSGIS\0"
"glGetString\0"
"glGetTexBumpParameterfvATI\0"
"glGetTexBumpParameterivATI\0"
"glGetTexEnvfv\0"
"glGetTexEnviv\0"
"glGetTexFilterFuncSGIS\0"
"glGetTexGendv\0"
"glGetTexGenfv\0"
"glGetTexGeniv\0"
"glGetTexImage\0"
"glGetTexLevelParameterfv\0"
"glGetTexLevelParameteriv\0"
"glGetTexParameterfv\0"
"glGetTexParameterIivEXT\0"
"glGetTexParameterIuivEXT\0"
"glGetTexParameteriv\0"
"glGetTexParameterPointervAPPLE\0"
"glGetTrackMatrixivNV\0"
"glGetTransformFeedbackVaryingNV\0"
"glGetUniformBufferSizeEXT\0"
"glGetUniformfv\0"
"glGetUniformfvARB\0"
"glGetUniformiv\0"
"glGetUniformivARB\0"
"glGetUniformLocation\0"
"glGetUniformLocationARB\0"
"glGetUniformOffsetEXT\0"
"glGetUniformuivEXT\0"
"glGetVariantArrayObjectfvATI\0"
"glGetVariantArrayObjectivATI\0"
"glGetVariantBooleanvEXT\0"
"glGetVariantFloatvEXT\0"
"glGetVariantIntegervEXT\0"
"glGetVariantPointervEXT\0"
"glGetVaryingLocationNV\0"
"glGetVertexAttribArrayObjectfvATI\0"
"glGetVertexAttribArrayObjectivATI\0"
"glGetVertexAttribdv\0"
"glGetVertexAttribdvARB\0"
"glGetVertexAttribdvNV\0"
"glGetVertexAttribfv\0"
"glGetVertexAttribfvARB\0"
"glGetVertexAttribfvNV\0"
"glGetVertexAttribIivEXT\0"
"glGetVertexAttribIuivEXT\0"
"glGetVertexAttribiv\0"
"glGetVertexAttribivARB\0"
"glGetVertexAttribivNV\0"
"glGetVertexAttribPointerv\0"
"glGetVertexAttribPointervARB\0"
"glGetVertexAttribPointervNV\0"
"glGlobalAlphaFactorbSUN\0"
"glGlobalAlphaFactordSUN\0"
"glGlobalAlphaFactorfSUN\0"
"glGlobalAlphaFactoriSUN\0"
"glGlobalAlphaFactorsSUN\0"
"glGlobalAlphaFactorubSUN\0"
"glGlobalAlphaFactoruiSUN\0"
"glGlobalAlphaFactorusSUN\0"
"glHint\0"
"glHintPGI\0"
"glHistogram\0"
"glHistogramEXT\0"
"glIglooInterfaceSGIX\0"
"glImageTransformParameterfHP\0"
"glImageTransformParameterfvHP\0"
"glImageTransformParameteriHP\0"
"glImageTransformParameterivHP\0"
"glIndexd\0"
"glIndexdv\0"
"glIndexf\0"
"glIndexFuncEXT\0"
"glIndexfv\0"
"glIndexi\0"
"glIndexiv\0"
"glIndexMask\0"
"glIndexMaterialEXT\0"
"glIndexPointer\0"
"glIndexPointerEXT\0"
"glIndexPointerListIBM\0"
"glIndexs\0"
"glIndexsv\0"
"glIndexub\0"
"glIndexubv\0"
"glInitNames\0"
"glInsertComponentEXT\0"
"glInstrumentsBufferSGIX\0"
"glInterleavedArrays\0"
"glIsArraySetARB\0"
"glIsAsyncMarkerSGIX\0"
"glIsBuffer\0"
"glIsBufferARB\0"
"glIsEnabled\0"
"glIsEnabledIndexedEXT\0"
"glIsFenceAPPLE\0"
"glIsFenceNV\0"
"glIsFramebufferEXT\0"
"glIsList\0"
"glIsObjectBufferATI\0"
"glIsOcclusionQuery\0"
"_glIsOcclusionQueryNV\0"
"glIsOcclusionQueryNV\0"
"glIsProgram\0"
"glIsProgramARB\0"
"glIsProgramNV\0"
"glIsQuery\0"
"glIsQueryARB\0"
"glIsRenderbufferEXT\0"
"glIsShader\0"
"glIsTexture\0"
"glIsTextureEXT\0"
"glIsVariantEnabledEXT\0"
"glIsVertexArrayAPPLE\0"
"glIsVertexAttribEnabledAPPLE\0"
"glLightEnviEXT\0"
"glLightEnviSGIX\0"
"glLightf\0"
"glLightfv\0"
"glLighti\0"
"glLightiv\0"
"glLightModelf\0"
"glLightModelfv\0"
"glLightModeli\0"
"glLightModeliv\0"
"glLineStipple\0"
"glLineWidth\0"
"glLinkProgram\0"
"glLinkProgramARB\0"
"glListBase\0"
"glListParameterfSGIX\0"
"glListParameterfvSGIX\0"
"glListParameteriSGIX\0"
"glListParameterivSGIX\0"
"glLoadIdentity\0"
"glLoadIdentityDeformationMapSGIX\0"
"glLoadMatrixd\0"
"glLoadMatrixf\0"
"glLoadName\0"
"glLoadProgramNV\0"
"glLoadTransposeMatrixd\0"
"glLoadTransposeMatrixdARB\0"
"glLoadTransposeMatrixf\0"
"glLoadTransposeMatrixfARB\0"
"glLockArraysEXT\0"
"glLogicOp\0"
"glMap1d\0"
"glMap1f\0"
"glMap2d\0"
"glMap2f\0"
"glMapBuffer\0"
"glMapBufferARB\0"
"glMapControlPointsNV\0"
"glMapGrid1d\0"
"glMapGrid1f\0"
"glMapGrid2d\0"
"glMapGrid2f\0"
"glMapObjectBufferATI\0"
"glMapParameterfvNV\0"
"glMapParameterivNV\0"
"glMapTexture3DATI\0"
"glMapVertexAttrib1dAPPLE\0"
"glMapVertexAttrib1fAPPLE\0"
"glMapVertexAttrib2dAPPLE\0"
"glMapVertexAttrib2fAPPLE\0"
"glMaterialf\0"
"glMaterialfv\0"
"glMateriali\0"
"glMaterialiv\0"
"glMatrixIndexPointerARB\0"
"glMatrixIndexubvARB\0"
"glMatrixIndexuivARB\0"
"glMatrixIndexusvARB\0"
"glMatrixMode\0"
"glMinmax\0"
"glMinmaxEXT\0"
"glMultiDrawArrays\0"
"glMultiDrawArraysEXT\0"
"glMultiDrawElementArrayAPPLE\0"
"glMultiDrawElements\0"
"glMultiDrawElementsEXT\0"
"glMultiDrawRangeElementArrayAPPLE\0"
"glMultiModeDrawArraysIBM\0"
"glMultiModeDrawElementsIBM\0"
"glMultiTexCoord1d\0"
"glMultiTexCoord1dARB\0"
"glMultiTexCoord1dSGIS\0"
"glMultiTexCoord1dv\0"
"glMultiTexCoord1dvARB\0"
"glMultiTexCoord1dvSGIS\0"
"glMultiTexCoord1f\0"
"glMultiTexCoord1fARB\0"
"glMultiTexCoord1fSGIS\0"
"glMultiTexCoord1fv\0"
"glMultiTexCoord1fvARB\0"
"glMultiTexCoord1fvSGIS\0"
"glMultiTexCoord1hNV\0"
"glMultiTexCoord1hvNV\0"
"glMultiTexCoord1i\0"
"glMultiTexCoord1iARB\0"
"glMultiTexCoord1iSGIS\0"
"glMultiTexCoord1iv\0"
"glMultiTexCoord1ivARB\0"
"glMultiTexCoord1ivSGIS\0"
"glMultiTexCoord1s\0"
"glMultiTexCoord1sARB\0"
"glMultiTexCoord1sSGIS\0"
"glMultiTexCoord1sv\0"
"glMultiTexCoord1svARB\0"
"glMultiTexCoord1svSGIS\0"
"glMultiTexCoord2d\0"
"glMultiTexCoord2dARB\0"
"glMultiTexCoord2dSGIS\0"
"glMultiTexCoord2dv\0"
"glMultiTexCoord2dvARB\0"
"glMultiTexCoord2dvSGIS\0"
"glMultiTexCoord2f\0"
"glMultiTexCoord2fARB\0"
"glMultiTexCoord2fSGIS\0"
"glMultiTexCoord2fv\0"
"glMultiTexCoord2fvARB\0"
"glMultiTexCoord2fvSGIS\0"
"glMultiTexCoord2hNV\0"
"glMultiTexCoord2hvNV\0"
"glMultiTexCoord2i\0"
"glMultiTexCoord2iARB\0"
"glMultiTexCoord2iSGIS\0"
"glMultiTexCoord2iv\0"
"glMultiTexCoord2ivARB\0"
"glMultiTexCoord2ivSGIS\0"
"glMultiTexCoord2s\0"
"glMultiTexCoord2sARB\0"
"glMultiTexCoord2sSGIS\0"
"glMultiTexCoord2sv\0"
"glMultiTexCoord2svARB\0"
"glMultiTexCoord2svSGIS\0"
"glMultiTexCoord3d\0"
"glMultiTexCoord3dARB\0"
"glMultiTexCoord3dSGIS\0"
"glMultiTexCoord3dv\0"
"glMultiTexCoord3dvARB\0"
"glMultiTexCoord3dvSGIS\0"
"glMultiTexCoord3f\0"
"glMultiTexCoord3fARB\0"
"glMultiTexCoord3fSGIS\0"
"glMultiTexCoord3fv\0"
"glMultiTexCoord3fvARB\0"
"glMultiTexCoord3fvSGIS\0"
"glMultiTexCoord3hNV\0"
"glMultiTexCoord3hvNV\0"
"glMultiTexCoord3i\0"
"glMultiTexCoord3iARB\0"
"glMultiTexCoord3iSGIS\0"
"glMultiTexCoord3iv\0"
"glMultiTexCoord3ivARB\0"
"glMultiTexCoord3ivSGIS\0"
"glMultiTexCoord3s\0"
"glMultiTexCoord3sARB\0"
"glMultiTexCoord3sSGIS\0"
"glMultiTexCoord3sv\0"
"glMultiTexCoord3svARB\0"
"glMultiTexCoord3svSGIS\0"
"glMultiTexCoord4d\0"
"glMultiTexCoord4dARB\0"
"glMultiTexCoord4dSGIS\0"
"glMultiTexCoord4dv\0"
"glMultiTexCoord4dvARB\0"
"glMultiTexCoord4dvSGIS\0"
"glMultiTexCoord4f\0"
"glMultiTexCoord4fARB\0"
"glMultiTexCoord4fSGIS\0"
"glMultiTexCoord4fv\0"
"glMultiTexCoord4fvARB\0"
"glMultiTexCoord4fvSGIS\0"
"glMultiTexCoord4hNV\0"
"glMultiTexCoord4hvNV\0"
"glMultiTexCoord4i\0"
"glMultiTexCoord4iARB\0"
"glMultiTexCoord4iSGIS\0"
"glMultiTexCoord4iv\0"
"glMultiTexCoord4ivARB\0"
"glMultiTexCoord4ivSGIS\0"
"glMultiTexCoord4s\0"
"glMultiTexCoord4sARB\0"
"glMultiTexCoord4sSGIS\0"
"glMultiTexCoord4sv\0"
"glMultiTexCoord4svARB\0"
"glMultiTexCoord4svSGIS\0"
"glMultiTexCoordPointerSGIS\0"
"glMultMatrixd\0"
"glMultMatrixf\0"
"glMultTransposeMatrixd\0"
"glMultTransposeMatrixdARB\0"
"glMultTransposeMatrixf\0"
"glMultTransposeMatrixfARB\0"
"glNewBufferRegion\0"
"glNewBufferRegionEXT\0"
"glNewList\0"
"glNewObjectBufferATI\0"
"glNormal3b\0"
"glNormal3bv\0"
"glNormal3d\0"
"glNormal3dv\0"
"glNormal3f\0"
"glNormal3fv\0"
"glNormal3fVertex3fSUN\0"
"glNormal3fVertex3fvSUN\0"
"glNormal3hNV\0"
"glNormal3hvNV\0"
"glNormal3i\0"
"glNormal3iv\0"
"glNormal3s\0"
"glNormal3sv\0"
"glNormalPointer\0"
"glNormalPointerEXT\0"
"glNormalPointerListIBM\0"
"glNormalPointervINTEL\0"
"glNormalStream3bATI\0"
"glNormalStream3bvATI\0"
"glNormalStream3dATI\0"
"glNormalStream3dvATI\0"
"glNormalStream3fATI\0"
"glNormalStream3fvATI\0"
"glNormalStream3iATI\0"
"glNormalStream3ivATI\0"
"glNormalStream3sATI\0"
"glNormalStream3svATI\0"
"glOrtho\0"
"glPassTexCoordATI\0"
"glPassThrough\0"
"glPixelDataRangeNV\0"
"glPixelMapfv\0"
"glPixelMapuiv\0"
"glPixelMapusv\0"
"glPixelStoref\0"
"glPixelStorei\0"
"glPixelTexGenParameterfSGIS\0"
"glPixelTexGenParameterfvSGIS\0"
"glPixelTexGenParameteriSGIS\0"
"glPixelTexGenParameterivSGIS\0"
"glPixelTexGenSGIX\0"
"glPixelTransferf\0"
"glPixelTransferi\0"
"glPixelTransformParameterfEXT\0"
"glPixelTransformParameterfvEXT\0"
"glPixelTransformParameteriEXT\0"
"glPixelTransformParameterivEXT\0"
"glPixelZoom\0"
"glPNTrianglesfATI\0"
"glPNTrianglesiATI\0"
"glPointParameterf\0"
"glPointParameterfARB\0"
"glPointParameterfEXT\0"
"glPointParameterfSGIS\0"
"glPointParameterfv\0"
"glPointParameterfvARB\0"
"glPointParameterfvEXT\0"
"glPointParameterfvSGIS\0"
"glPointParameteri\0"
"glPointParameteriEXT\0"
"glPointParameteriNV\0"
"glPointParameteriv\0"
"glPointParameterivEXT\0"
"glPointParameterivNV\0"
"glPointSize\0"
"glPollAsyncSGIX\0"
"glPollInstrumentsSGIX\0"
"glPolygonMode\0"
"glPolygonOffset\0"
"glPolygonOffsetEXT\0"
"glPolygonStipple\0"
"glPopAttrib\0"
"glPopClientAttrib\0"
"glPopMatrix\0"
"glPopName\0"
"glPrimitiveRestartIndexNV\0"
"glPrimitiveRestartNV\0"
"glPrioritizeTextures\0"
"glPrioritizeTexturesEXT\0"
"glProgramBufferParametersfvNV\0"
"glProgramBufferParametersIivNV\0"
"glProgramBufferParametersIuivNV\0"
"glProgramCallbackMESA\0"
"glProgramEnvParameter4dARB\0"
"glProgramEnvParameter4dvARB\0"
"glProgramEnvParameter4fARB\0"
"glProgramEnvParameter4fvARB\0"
"glProgramEnvParameterI4iNV\0"
"glProgramEnvParameterI4ivNV\0"
"glProgramEnvParameterI4uiNV\0"
"glProgramEnvParameterI4uivNV\0"
"glProgramEnvParameters4fvEXT\0"
"glProgramEnvParametersI4ivNV\0"
"glProgramEnvParametersI4uivNV\0"
"glProgramLocalParameter4dARB\0"
"glProgramLocalParameter4dvARB\0"
"glProgramLocalParameter4fARB\0"
"glProgramLocalParameter4fvARB\0"
"glProgramLocalParameterI4iNV\0"
"glProgramLocalParameterI4ivNV\0"
"glProgramLocalParameterI4uiNV\0"
"glProgramLocalParameterI4uivNV\0"
"glProgramLocalParameters4fvEXT\0"
"glProgramLocalParametersI4ivNV\0"
"glProgramLocalParametersI4uivNV\0"
"glProgramNamedParameter4dNV\0"
"glProgramNamedParameter4dvNV\0"
"glProgramNamedParameter4fNV\0"
"glProgramNamedParameter4fvNV\0"
"glProgramParameter4dNV\0"
"glProgramParameter4dvNV\0"
"glProgramParameter4fNV\0"
"glProgramParameter4fvNV\0"
"glProgramParameteriEXT\0"
"glProgramParameters4dvNV\0"
"glProgramParameters4fvNV\0"
"glProgramStringARB\0"
"glProgramVertexLimitNV\0"
"glPushAttrib\0"
"glPushClientAttrib\0"
"glPushMatrix\0"
"glPushName\0"
"glRasterPos2d\0"
"glRasterPos2dv\0"
"glRasterPos2f\0"
"glRasterPos2fv\0"
"glRasterPos2i\0"
"glRasterPos2iv\0"
"glRasterPos2s\0"
"glRasterPos2sv\0"
"glRasterPos3d\0"
"glRasterPos3dv\0"
"glRasterPos3f\0"
"glRasterPos3fv\0"
"glRasterPos3i\0"
"glRasterPos3iv\0"
"glRasterPos3s\0"
"glRasterPos3sv\0"
"glRasterPos4d\0"
"glRasterPos4dv\0"
"glRasterPos4f\0"
"glRasterPos4fv\0"
"glRasterPos4i\0"
"glRasterPos4iv\0"
"glRasterPos4s\0"
"glRasterPos4sv\0"
"glReadBuffer\0"
"glReadBufferRegion\0"
"glReadBufferRegionEXT\0"
"glReadInstrumentsSGIX\0"
"glReadPixels\0"
"glReadVideoPixelsSUN\0"
"glRectd\0"
"glRectdv\0"
"glRectf\0"
"glRectfv\0"
"glRecti\0"
"glRectiv\0"
"glRects\0"
"glRectsv\0"
"glReferencePlaneSGIX\0"
"glRenderbufferStorageEXT\0"
"glRenderbufferStorageMultisampleCoverageNV\0"
"glRenderbufferStorageMultisampleEXT\0"
"glRenderMode\0"
"glReplacementCodePointerSUN\0"
"glReplacementCodeubSUN\0"
"glReplacementCodeubvSUN\0"
"glReplacementCodeuiColor3fVertex3fSUN\0"
"glReplacementCodeuiColor3fVertex3fvSUN\0"
"glReplacementCodeuiColor4fNormal3fVertex3fSUN\0"
"glReplacementCodeuiColor4fNormal3fVertex3fvSUN\0"
"glReplacementCodeuiColor4ubVertex3fSUN\0"
"glReplacementCodeuiColor4ubVertex3fvSUN\0"
"glReplacementCodeuiNormal3fVertex3fSUN\0"
"glReplacementCodeuiNormal3fVertex3fvSUN\0"
"glReplacementCodeuiSUN\0"
"glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN\0"
"glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN\0"
"glReplacementCodeuiTexCoord2fNormal3fVertex3fSUN\0"
"glReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN\0"
"glReplacementCodeuiTexCoord2fVertex3fSUN\0"
"glReplacementCodeuiTexCoord2fVertex3fvSUN\0"
"glReplacementCodeuiVertex3fSUN\0"
"glReplacementCodeuiVertex3fvSUN\0"
"glReplacementCodeuivSUN\0"
"glReplacementCodeusSUN\0"
"glReplacementCodeusvSUN\0"
"glRequestResidentProgramsNV\0"
"glResetHistogram\0"
"glResetHistogramEXT\0"
"glResetMinmax\0"
"glResetMinmaxEXT\0"
"glResizeBuffersMESA\0"
"glRotated\0"
"glRotatef\0"
"glSampleCoverage\0"
"glSampleCoverageARB\0"
"glSampleMapATI\0"
"glSampleMaskEXT\0"
"glSampleMaskSGIS\0"
"glSamplePassARB\0"
"glSamplePatternEXT\0"
"glSamplePatternSGIS\0"
"glScaled\0"
"glScalef\0"
"glScissor\0"
"glSecondaryColor3b\0"
"glSecondaryColor3bEXT\0"
"glSecondaryColor3bv\0"
"glSecondaryColor3bvEXT\0"
"glSecondaryColor3d\0"
"glSecondaryColor3dEXT\0"
"glSecondaryColor3dv\0"
"glSecondaryColor3dvEXT\0"
"glSecondaryColor3f\0"
"glSecondaryColor3fEXT\0"
"glSecondaryColor3fv\0"
"glSecondaryColor3fvEXT\0"
"glSecondaryColor3hNV\0"
"glSecondaryColor3hvNV\0"
"glSecondaryColor3i\0"
"glSecondaryColor3iEXT\0"
"glSecondaryColor3iv\0"
"glSecondaryColor3ivEXT\0"
"glSecondaryColor3s\0"
"glSecondaryColor3sEXT\0"
"glSecondaryColor3sv\0"
"glSecondaryColor3svEXT\0"
"glSecondaryColor3ub\0"
"glSecondaryColor3ubEXT\0"
"glSecondaryColor3ubv\0"
"glSecondaryColor3ubvEXT\0"
"glSecondaryColor3ui\0"
"glSecondaryColor3uiEXT\0"
"glSecondaryColor3uiv\0"
"glSecondaryColor3uivEXT\0"
"glSecondaryColor3us\0"
"glSecondaryColor3usEXT\0"
"glSecondaryColor3usv\0"
"glSecondaryColor3usvEXT\0"
"glSecondaryColorPointer\0"
"glSecondaryColorPointerEXT\0"
"glSecondaryColorPointerListIBM\0"
"glSelectBuffer\0"
"glSelectTextureCoordSetSGIS\0"
"glSelectTextureSGIS\0"
"glSelectTextureTransformSGIS\0"
"glSeparableFilter2D\0"
"glSeparableFilter2DEXT\0"
"glSetFenceAPPLE\0"
"glSetFenceNV\0"
"glSetFragmentShaderConstantATI\0"
"glSetInvariantEXT\0"
"glSetLocalConstantEXT\0"
"glShadeModel\0"
"glShaderOp1EXT\0"
"glShaderOp2EXT\0"
"glShaderOp3EXT\0"
"glShaderSource\0"
"glShaderSourceARB\0"
"glSharpenTexFuncSGIS\0"
"glSpriteParameterfSGIX\0"
"glSpriteParameterfvSGIX\0"
"glSpriteParameteriSGIX\0"
"glSpriteParameterivSGIX\0"
"glStartInstrumentsSGIX\0"
"glStencilClearTagEXT\0"
"glStencilFunc\0"
"glStencilFuncSeparate\0"
"glStencilFuncSeparateATI\0"
"glStencilMask\0"
"glStencilMaskSeparate\0"
"glStencilOp\0"
"glStencilOpSeparate\0"
"glStencilOpSeparateATI\0"
"glStopInstrumentsSGIX\0"
"glStringMarkerGREMEDY\0"
"glSwapAPPLE\0"
"glSwizzleEXT\0"
"glTagSampleBufferSGIX\0"
"glTangent3bEXT\0"
"glTangent3bvEXT\0"
"glTangent3dEXT\0"
"glTangent3dvEXT\0"
"glTangent3fEXT\0"
"glTangent3fvEXT\0"
"glTangent3iEXT\0"
"glTangent3ivEXT\0"
"glTangent3sEXT\0"
"glTangent3svEXT\0"
"glTangentPointerEXT\0"
"glTbufferMask3DFX\0"
"glTestFenceAPPLE\0"
"glTestFenceNV\0"
"glTestObjectAPPLE\0"
"glTexBufferEXT\0"
"glTexBumpParameterfvATI\0"
"glTexBumpParameterivATI\0"
"glTexCoord1d\0"
"glTexCoord1dv\0"
"glTexCoord1f\0"
"glTexCoord1fv\0"
"glTexCoord1hNV\0"
"glTexCoord1hvNV\0"
"glTexCoord1i\0"
"glTexCoord1iv\0"
"glTexCoord1s\0"
"glTexCoord1sv\0"
"glTexCoord2d\0"
"glTexCoord2dv\0"
"glTexCoord2f\0"
"glTexCoord2fColor3fVertex3fSUN\0"
"glTexCoord2fColor3fVertex3fvSUN\0"
"glTexCoord2fColor4fNormal3fVertex3fSUN\0"
"glTexCoord2fColor4fNormal3fVertex3fvSUN\0"
"glTexCoord2fColor4ubVertex3fSUN\0"
"glTexCoord2fColor4ubVertex3fvSUN\0"
"glTexCoord2fNormal3fVertex3fSUN\0"
"glTexCoord2fNormal3fVertex3fvSUN\0"
"glTexCoord2fv\0"
"glTexCoord2fVertex3fSUN\0"
"glTexCoord2fVertex3fvSUN\0"
"glTexCoord2hNV\0"
"glTexCoord2hvNV\0"
"glTexCoord2i\0"
"glTexCoord2iv\0"
"glTexCoord2s\0"
"glTexCoord2sv\0"
"glTexCoord3d\0"
"glTexCoord3dv\0"
"glTexCoord3f\0"
"glTexCoord3fv\0"
"glTexCoord3hNV\0"
"glTexCoord3hvNV\0"
"glTexCoord3i\0"
"glTexCoord3iv\0"
"glTexCoord3s\0"
"glTexCoord3sv\0"
"glTexCoord4d\0"
"glTexCoord4dv\0"
"glTexCoord4f\0"
"glTexCoord4fColor4fNormal3fVertex4fSUN\0"
"glTexCoord4fColor4fNormal3fVertex4fvSUN\0"
"glTexCoord4fv\0"
"glTexCoord4fVertex4fSUN\0"
"glTexCoord4fVertex4fvSUN\0"
"glTexCoord4hNV\0"
"glTexCoord4hvNV\0"
"glTexCoord4i\0"
"glTexCoord4iv\0"
"glTexCoord4s\0"
"glTexCoord4sv\0"
"glTexCoordPointer\0"
"glTexCoordPointerEXT\0"
"glTexCoordPointerListIBM\0"
"glTexCoordPointervINTEL\0"
"glTexEnvf\0"
"glTexEnvfv\0"
"glTexEnvi\0"
"glTexEnviv\0"
"glTexFilterFuncSGIS\0"
"glTexGend\0"
"glTexGendv\0"
"glTexGenf\0"
"glTexGenfv\0"
"glTexGeni\0"
"glTexGeniv\0"
"glTexImage1D\0"
"glTexImage2D\0"
"glTexImage3D\0"
"glTexImage3DEXT\0"
"glTexImage4DSGIS\0"
"glTexParameterf\0"
"glTexParameterfv\0"
"glTexParameteri\0"
"glTexParameterIivEXT\0"
"glTexParameterIuivEXT\0"
"glTexParameteriv\0"
"glTexScissorFuncINTEL\0"
"glTexScissorINTEL\0"
"glTexSubImage1D\0"
"glTexSubImage1DEXT\0"
"glTexSubImage2D\0"
"glTexSubImage2DEXT\0"
"glTexSubImage3D\0"
"glTexSubImage3DEXT\0"
"glTexSubImage4DSGIS\0"
"glTextureColorMaskSGIS\0"
"glTextureFogSGIX\0"
"glTextureLightEXT\0"
"glTextureMaterialEXT\0"
"glTextureNormalEXT\0"
"glTextureRangeAPPLE\0"
"glTrackMatrixNV\0"
"glTransformFeedbackAttribsNV\0"
"glTransformFeedbackVaryingsNV\0"
"glTranslated\0"
"glTranslatef\0"
"glUniform1f\0"
"glUniform1fARB\0"
"glUniform1fv\0"
"glUniform1fvARB\0"
"glUniform1i\0"
"glUniform1iARB\0"
"glUniform1iv\0"
"glUniform1ivARB\0"
"glUniform1uiEXT\0"
"glUniform1uivEXT\0"
"glUniform2f\0"
"glUniform2fARB\0"
"glUniform2fv\0"
"glUniform2fvARB\0"
"glUniform2i\0"
"glUniform2iARB\0"
"glUniform2iv\0"
"glUniform2ivARB\0"
"glUniform2uiEXT\0"
"glUniform2uivEXT\0"
"glUniform3f\0"
"glUniform3fARB\0"
"glUniform3fv\0"
"glUniform3fvARB\0"
"glUniform3i\0"
"glUniform3iARB\0"
"glUniform3iv\0"
"glUniform3ivARB\0"
"glUniform3uiEXT\0"
"glUniform3uivEXT\0"
"glUniform4f\0"
"glUniform4fARB\0"
"glUniform4fv\0"
"glUniform4fvARB\0"
"glUniform4i\0"
"glUniform4iARB\0"
"glUniform4iv\0"
"glUniform4ivARB\0"
"glUniform4uiEXT\0"
"glUniform4uivEXT\0"
"glUniformBufferEXT\0"
"glUniformMatrix2fv\0"
"glUniformMatrix2fvARB\0"
"glUniformMatrix2x3fv\0"
"glUniformMatrix2x4fv\0"
"glUniformMatrix3fv\0"
"glUniformMatrix3fvARB\0"
"glUniformMatrix3x2fv\0"
"glUniformMatrix3x4fv\0"
"glUniformMatrix4fv\0"
"glUniformMatrix4fvARB\0"
"glUniformMatrix4x2fv\0"
"glUniformMatrix4x3fv\0"
"glUnlockArraysEXT\0"
"glUnmapBuffer\0"
"glUnmapBufferARB\0"
"glUnmapObjectBufferATI\0"
"glUnmapTexture3DATI\0"
"glUpdateObjectBufferATI\0"
"glUseProgram\0"
"glUseProgramObjectARB\0"
"glValidateProgram\0"
"glValidateProgramARB\0"
"glValidBackBufferHintAutodesk\0"
"glVariantArrayObjectATI\0"
"glVariantbvEXT\0"
"glVariantdvEXT\0"
"glVariantfvEXT\0"
"glVariantivEXT\0"
"glVariantPointerEXT\0"
"glVariantsvEXT\0"
"glVariantubvEXT\0"
"glVariantuivEXT\0"
"glVariantusvEXT\0"
"glVertex2d\0"
"glVertex2dv\0"
"glVertex2f\0"
"glVertex2fv\0"
"glVertex2hNV\0"
"glVertex2hvNV\0"
"glVertex2i\0"
"glVertex2iv\0"
"glVertex2s\0"
"glVertex2sv\0"
"glVertex3d\0"
"glVertex3dv\0"
"glVertex3f\0"
"glVertex3fv\0"
"glVertex3hNV\0"
"glVertex3hvNV\0"
"glVertex3i\0"
"glVertex3iv\0"
"glVertex3s\0"
"glVertex3sv\0"
"glVertex4d\0"
"glVertex4dv\0"
"glVertex4f\0"
"glVertex4fv\0"
"glVertex4hNV\0"
"glVertex4hvNV\0"
"glVertex4i\0"
"glVertex4iv\0"
"glVertex4s\0"
"glVertex4sv\0"
"glVertexArrayParameteriAPPLE\0"
"glVertexArrayRangeAPPLE\0"
"glVertexArrayRangeNV\0"
"glVertexAttrib1d\0"
"glVertexAttrib1dARB\0"
"glVertexAttrib1dNV\0"
"glVertexAttrib1dv\0"
"glVertexAttrib1dvARB\0"
"glVertexAttrib1dvNV\0"
"glVertexAttrib1f\0"
"glVertexAttrib1fARB\0"
"glVertexAttrib1fNV\0"
"glVertexAttrib1fv\0"
"glVertexAttrib1fvARB\0"
"glVertexAttrib1fvNV\0"
"glVertexAttrib1hNV\0"
"glVertexAttrib1hvNV\0"
"glVertexAttrib1s\0"
"glVertexAttrib1sARB\0"
"glVertexAttrib1sNV\0"
"glVertexAttrib1sv\0"
"glVertexAttrib1svARB\0"
"glVertexAttrib1svNV\0"
"glVertexAttrib2d\0"
"glVertexAttrib2dARB\0"
"glVertexAttrib2dNV\0"
"glVertexAttrib2dv\0"
"glVertexAttrib2dvARB\0"
"glVertexAttrib2dvNV\0"
"glVertexAttrib2f\0"
"glVertexAttrib2fARB\0"
"glVertexAttrib2fNV\0"
"glVertexAttrib2fv\0"
"glVertexAttrib2fvARB\0"
"glVertexAttrib2fvNV\0"
"glVertexAttrib2hNV\0"
"glVertexAttrib2hvNV\0"
"glVertexAttrib2s\0"
"glVertexAttrib2sARB\0"
"glVertexAttrib2sNV\0"
"glVertexAttrib2sv\0"
"glVertexAttrib2svARB\0"
"glVertexAttrib2svNV\0"
"glVertexAttrib3d\0"
"glVertexAttrib3dARB\0"
"glVertexAttrib3dNV\0"
"glVertexAttrib3dv\0"
"glVertexAttrib3dvARB\0"
"glVertexAttrib3dvNV\0"
"glVertexAttrib3f\0"
"glVertexAttrib3fARB\0"
"glVertexAttrib3fNV\0"
"glVertexAttrib3fv\0"
"glVertexAttrib3fvARB\0"
"glVertexAttrib3fvNV\0"
"glVertexAttrib3hNV\0"
"glVertexAttrib3hvNV\0"
"glVertexAttrib3s\0"
"glVertexAttrib3sARB\0"
"glVertexAttrib3sNV\0"
"glVertexAttrib3sv\0"
"glVertexAttrib3svARB\0"
"glVertexAttrib3svNV\0"
"glVertexAttrib4bv\0"
"glVertexAttrib4bvARB\0"
"glVertexAttrib4d\0"
"glVertexAttrib4dARB\0"
"glVertexAttrib4dNV\0"
"glVertexAttrib4dv\0"
"glVertexAttrib4dvARB\0"
"glVertexAttrib4dvNV\0"
"glVertexAttrib4f\0"
"glVertexAttrib4fARB\0"
"glVertexAttrib4fNV\0"
"glVertexAttrib4fv\0"
"glVertexAttrib4fvARB\0"
"glVertexAttrib4fvNV\0"
"glVertexAttrib4hNV\0"
"glVertexAttrib4hvNV\0"
"glVertexAttrib4iv\0"
"glVertexAttrib4ivARB\0"
"glVertexAttrib4Nbv\0"
"glVertexAttrib4NbvARB\0"
"glVertexAttrib4Niv\0"
"glVertexAttrib4NivARB\0"
"glVertexAttrib4Nsv\0"
"glVertexAttrib4NsvARB\0"
"glVertexAttrib4Nub\0"
"glVertexAttrib4NubARB\0"
"glVertexAttrib4Nubv\0"
"glVertexAttrib4NubvARB\0"
"glVertexAttrib4Nuiv\0"
"glVertexAttrib4NuivARB\0"
"glVertexAttrib4Nusv\0"
"glVertexAttrib4NusvARB\0"
"glVertexAttrib4s\0"
"glVertexAttrib4sARB\0"
"glVertexAttrib4sNV\0"
"glVertexAttrib4sv\0"
"glVertexAttrib4svARB\0"
"glVertexAttrib4svNV\0"
"glVertexAttrib4ubNV\0"
"glVertexAttrib4ubv\0"
"glVertexAttrib4ubvARB\0"
"glVertexAttrib4ubvNV\0"
"glVertexAttrib4uiv\0"
"glVertexAttrib4uivARB\0"
"glVertexAttrib4usv\0"
"glVertexAttrib4usvARB\0"
"glVertexAttribArrayObjectATI\0"
"glVertexAttribI1iEXT\0"
"glVertexAttribI1ivEXT\0"
"glVertexAttribI1uiEXT\0"
"glVertexAttribI1uivEXT\0"
"glVertexAttribI2iEXT\0"
"glVertexAttribI2ivEXT\0"
"glVertexAttribI2uiEXT\0"
"glVertexAttribI2uivEXT\0"
"glVertexAttribI3iEXT\0"
"glVertexAttribI3ivEXT\0"
"glVertexAttribI3uiEXT\0"
"glVertexAttribI3uivEXT\0"
"glVertexAttribI4bvEXT\0"
"glVertexAttribI4iEXT\0"
"glVertexAttribI4ivEXT\0"
"glVertexAttribI4svEXT\0"
"glVertexAttribI4ubvEXT\0"
"glVertexAttribI4uiEXT\0"
"glVertexAttribI4uivEXT\0"
"glVertexAttribI4usvEXT\0"
"glVertexAttribIPointerEXT\0"
"glVertexAttribPointer\0"
"glVertexAttribPointerARB\0"
"glVertexAttribPointerNV\0"
"glVertexAttribs1dvNV\0"
"glVertexAttribs1fvNV\0"
"glVertexAttribs1hvNV\0"
"glVertexAttribs1svNV\0"
"glVertexAttribs2dvNV\0"
"glVertexAttribs2fvNV\0"
"glVertexAttribs2hvNV\0"
"glVertexAttribs2svNV\0"
"glVertexAttribs3dvNV\0"
"glVertexAttribs3fvNV\0"
"glVertexAttribs3hvNV\0"
"glVertexAttribs3svNV\0"
"glVertexAttribs4dvNV\0"
"glVertexAttribs4fvNV\0"
"glVertexAttribs4hvNV\0"
"glVertexAttribs4svNV\0"
"glVertexAttribs4ubvNV\0"
"glVertexBlendARB\0"
"glVertexBlendEnvfATI\0"
"glVertexBlendEnviATI\0"
"glVertexPointer\0"
"glVertexPointerEXT\0"
"glVertexPointerListIBM\0"
"glVertexPointervINTEL\0"
"glVertexStream1dATI\0"
"glVertexStream1dvATI\0"
"glVertexStream1fATI\0"
"glVertexStream1fvATI\0"
"glVertexStream1iATI\0"
"glVertexStream1ivATI\0"
"glVertexStream1sATI\0"
"glVertexStream1svATI\0"
"glVertexStream2dATI\0"
"glVertexStream2dvATI\0"
"glVertexStream2fATI\0"
"glVertexStream2fvATI\0"
"glVertexStream2iATI\0"
"glVertexStream2ivATI\0"
"glVertexStream2sATI\0"
"glVertexStream2svATI\0"
"glVertexStream3dATI\0"
"glVertexStream3dvATI\0"
"glVertexStream3fATI\0"
"glVertexStream3fvATI\0"
"glVertexStream3iATI\0"
"glVertexStream3ivATI\0"
"glVertexStream3sATI\0"
"glVertexStream3svATI\0"
"glVertexStream4dATI\0"
"glVertexStream4dvATI\0"
"glVertexStream4fATI\0"
"glVertexStream4fvATI\0"
"glVertexStream4iATI\0"
"glVertexStream4ivATI\0"
"glVertexStream4sATI\0"
"glVertexStream4svATI\0"
"glVertexWeightfEXT\0"
"glVertexWeightfvEXT\0"
"glVertexWeighthNV\0"
"glVertexWeighthvNV\0"
"glVertexWeightPointerEXT\0"
"glViewport\0"
"glWeightbvARB\0"
"glWeightdvARB\0"
"glWeightfvARB\0"
"glWeightivARB\0"
"glWeightPointerARB\0"
"glWeightsvARB\0"
"glWeightubvARB\0"
"glWeightuivARB\0"
"glWeightusvARB\0"
"glWindowBackBufferHintAutodesk\0"
"glWindowPos2d\0"
"glWindowPos2dARB\0"
"glWindowPos2dMESA\0"
"glWindowPos2dv\0"
"glWindowPos2dvARB\0"
"glWindowPos2dvMESA\0"
"glWindowPos2f\0"
"glWindowPos2fARB\0"
"glWindowPos2fMESA\0"
"glWindowPos2fv\0"
"glWindowPos2fvARB\0"
"glWindowPos2fvMESA\0"
"glWindowPos2i\0"
"glWindowPos2iARB\0"
"glWindowPos2iMESA\0"
"glWindowPos2iv\0"
"glWindowPos2ivARB\0"
"glWindowPos2ivMESA\0"
"glWindowPos2s\0"
"glWindowPos2sARB\0"
"glWindowPos2sMESA\0"
"glWindowPos2sv\0"
"glWindowPos2svARB\0"
"glWindowPos2svMESA\0"
"glWindowPos3d\0"
"glWindowPos3dARB\0"
"glWindowPos3dMESA\0"
"glWindowPos3dv\0"
"glWindowPos3dvARB\0"
"glWindowPos3dvMESA\0"
"glWindowPos3f\0"
"glWindowPos3fARB\0"
"glWindowPos3fMESA\0"
"glWindowPos3fv\0"
"glWindowPos3fvARB\0"
"glWindowPos3fvMESA\0"
"glWindowPos3i\0"
"glWindowPos3iARB\0"
"glWindowPos3iMESA\0"
"glWindowPos3iv\0"
"glWindowPos3ivARB\0"
"glWindowPos3ivMESA\0"
"glWindowPos3s\0"
"glWindowPos3sARB\0"
"glWindowPos3sMESA\0"
"glWindowPos3sv\0"
"glWindowPos3svARB\0"
"glWindowPos3svMESA\0"
"glWindowPos4dMESA\0"
"glWindowPos4dvMESA\0"
"glWindowPos4fMESA\0"
"glWindowPos4fvMESA\0"
"glWindowPos4iMESA\0"
"glWindowPos4ivMESA\0"
"glWindowPos4sMESA\0"
"glWindowPos4svMESA\0"
"glWriteMaskEXT\0"
"glXAllocateMemoryMESA\0"
"glXAllocateMemoryNV\0"
"glXBindChannelToWindowSGIX\0"
"glXBindHyperpipeSGIX\0"
"glXBindSwapBarrierNV\0"
"glXBindSwapBarrierSGIX\0"
"glXBindTexImageEXT\0"
"glXBindVideoImageNV\0"
"glXChannelRectSGIX\0"
"glXChannelRectSyncSGIX\0"
"glXChooseFBConfig\0"
"glXChooseVisual\0"
"glXCopyContext\0"
"glXCopySubBufferMESA\0"
"glXCreateContext\0"
"glXCreateGLXPixmap\0"
"glXCreateGLXPixmapMESA\0"
"glXCreateGLXPixmapWithConfigSGIX\0"
"glXCreateNewContext\0"
"glXCreatePixmap\0"
"glXCreateWindow\0"
"glXCushionSGI\0"
"glXDestroyContext\0"
"glXDestroyGLXPixmap\0"
"glXDestroyHyperpipeConfigSGIX\0"
"glXDestroyPixmap\0"
"glXDestroyWindow\0"
"glXDrawableAttribARB\0"
"glXDrawableAttribATI\0"
"glXFreeContextEXT\0"
"glXFreeMemoryMESA\0"
"glXFreeMemoryNV\0"
"glXGetAGPOffsetMESA\0"
"glXGetClientString\0"
"glXGetConfig\0"
"glXGetContextIDEXT\0"
"glXGetCurrentContext\0"
"glXGetCurrentDisplay\0"
"glXGetCurrentDisplayEXT\0"
"glXGetCurrentDrawable\0"
"glXGetCurrentReadDrawable\0"
"glXGetCurrentReadDrawableSGI\0"
"glXGetDriverConfig\0"
"glXGetFBConfigAttrib\0"
"glXGetFBConfigFromVisualSGIX\0"
"glXGetFBConfigs\0"
"glXGetMemoryOffsetMESA\0"
"glXGetMscRateOML\0"
"glXGetProcAddress\0"
"glXGetProcAddressARB\0"
"glXGetRefreshRateSGI\0"
"glXGetScreenDriver\0"
"glXGetSelectedEvent\0"
"glXGetSelectedEventSGIX\0"
"glXGetSyncValuesOML\0"
"glXGetTransparentIndexSUN\0"
"glXGetVideoDeviceNV\0"
"glXGetVideoInfoNV\0"
"glXGetVideoResizeSUN\0"
"glXGetVideoSyncSGI\0"
"glXGetVisualFromFBConfig\0"
"glXGetVisualFromFBConfigSGIX\0"
"glXHyperpipeAttribSGIX\0"
"glXHyperpipeConfigSGIX\0"
"glXImportContextEXT\0"
"glXIsDirect\0"
"glXJoinSwapGroupNV\0"
"glXJoinSwapGroupSGIX\0"
"glXMakeContextCurrent\0"
"glXMakeCurrent\0"
"glXMakeCurrentReadSGI\0"
"glXQueryChannelDeltasSGIX\0"
"glXQueryChannelRectSGIX\0"
"glXQueryContext\0"
"glXQueryContextInfoEXT\0"
"glXQueryDrawable\0"
"glXQueryExtension\0"
"glXQueryExtensionsString\0"
"glXQueryFrameCountNV\0"
"glXQueryHyperpipeAttribSGIX\0"
"glXQueryHyperpipeBestAttribSGIX\0"
"glXQueryHyperpipeConfigSGIX\0"
"glXQueryHyperpipeNetworkSGIX\0"
"glXQueryMaxSwapBarriersSGIX\0"
"glXQueryMaxSwapGroupsNV\0"
"glXQueryServerString\0"
"glXQuerySwapGroupNV\0"
"glXQueryVersion\0"
"glXReleaseBuffersMESA\0"
"glXReleaseTexImageEXT\0"
"glXReleaseVideoDeviceNV\0"
"glXReleaseVideoImageNV\0"
"glXResetFrameCountNV\0"
"glXSelectEvent\0"
"glXSelectEventSGIX\0"
"glXSendPbufferToVideoNV\0"
"glXSet3DfxModeMESA\0"
"glXSwapBuffers\0"
"glXSwapBuffersMscOML\0"
"glXSwapIntervalSGI\0"
"glXUseXFont\0"
"glXVideoResizeSUN\0"
"glXWaitForMscOML\0"
"glXWaitForSbcOML\0"
"glXWaitGL\0"
"glXWaitVideoSyncSGI\0"
"glXWaitX\0"
"\0"
};

typedef struct
{
  unsigned int hash;
  char* name;
  __GLXextFuncPtr func;
  int found_on_client_side;
  int found_on_server_side;
  int alreadyAsked;
} AssocProcAdress;

#ifdef PROVIDE_STUB_IMPLEMENTATION
static void _glStubImplementation()
{
  log_gl("This function is a stub !!!\n");
}
#endif

__GLXextFuncPtr glXGetProcAddress_no_lock(const GLubyte * _name)
{
  static int nbElts = 0;
  static int tabSize = 0;
  static AssocProcAdress* tab_assoc = NULL;
  static void* handle = NULL;
  __GLXextFuncPtr ret = NULL;

  const char* name = (const char*)_name;
  if(debug_gl) log_gl("looking for \"%s\",\n", name);
  int i;

  if (name == NULL)
  {
    goto end_of_glx_get_proc_address;
  }

  if (tabSize == 0)
  {
    tabSize = 2000;
    tab_assoc = calloc(tabSize, sizeof(AssocProcAdress));

    handle = dlopen(getenv("REAL_LIBGL") ? getenv("REAL_LIBGL") : "libGL.so.1.2" ,RTLD_LAZY);
    if (!handle) {
      log_gl("%s\n", dlerror());
      exit(1);
    }

    {
      if(debug_gl) log_gl("global_glXGetProcAddress request\n");
      int sizeOfString = 0;
      int nbRequestElts = 0;
      int i = 0;
      while(1)
      {
        if (global_glXGetProcAddress_request[i] == '\0')
        {
          nbRequestElts++;
          if (global_glXGetProcAddress_request[i + 1] == '\0')
          {
            sizeOfString = i + 1;
            break;
          }
        }
        i++;
      }
      if(debug_gl) log_gl("nbRequestElts=%d\n", nbRequestElts);
      char* result = (char*)malloc(nbRequestElts);

      long args[] = { INT_TO_ARG(nbRequestElts), POINTER_TO_ARG(global_glXGetProcAddress_request), POINTER_TO_ARG(result) };
      int args_size[] = { 0, sizeOfString, nbRequestElts };
      do_opengl_call_no_lock(glXGetProcAddress_global_fake_func, NULL, CHECK_ARGS(args, args_size));
      int offset = 0;
      for(i=0; i<nbRequestElts;i++)
      {
        const char* funcName = global_glXGetProcAddress_request + offset;
        void* func = dlsym(handle, funcName);
#ifdef PROVIDE_STUB_IMPLEMENTATION
        if (func == NULL)
          func = _glStubImplementation;
#endif
        if (result[i] && func == NULL && debug_gl)
          log_gl("%s %d %d\n", funcName, result[i], func != NULL);
        if (result[i] == 0)
        {
          if (nbElts < tabSize)
          {
            int hash = str_hash(funcName);
            tab_assoc[nbElts].alreadyAsked = 0;
            tab_assoc[nbElts].found_on_server_side = 0;
            tab_assoc[nbElts].found_on_client_side = func != NULL;
            tab_assoc[nbElts].hash = hash;
            tab_assoc[nbElts].name = strdup(funcName);
            tab_assoc[nbElts].func = NULL;
            nbElts ++;
          }
        }
        else
        {
          if (nbElts < tabSize)
          {
            int hash = str_hash(funcName);
            tab_assoc[nbElts].alreadyAsked = 0;
            tab_assoc[nbElts].found_on_server_side = 1;
            tab_assoc[nbElts].found_on_client_side = func != NULL;
            tab_assoc[nbElts].hash = hash;
            tab_assoc[nbElts].name = strdup(funcName);
            tab_assoc[nbElts].func = func;
            nbElts ++;
          }
        }
        offset += strlen(funcName) + 1;
      }
      free(result);

      ret = glXGetProcAddress_no_lock(_name);
      goto end_of_glx_get_proc_address;
    }
  }

  int hash = str_hash(name);
  for(i=0;i<nbElts;i++)
  {
    if (tab_assoc[i].hash == hash && strcmp(tab_assoc[i].name, name) == 0)
    {
      if (tab_assoc[i].alreadyAsked == 0)
      {
        tab_assoc[i].alreadyAsked = 1;
        if (tab_assoc[i].found_on_server_side == 0 && tab_assoc[i].found_on_client_side == 0)
        {
          log_gl("not found name on server and client side = %s\n", name);
        }
        else if (tab_assoc[i].found_on_server_side == 0)
        {
          log_gl("not found name on server side = %s\n", name);
        }
        else if (tab_assoc[i].found_on_client_side == 0)
        {
          log_gl("not found name on client side = %s\n", name);
        }
      }
      ret = (__GLXextFuncPtr)tab_assoc[i].func;
      goto end_of_glx_get_proc_address;
    }
  }
  if(1/*debug_gl*/) log_gl("looking for \"%s\",\n", name);
  int ret_call = 0;
  long args[] = { INT_TO_ARG(name) };
  do_opengl_call_no_lock(glXGetProcAddress_fake_func, &ret_call, args, NULL);
  void* func = dlsym(handle, name);
#ifdef PROVIDE_STUB_IMPLEMENTATION
  if (func == NULL)
    func = _glStubImplementation;
#endif
  if (ret_call == 0)
  {
    if (nbElts < tabSize)
    {
      tab_assoc[nbElts].alreadyAsked = 1;
      tab_assoc[nbElts].found_on_server_side = 0;
      tab_assoc[nbElts].found_on_client_side = func != NULL;
      tab_assoc[nbElts].hash = hash;
      tab_assoc[nbElts].name = strdup(name);
      tab_assoc[nbElts].func = NULL;
      nbElts ++;
    }
    if (func == NULL)
    {
      log_gl("not found name on server and client side = %s\n", name);
    }
    else
    {
      log_gl("not found name on server side = %s\n", name);
    }
    ret = NULL;
    goto end_of_glx_get_proc_address;
  }
  else
  {
    if (func == NULL)
    {
      log_gl("not found name on client side = %s\n", name);
    }
    if (nbElts < tabSize)
    {
      tab_assoc[nbElts].alreadyAsked = 1;
      tab_assoc[nbElts].found_on_server_side = 1;
      tab_assoc[nbElts].found_on_client_side = func != NULL;
      tab_assoc[nbElts].hash = hash;
      tab_assoc[nbElts].name = strdup(name);
      tab_assoc[nbElts].func = func;
      nbElts ++;
    }
    ret = func;
    goto end_of_glx_get_proc_address;
  }
end_of_glx_get_proc_address:
  return ret;
}

__GLXextFuncPtr glXGetProcAddress(const GLubyte * name)
{
  __GLXextFuncPtr ret;
  LOCK(glXGetProcAddress_fake_func);
  ret = glXGetProcAddress_no_lock(name);
  UNLOCK(glXGetProcAddress_fake_func);
  return ret;
}

__GLXextFuncPtr glXGetProcAddressARB (const GLubyte * name)
{
  return glXGetProcAddress(name);
}

