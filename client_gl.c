#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "opengl_func.h"
#include "common.h"
#include "lock.h"
#include "call.h"

#include "client_stub.c"

#include "glgetv_cst.h"

#include "log.h"

GLAPI void APIENTRY glPushAttrib(GLbitfield mask)
{
  GET_CURRENT_STATE();
  long args[] = { UNSIGNED_INT_TO_ARG(mask)};
  if (state->server_sp < MAX_SERVER_STATE_STACK_SIZE)
  {
    state->server_stack[state->server_sp].mask = mask;
    if (mask & GL_ENABLE_BIT)
    {
      state->server_stack[state->server_sp].linesmoothEnabled = state->current_server_state.linesmoothEnabled;
      state->server_stack[state->server_sp].lightingEnabled = state->current_server_state.lightingEnabled;
      state->server_stack[state->server_sp].texture2DEnabled = state->current_server_state.texture2DEnabled;
      state->server_stack[state->server_sp].blendEnabled = state->current_server_state.blendEnabled;
    }
    if (mask & GL_TRANSFORM_BIT)
    {
      state->server_stack[state->server_sp].matrixMode = state->current_server_state.matrixMode;
    }
    if (mask & GL_DEPTH_BUFFER_BIT)
    {
      state->server_stack[state->server_sp].depthFunc = state->current_server_state.depthFunc;
    }
    if (mask & GL_TEXTURE_BIT)
    {
      state->server_stack[state->server_sp].bindTexture2D = state->current_server_state.bindTexture2D;
      state->server_stack[state->server_sp].bindTextureRectangle = state->current_server_state.bindTextureRectangle;
    }
    if (mask & GL_FOG_BIT)
    {
      state->server_stack[state->server_sp].fog = state->current_server_state.fog;
    }
    state->server_sp++;
  }
  else
  {
    log_gl("server_sp > MAX_SERVER_STATE_STACK_SIZE\n");
  }
  do_opengl_call(glPushAttrib_func, NULL, args, NULL);
}

GLAPI void APIENTRY glPopAttrib()
{
  GET_CURRENT_STATE();
  if (state->server_sp > 0)
  {
    state->server_sp--;
    if (state->server_stack[state->server_sp].mask & GL_ENABLE_BIT)
    {
      state->current_server_state.linesmoothEnabled = state->server_stack[state->server_sp].linesmoothEnabled;
      state->current_server_state.lightingEnabled = state->server_stack[state->server_sp].lightingEnabled;
      state->current_server_state.texture2DEnabled = state->server_stack[state->server_sp].texture2DEnabled;
      state->current_server_state.blendEnabled = state->server_stack[state->server_sp].blendEnabled;
    }
    if (state->server_stack[state->server_sp].mask & GL_TRANSFORM_BIT)
    {
      state->current_server_state.matrixMode = state->server_stack[state->server_sp].matrixMode;
    }
    if (state->server_stack[state->server_sp].mask & GL_DEPTH_BUFFER_BIT)
    {
      state->current_server_state.depthFunc = state->server_stack[state->server_sp].depthFunc;
    }
    if (state->server_stack[state->server_sp].mask & GL_TEXTURE_BIT)
    {
      state->current_server_state.bindTexture2D = state->server_stack[state->server_sp].bindTexture2D;
      state->current_server_state.bindTextureRectangle = state->server_stack[state->server_sp].bindTextureRectangle;
    }
    if (state->server_stack[state->server_sp].mask & GL_FOG_BIT)
    {
      state->current_server_state.fog = state->server_stack[state->server_sp].fog;
    }
  }
  else
  {
    log_gl("server_sp <= 0\n");
  }
  do_opengl_call(glPopAttrib_func, NULL, NULL, NULL);
}

GLAPI GLboolean APIENTRY glIsEnabled( GLenum cap )
{
  GLboolean ret = 0;
  if (debug_gl) log_gl("glIsEnabled(0x%X)\n", cap);
  GET_CURRENT_STATE();
  if (!disable_optim)
  {
    if (cap == GL_LINE_SMOOTH)
    {
      return state->current_server_state.linesmoothEnabled;
    }
    else if (cap == GL_LIGHTING)
    {
      return state->current_server_state.lightingEnabled;
    }
    else if (cap == GL_TEXTURE_2D)
    {
      return state->current_server_state.texture2DEnabled;
    }
    else if (cap == GL_BLEND)
    {
      return state->current_server_state.blendEnabled;
    }
    else if (cap == GL_SCISSOR_TEST)
    {
      return state->current_server_state.scissorTestEnabled;
    }
    else if (cap == GL_VERTEX_PROGRAM_NV)
    {
      return state->current_server_state.vertexProgramEnabled;
    }
    else if (cap == GL_FOG)
    {
      return state->current_server_state.fogEnabled;
    }
  }
  long args[] = { INT_TO_ARG(cap) };
  do_opengl_call(glIsEnabled_func, &ret, args, NULL);
  if (debug_gl) log_gl("glIsEnabled(0x%X) = %d\n", cap, ret);
  return ret;
}

static ClientArray* _getArray(GLenum cap, int verbose)
{
  GET_CURRENT_STATE();
  switch(cap)
  {
    case GL_VERTEX_ARRAY: return &state->client_state.arrays.vertexArray;
    case GL_COLOR_ARRAY: return &state->client_state.arrays.colorArray;
    case GL_SECONDARY_COLOR_ARRAY: return &state->client_state.arrays.secondaryColorArray;
    case GL_NORMAL_ARRAY: return &state->client_state.arrays.normalArray;
    case GL_INDEX_ARRAY: return &state->client_state.arrays.indexArray;
    case GL_TEXTURE_COORD_ARRAY:
      return &state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture];
    case GL_EDGE_FLAG_ARRAY: return &state->client_state.arrays.edgeFlagArray;
    case GL_WEIGHT_ARRAY_ARB: return &state->client_state.arrays.weightArray;
    case GL_MATRIX_INDEX_ARRAY_POINTER_ARB: return &state->client_state.arrays.matrixIndexArray;
    case GL_FOG_COORD_ARRAY: return &state->client_state.arrays.fogCoordArray;
    case GL_ELEMENT_ARRAY_ATI: return &state->client_state.arrays.elementArrayATI;
    default:
      if (cap >= GL_VERTEX_ATTRIB_ARRAY0_NV && cap < GL_VERTEX_ATTRIB_ARRAY0_NV + MY_GL_MAX_VERTEX_ATTRIBS_NV)
        return &state->client_state.arrays.vertexAttribArrayNV[cap - GL_VERTEX_ATTRIB_ARRAY0_NV];
      if (verbose) log_gl("unhandled cap : %d\n", cap); return NULL;
  }
}

GLAPI void APIENTRY glEnable(GLenum cap)
{
  GET_CURRENT_STATE();
  /* Strange but some programs use glEnable(GL_VERTEX_ARRAY)
     instead of glEnableClientState(GL_VERTEX_ARRAY) ...
     I've discovered that while trying to make the spheres demo of chromium running
     and mesa confirms it too */
  if (_getArray(cap, 0))
  {
    glEnableClientState(cap);
    return;
  }

  if (cap == GL_LINE_SMOOTH)
  {
    state->current_server_state.linesmoothEnabled = 1;
  }
  else if (cap == GL_LIGHTING)
  {
    state->current_server_state.lightingEnabled = 1;
  }
  else if (cap == GL_TEXTURE_2D)
  {
    state->current_server_state.texture2DEnabled = 1;
  }
  else if (cap == GL_BLEND)
  {
    state->current_server_state.blendEnabled = 1;
  }
  else if (cap == GL_SCISSOR_TEST)
  {
    state->current_server_state.scissorTestEnabled = 1;
  }
  else if (cap == GL_VERTEX_PROGRAM_NV)
  {
    state->current_server_state.vertexProgramEnabled = 1;
  }
  else if (cap == GL_FOG)
  {
    state->current_server_state.fogEnabled = 1;
  }
  long args[] = { INT_TO_ARG(cap)};
  do_opengl_call(glEnable_func, NULL, args, NULL);
}

GLAPI void APIENTRY glDisable(GLenum cap)
{
  GET_CURRENT_STATE();
  if (_getArray(cap, 0))
  {
    glDisableClientState(cap);
    return;
  }

  if (cap == GL_LINE_SMOOTH)
  {
    state->current_server_state.linesmoothEnabled = 0;
  }
  else if (cap == GL_LIGHTING)
  {
    state->current_server_state.lightingEnabled = 0;
  }
  else if (cap == GL_TEXTURE_2D)
  {
    state->current_server_state.texture2DEnabled = 0;
  }
  else if (cap == GL_BLEND)
  {
    state->current_server_state.blendEnabled = 0;
  }
  else if (cap == GL_SCISSOR_TEST)
  {
    state->current_server_state.scissorTestEnabled = 0;
  }
  else if (cap == GL_VERTEX_PROGRAM_NV)
  {
    state->current_server_state.vertexProgramEnabled = 0;
  }
  else if (cap == GL_FOG)
  {
    state->current_server_state.fogEnabled = 0;
  }
  long args[] = { INT_TO_ARG(cap)};
  do_opengl_call(glDisable_func, NULL, args, NULL);
}

#define GET_CAP_NAME(x) case x: return #x;
static const char* _getArrayName(GLenum cap)
{
  switch (cap)
  {
    GET_CAP_NAME(GL_VERTEX_ARRAY);
    GET_CAP_NAME(GL_COLOR_ARRAY);
    GET_CAP_NAME(GL_SECONDARY_COLOR_ARRAY);
    GET_CAP_NAME(GL_NORMAL_ARRAY);
    GET_CAP_NAME(GL_INDEX_ARRAY);
    GET_CAP_NAME(GL_TEXTURE_COORD_ARRAY);
    GET_CAP_NAME(GL_EDGE_FLAG_ARRAY);
    GET_CAP_NAME(GL_WEIGHT_ARRAY_ARB);
    GET_CAP_NAME(GL_MATRIX_INDEX_ARRAY_ARB);
    GET_CAP_NAME(GL_FOG_COORD_ARRAY);
    GET_CAP_NAME(GL_ELEMENT_ARRAY_ATI);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY0_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY1_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY2_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY3_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY4_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY5_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY6_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY7_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY8_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY9_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY10_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY11_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY12_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY13_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY14_NV);
    GET_CAP_NAME(GL_VERTEX_ATTRIB_ARRAY15_NV);
  }
  return "unknown";
}

GLAPI void APIENTRY EXT_FUNC(glEnableVertexAttribArrayARB)(GLuint index)
{
  CHECK_PROC(glEnableVertexAttribArrayARB);
  GET_CURRENT_STATE();
  long args[] = { index };
  do_opengl_call(glEnableVertexAttribArrayARB_func, NULL, args, NULL);

  if (index < MY_GL_MAX_VERTEX_ATTRIBS_ARB)
  {
    state->client_state.arrays.vertexAttribArray[index].enabled = 1;
  }
  else
  {
    log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
  }
}

GLAPI void APIENTRY EXT_FUNC(glEnableVertexAttribArray)(GLuint index)
{
  CHECK_PROC(glEnableVertexAttribArray);
  GET_CURRENT_STATE();
  long args[] = { index };
  do_opengl_call(glEnableVertexAttribArray_func, NULL, args, NULL);

  if (index < MY_GL_MAX_VERTEX_ATTRIBS_ARB)
  {
    state->client_state.arrays.vertexAttribArray[index].enabled = 1;
  }
  else
  {
    log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
  }
}

GLAPI void APIENTRY EXT_FUNC(glDisableVertexAttribArrayARB)(GLuint index)
{
  CHECK_PROC(glDisableVertexAttribArrayARB);
  GET_CURRENT_STATE();
  long args[] = { index };
  do_opengl_call(glDisableVertexAttribArrayARB_func, NULL, args, NULL);

  if (index < MY_GL_MAX_VERTEX_ATTRIBS_ARB)
  {
    state->client_state.arrays.vertexAttribArray[index].enabled = 0;
  }
  else
  {
    log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
  }
}

GLAPI void APIENTRY EXT_FUNC(glDisableVertexAttribArray)(GLuint index)
{
  CHECK_PROC(glDisableVertexAttribArray);
  GET_CURRENT_STATE();
  long args[] = { index };
  do_opengl_call(glDisableVertexAttribArray_func, NULL, args, NULL);

  if (index < MY_GL_MAX_VERTEX_ATTRIBS_ARB)
  {
    state->client_state.arrays.vertexAttribArray[index].enabled = 0;
  }
  else
  {
    log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
  }
}

GLAPI void APIENTRY glEnableClientState(GLenum cap)
{
  if (cap == GL_VERTEX_ARRAY_RANGE_NV ||
      cap == GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV) return; /* FIXME */
  GET_CURRENT_STATE();
  ClientArray* array = _getArray(cap, 1);
  if (array == NULL) return;
  if (debug_array_ptr)
  {
    if (cap == GL_TEXTURE_COORD_ARRAY)
      log_gl("enable texture %d\n", state->client_state.clientActiveTexture);
    else
      log_gl("enable feature %s\n", _getArrayName(cap));
  }
  if ((!disable_optim) && array->enabled)
  {
    //log_gl("discard useless command\n");
    return;
  }
  array->enabled = 1;
  /*if ((!disable_optim) && cap == GL_TEXTURE_COORD_ARRAY)
  {
    long args[] = { UNSIGNED_INT_TO_ARG(state->client_state.clientActiveTexture + GL_TEXTURE0_ARB)};
    do_opengl_call(glClientActiveTexture_func, NULL, args, NULL);
  }*/
  long args[] = { UNSIGNED_INT_TO_ARG(cap)};
  do_opengl_call(glEnableClientState_func, NULL, args, NULL);
}

GLAPI void APIENTRY glDisableClientState(GLenum cap)
{
  if (cap == GL_VERTEX_ARRAY_RANGE_NV ||
      cap == GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV) return; /* FIXME */
  GET_CURRENT_STATE();
  ClientArray* array = _getArray(cap, 1);
  if (array == NULL) return;
  if (debug_array_ptr)
  {
    if (cap == GL_TEXTURE_COORD_ARRAY)
      log_gl("disable texture %d\n", state->client_state.clientActiveTexture);
    else
      log_gl("disable feature %s\n", _getArrayName(cap));
  }

  if ((!disable_optim) && array->enabled == 0)
  {
    //log_gl("discard useless command\n");
    return;
  }
  array->enabled = 0;
  /*if ((!disable_optim) && cap == GL_TEXTURE_COORD_ARRAY)
  {
    long args[] = { UNSIGNED_INT_TO_ARG(state->client_state.clientActiveTexture + GL_TEXTURE0_ARB)};
    do_opengl_call(glClientActiveTexture_func, NULL, args, NULL);
  }*/
  long args[] = { UNSIGNED_INT_TO_ARG(cap)};
  do_opengl_call(glDisableClientState_func, NULL, args, NULL);
}


GLAPI void APIENTRY glClientActiveTexture(GLenum texture)
{
  GET_CURRENT_STATE();

  if (disable_optim || state->client_state.clientActiveTexture != (int)texture - GL_TEXTURE0_ARB)
  {
    long args[] = { UNSIGNED_INT_TO_ARG(texture)};
    do_opengl_call(glClientActiveTexture_func, NULL, args, NULL);
  }

  state->client_state.clientActiveTexture = (int)texture - GL_TEXTURE0_ARB;

  assert(state->client_state.clientActiveTexture >= 0 &&
         state->client_state.clientActiveTexture < NB_MAX_TEXTURES);
}

GLAPI void APIENTRY glClientActiveTextureARB(GLenum texture)
{
  glClientActiveTexture(texture);
}

GLAPI void APIENTRY glActiveTextureARB(GLenum texture)
{
  GET_CURRENT_STATE();
  if (disable_optim || state->activeTexture != texture)
  {
    state->activeTexture = texture;

    long args[] = { INT_TO_ARG(texture) };
    do_opengl_call(glActiveTextureARB_func, NULL, args, NULL);
  }
}

GLAPI void APIENTRY glPushClientAttrib(GLbitfield mask)
{
  GET_CURRENT_STATE();
  long args[] = { UNSIGNED_INT_TO_ARG(mask)};
  if (state->client_state_sp < MAX_CLIENT_STATE_STACK_SIZE)
  {
    state->client_state_stack[state->client_state_sp].mask = mask;
    if (mask & GL_CLIENT_VERTEX_ARRAY_BIT)
    {
      state->client_state_stack[state->client_state_sp].arrays = state->client_state.arrays;
    }
    if (mask & GL_CLIENT_PIXEL_STORE_BIT)
    {
      state->client_state_stack[state->client_state_sp].pack = state->client_state.pack;
      state->client_state_stack[state->client_state_sp].unpack = state->client_state.unpack;
    }
    state->client_state_sp++;
  }
  else
  {
    log_gl("state->client_state_sp > MAX_CLIENT_STATE_STACK_SIZE\n");
  }
  do_opengl_call(glPushClientAttrib_func, NULL, args, NULL);
}

GLAPI void APIENTRY glPopClientAttrib()
{
  GET_CURRENT_STATE();
  if (state->client_state_sp > 0)
  {
    state->client_state_sp--;
    if (state->client_state_stack[state->client_state_sp].mask & GL_CLIENT_VERTEX_ARRAY_BIT)
    {
      state->client_state.arrays = state->client_state_stack[state->client_state_sp].arrays;
    }
    if (state->client_state_stack[state->client_state_sp].mask & GL_CLIENT_PIXEL_STORE_BIT)
    {
      state->client_state.pack = state->client_state_stack[state->client_state_sp].pack;
      state->client_state.unpack = state->client_state_stack[state->client_state_sp].unpack;
    }
  }
  else
  {
    log_gl("state->client_state_sp <= 0\n");
  }
  do_opengl_call(glPopClientAttrib_func, NULL, NULL, NULL);
}

static void _glPixelStore(GLenum pname, GLint param)
{
  GET_CURRENT_STATE();
  switch (pname)
  {
    case GL_PACK_SWAP_BYTES: state->client_state.pack.swapEndian = param != 0; break;
    case GL_PACK_LSB_FIRST: state->client_state.pack.lsbFirst = param != 0; break;
    case GL_PACK_ROW_LENGTH: if (param >= 0) state->client_state.pack.rowLength = param; break;
    case GL_PACK_IMAGE_HEIGHT: if (param >= 0) state->client_state.pack.imageHeight = param; break;
    case GL_PACK_SKIP_ROWS: if (param >= 0) state->client_state.pack.skipRows = param; break;
    case GL_PACK_SKIP_PIXELS: if (param >= 0) state->client_state.pack.skipPixels = param; break;
    case GL_PACK_SKIP_IMAGES: if (param >= 0) state->client_state.pack.skipImages = param; break;
    case GL_PACK_ALIGNMENT: if (param == 1 || param == 2 || param == 4 || param == 8)
                                state->client_state.pack.alignment = param; break;
    case GL_UNPACK_SWAP_BYTES: state->client_state.unpack.swapEndian = param != 0; break;
    case GL_UNPACK_LSB_FIRST: state->client_state.unpack.lsbFirst = param != 0; break;
    case GL_UNPACK_ROW_LENGTH: if (param >= 0) state->client_state.unpack.rowLength = param; break;
    case GL_UNPACK_IMAGE_HEIGHT: if (param >= 0) state->client_state.unpack.imageHeight = param; break;
    case GL_UNPACK_SKIP_ROWS: if (param >= 0) state->client_state.unpack.skipRows = param; break;
    case GL_UNPACK_SKIP_PIXELS: if (param >= 0) state->client_state.unpack.skipPixels = param; break;
    case GL_UNPACK_SKIP_IMAGES: if (param >= 0) state->client_state.unpack.skipImages = param; break;
    case GL_UNPACK_ALIGNMENT: if (param == 1 || param == 2 || param == 4 || param == 8)
                                  state->client_state.unpack.alignment = param; break;
    default: log_gl("unhandled pname %d\n", pname); break;
  }
}

GLAPI void APIENTRY glPixelStoref(GLenum pname, GLfloat param)
{
  long args[] = { UNSIGNED_INT_TO_ARG(pname), FLOAT_TO_ARG(param)};
  if (!(pname == GL_PACK_SKIP_PIXELS || pname == GL_PACK_SKIP_PIXELS || pname == GL_PACK_SKIP_IMAGES ||
        pname == GL_UNPACK_SKIP_PIXELS || pname == GL_UNPACK_SKIP_PIXELS || pname == GL_UNPACK_SKIP_IMAGES))
  {
    _glPixelStore(pname, (GLint)(param + 0.5));
    do_opengl_call(glPixelStoref_func, NULL, args, NULL);
  }
}

void glPixelStorei_no_lock(GLenum pname, GLint param)
{
  _glPixelStore(pname, param);
  if (!(pname == GL_PACK_SKIP_PIXELS || pname == GL_PACK_SKIP_ROWS || pname == GL_PACK_SKIP_IMAGES ||
        pname == GL_UNPACK_SKIP_PIXELS || pname == GL_UNPACK_SKIP_ROWS || pname == GL_UNPACK_SKIP_IMAGES))
  {
    long args[] = { UNSIGNED_INT_TO_ARG(pname), INT_TO_ARG(param)};
    do_opengl_call_no_lock(glPixelStorei_func, NULL, args, NULL);
  }
}

GLAPI void APIENTRY glPixelStorei(GLenum pname, GLint param)
{
  LOCK(glPixelStorei_func);
  glPixelStorei_no_lock(pname, param);
  UNLOCK(glPixelStorei_func);
}

static int get_nb_composants_of_gl_get_constant_compare(const void* a, const void* b)
{
  GlGetConstant* constantA = (GlGetConstant*)a;
  GlGetConstant* constantB = (GlGetConstant*)b;
  return constantA->token - constantB->token;
}

static int get_size_get_boolean_integer_float_double_v(int func_number, int pname)
{
  GlGetConstant constant;
  GlGetConstant* found;
  constant.token = pname;
  found = bsearch(&constant, gl_get_constants,
                  sizeof(gl_get_constants) / sizeof(GlGetConstant), sizeof(GlGetConstant),
                  get_nb_composants_of_gl_get_constant_compare);
  if (found)
    return found->count;
  else
  {
    log_gl("unknown name for %s : %d\n", tab_opengl_calls_name[func_number], pname);
    log_gl("hoping that size is 1...\n");
    return 1;
  }
}

#define AFFECT_N(x, y, N) do { int _i; for(_i=0;_i<N;_i++) x[_i]=y[_i]; } while(0)

#define IS_VALID_MATRIX_MODE(mode) \
     ((mode >= GL_MODELVIEW && mode <= GL_TEXTURE) || \
      (mode == GL_MATRIX_PALETTE_ARB) || \
      (mode == GL_MODELVIEW1_ARB) || \
      (mode >= GL_MODELVIEW2_ARB && mode <= GL_MODELVIEW31_ARB))

#define MATRIX_MODE_TO_MATRIX_INDEX(mode) \
     ((mode == GL_MODELVIEW) ? 0 : \
      (mode == GL_PROJECTION) ? 1 : \
      (mode == GL_TEXTURE) ? 2 + state->activeTexture - GL_TEXTURE0_ARB: \
      (mode == GL_MATRIX_PALETTE_ARB) ? 2 + NB_MAX_TEXTURES : \
      (mode == GL_MODELVIEW1_ARB) ? 3 + NB_MAX_TEXTURES : \
      (mode >= GL_MODELVIEW2_ARB && mode <= GL_MODELVIEW31_ARB) ? 4 + NB_MAX_TEXTURES + mode - GL_MODELVIEW2_ARB : -1)

static int maxTextureSize = -1; /* optimization for ppracer */
static int maxTextureUnits = -1;

static int _glGetIntegerv(GLenum pname)
{
  int ret;
  long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(&ret) };
  int args_size[] = { 0, sizeof(int) };
  do_opengl_call_no_lock(glGetIntegerv_func, NULL, CHECK_ARGS(args, args_size));
  return ret;
}

void glGetIntegerv_no_lock( GLenum pname, GLint *params );

#define glGetf(funcName, funcNumber, cType, typeBase) \
void CONCAT(funcName,_no_lock)( GLenum pname, cType *params ) \
{ \
  GET_CURRENT_STATE(); \
  switch (pname) \
  { \
    case GL_VERTEX_ARRAY: *params = state->client_state.arrays.vertexArray.enabled; break; \
    case GL_NORMAL_ARRAY: *params = state->client_state.arrays.normalArray.enabled; break; \
    case GL_COLOR_ARRAY: *params = state->client_state.arrays.colorArray.enabled; break; \
    case GL_INDEX_ARRAY: *params = state->client_state.arrays.indexArray.enabled; break; \
    case GL_TEXTURE_COORD_ARRAY: *params = state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].enabled; break; \
    case GL_EDGE_FLAG_ARRAY: *params = state->client_state.arrays.edgeFlagArray.enabled; break; \
    case GL_SECONDARY_COLOR_ARRAY: *params = state->client_state.arrays.secondaryColorArray.enabled; break; \
    case GL_FOG_COORDINATE_ARRAY: *params = state->client_state.arrays.fogCoordArray.enabled; break; \
    case GL_WEIGHT_ARRAY_ARB: *params = state->client_state.arrays.weightArray.enabled; break; \
    case GL_VERTEX_ARRAY_SIZE: *params = state->client_state.arrays.vertexArray.size; break; \
    case GL_VERTEX_ARRAY_TYPE: *params = state->client_state.arrays.vertexArray.type; break; \
    case GL_VERTEX_ARRAY_STRIDE: *params = state->client_state.arrays.vertexArray.stride; break; \
    case GL_NORMAL_ARRAY_TYPE: *params = state->client_state.arrays.normalArray.type; break; \
    case GL_NORMAL_ARRAY_STRIDE: *params = state->client_state.arrays.normalArray.stride; break; \
    case GL_COLOR_ARRAY_SIZE: *params = state->client_state.arrays.colorArray.size; break; \
    case GL_COLOR_ARRAY_TYPE: *params = state->client_state.arrays.colorArray.type; break; \
    case GL_COLOR_ARRAY_STRIDE: *params = state->client_state.arrays.colorArray.stride; break; \
    case GL_INDEX_ARRAY_TYPE: *params = state->client_state.arrays.indexArray.type; break; \
    case GL_INDEX_ARRAY_STRIDE: *params = state->client_state.arrays.indexArray.stride; break; \
    case GL_TEXTURE_COORD_ARRAY_SIZE: *params = state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].size; break; \
    case GL_TEXTURE_COORD_ARRAY_TYPE: *params = state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].type; break; \
    case GL_TEXTURE_COORD_ARRAY_STRIDE: *params = state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].stride; break; \
    case GL_EDGE_FLAG_ARRAY_STRIDE: *params = state->client_state.arrays.edgeFlagArray.stride; break; \
    case GL_SECONDARY_COLOR_ARRAY_SIZE: *params = state->client_state.arrays.secondaryColorArray.size; break; \
    case GL_SECONDARY_COLOR_ARRAY_TYPE: *params = state->client_state.arrays.secondaryColorArray.type; break; \
    case GL_SECONDARY_COLOR_ARRAY_STRIDE: *params = state->client_state.arrays.secondaryColorArray.stride; break; \
    case GL_FOG_COORDINATE_ARRAY_TYPE: *params = state->client_state.arrays.fogCoordArray.type; break; \
    case GL_FOG_COORDINATE_ARRAY_POINTER: *params = state->client_state.arrays.fogCoordArray.stride; break; \
    case GL_WEIGHT_ARRAY_SIZE_ARB: *params = state->client_state.arrays.weightArray.size; break; \
    case GL_WEIGHT_ARRAY_TYPE_ARB: *params = state->client_state.arrays.weightArray.type; break; \
    case GL_WEIGHT_ARRAY_STRIDE_ARB: *params = state->client_state.arrays.weightArray.stride; break; \
    case GL_MATRIX_INDEX_ARRAY_SIZE_ARB: *params = state->client_state.arrays.matrixIndexArray.size; break; \
    case GL_MATRIX_INDEX_ARRAY_TYPE_ARB: *params = state->client_state.arrays.matrixIndexArray.type; break; \
    case GL_MATRIX_INDEX_ARRAY_STRIDE_ARB: *params = state->client_state.arrays.matrixIndexArray.stride; break; \
    case GL_VERTEX_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.vertexArray.vbo_name; break; \
    case GL_NORMAL_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.normalArray.vbo_name; break; \
    case GL_COLOR_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.colorArray.vbo_name; break; \
    case GL_INDEX_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.indexArray.vbo_name; break; \
    case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].vbo_name; break; \
    case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.edgeFlagArray.vbo_name; break; \
    case GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.secondaryColorArray.vbo_name; break; \
    case GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.fogCoordArray.vbo_name; break; \
    case GL_WEIGHT_ARRAY_BUFFER_BINDING: *params = state->client_state.arrays.weightArray.vbo_name; break; \
    case GL_PACK_SWAP_BYTES: *params = state->client_state.pack.swapEndian; break; \
    case GL_PACK_LSB_FIRST: *params = state->client_state.pack.lsbFirst; break; \
    case GL_PACK_ROW_LENGTH: *params = state->client_state.pack.rowLength; break; \
    case GL_PACK_IMAGE_HEIGHT: *params = state->client_state.pack.imageHeight; break; \
    case GL_PACK_SKIP_ROWS: *params = state->client_state.pack.skipRows; break; \
    case GL_PACK_SKIP_PIXELS: *params = state->client_state.pack.skipPixels; break; \
    case GL_PACK_SKIP_IMAGES: *params = state->client_state.pack.skipImages; break; \
    case GL_PACK_ALIGNMENT: *params = state->client_state.pack.alignment; break; \
    case GL_UNPACK_SWAP_BYTES: *params = state->client_state.unpack.swapEndian; break; \
    case GL_UNPACK_LSB_FIRST: *params = state->client_state.unpack.lsbFirst; break; \
    case GL_UNPACK_ROW_LENGTH: *params = state->client_state.unpack.rowLength; break; \
    case GL_UNPACK_IMAGE_HEIGHT: *params = state->client_state.unpack.imageHeight; break; \
    case GL_UNPACK_SKIP_ROWS: *params = state->client_state.unpack.skipRows; break; \
    case GL_UNPACK_SKIP_PIXELS: *params = state->client_state.unpack.skipPixels; break; \
    case GL_UNPACK_SKIP_IMAGES: *params = state->client_state.unpack.skipImages; break; \
    case GL_UNPACK_ALIGNMENT: *params = state->client_state.unpack.alignment; break; \
    case GL_TEXTURE_2D_BINDING_EXT: *params = state->current_server_state.bindTexture2D; break; \
    case GL_TEXTURE_BINDING_RECTANGLE_ARB: *params = state->current_server_state.bindTextureRectangle; break; \
    case GL_VIEWPORT: params[0] = state->viewport.x; params[1] = state->viewport.y; params[2] = state->viewport.width; params[3] = state->viewport.height; break; \
    case GL_SCISSOR_BOX: params[0] = state->scissorbox.x; params[1] = state->scissorbox.y; params[2] = state->scissorbox.width; params[3] = state->scissorbox.height; break; \
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: *params = 16; break;\
    case GL_MATRIX_MODE: *params = state->current_server_state.matrixMode; break; \
    case GL_DEPTH_FUNC: *params = state->current_server_state.depthFunc; break; \
    case GL_FOG_MODE: *params = state->current_server_state.fog.mode; break; \
    case GL_FOG_DENSITY: *params = state->current_server_state.fog.density; break; \
    case GL_FOG_START: *params = state->current_server_state.fog.start; break; \
    case GL_FOG_END: *params = state->current_server_state.fog.end; break; \
    case GL_FOG_INDEX: *params = state->current_server_state.fog.index; break; \
    case GL_FOG_COLOR: AFFECT_N(params, state->current_server_state.fog.color, 4); break; \
    case GL_COMPRESSED_TEXTURE_FORMATS_ARB: \
    { \
      long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(params) }; \
      int args_size[] = { 0, 0 }; \
      int nb_compressed_texture_formats = 0; \
      glGetIntegerv_no_lock(GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB, &nb_compressed_texture_formats); \
      args_size[1] = tab_args_type_length[typeBase] * nb_compressed_texture_formats; \
      do_opengl_call_no_lock(funcNumber, NULL, CHECK_ARGS(args, args_size)); \
      break; \
    } \
    case GL_ARRAY_BUFFER_BINDING: *params = state->arrayBuffer; break; \
    case GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB: *params = state->elementArrayBuffer; break; \
    case GL_MAX_TEXTURE_SIZE: if (maxTextureSize == -1) maxTextureSize = _glGetIntegerv(pname); *params = maxTextureSize; break; \
    case GL_MAX_TEXTURE_UNITS_ARB: if (maxTextureUnits == -1) maxTextureUnits = _glGetIntegerv(pname); *params = maxTextureUnits; break; \
    case GL_MODELVIEW_MATRIX: \
    case GL_PROJECTION_MATRIX: \
    case GL_TEXTURE_MATRIX: AFFECT_N(params, state->matrix[pname - GL_MODELVIEW_MATRIX].current.val, 16); break; \
    case GL_CURRENT_RASTER_POSITION: \
    { \
      if (!state->currentRasterPosKnown) \
      { \
        long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(state->currentRasterPos) }; \
        int args_size[] = { 0, 4 * sizeof(float) }; \
        if(debug_gl) log_gl("getting value 0x%X\n", pname); \
        do_opengl_call_no_lock(glGetFloatv_func, NULL, CHECK_ARGS(args, args_size)); \
        state->currentRasterPosKnown = 1; \
      } \
      AFFECT_N(params, state->currentRasterPos, 4); \
      break; \
    } \
    default: \
    { \
      long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(params) }; \
      int args_size[] = { 0, 0 }; \
      args_size[1] = tab_args_type_length[typeBase] * get_size_get_boolean_integer_float_double_v(funcNumber, pname); \
      if(debug_gl) log_gl("getting value 0x%X\n", pname); \
      do_opengl_call_no_lock(funcNumber, NULL, CHECK_ARGS(args, args_size)); \
      if (typeBase == TYPE_INT) log_gl("val=%d\n", (int)*params); \
      else if (typeBase == TYPE_FLOAT) log_gl("val=%f\n", (float)*params); \
    } \
  } \
} \
GLAPI void APIENTRY funcName( GLenum pname, cType *params ) \
{ \
  LOCK(funcNumber); \
  CONCAT(funcName,_no_lock)(pname, params); \
  UNLOCK(funcNumber); \
}

glGetf(glGetBooleanv, glGetBooleanv_func, GLboolean, TYPE_CHAR);
glGetf(glGetIntegerv, glGetIntegerv_func, GLint, TYPE_INT);
glGetf(glGetFloatv, glGetFloatv_func, GLfloat, TYPE_FLOAT);
glGetf(glGetDoublev, glGetDoublev_func, GLdouble, TYPE_DOUBLE);

GLAPI void APIENTRY glDepthFunc(GLenum func)
{
  long args[] = { UNSIGNED_INT_TO_ARG(func)};
  GET_CURRENT_STATE();
  state->current_server_state.depthFunc = func;
  do_opengl_call(glDepthFunc_func, NULL, args, NULL);
}

GLAPI void APIENTRY glClipPlane(GLenum plane, const GLdouble * equation)
{
  GET_CURRENT_STATE();
  if (plane >= GL_CLIP_PLANE0 && plane < GL_CLIP_PLANE0 + N_CLIP_PLANES)
    memcpy(state->current_server_state.clipPlanes[plane-GL_CLIP_PLANE0], equation, 4 * sizeof(double));
  long args[] = { UNSIGNED_INT_TO_ARG(plane), POINTER_TO_ARG(equation)};
  do_opengl_call(glClipPlane_func, NULL, args, NULL);
}

GLAPI void APIENTRY glGetClipPlane(GLenum plane, GLdouble * equation)
{
  GET_CURRENT_STATE();
  if (plane >= GL_CLIP_PLANE0 && plane < GL_CLIP_PLANE0 + N_CLIP_PLANES)
  {
    memcpy(equation, state->current_server_state.clipPlanes[plane-GL_CLIP_PLANE0], 4 * sizeof(double));
    return;
  }
  long args[] = { UNSIGNED_INT_TO_ARG(plane), POINTER_TO_ARG(equation)};
  do_opengl_call(glGetClipPlane_func, NULL, args, NULL);
}


GLAPI void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
  GET_CURRENT_STATE();
  long args[] = { INT_TO_ARG(x), INT_TO_ARG(y), INT_TO_ARG(width), INT_TO_ARG(height)};
  state->viewport.x = x;
  state->viewport.y = y;
  state->viewport.width = width;
  state->viewport.height = height;
  if (debug_gl) log_gl("viewport %d,%d,%d,%d\n", x, y, width, height);
  do_opengl_call(glViewport_func, NULL, args, NULL);
}

GLAPI void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
  GET_CURRENT_STATE();
  long args[] = { INT_TO_ARG(x), INT_TO_ARG(y), INT_TO_ARG(width), INT_TO_ARG(height)};
  state->scissorbox.x = x;
  state->scissorbox.y = y;
  state->scissorbox.width = width;
  state->scissorbox.height = height;
  do_opengl_call(glScissor_func, NULL, args, NULL);
}


/* Matrix optimization : openquartz */
#if 1
GLAPI void APIENTRY glMatrixMode(GLenum mode)
{
  GET_CURRENT_STATE();
  long args[] = { UNSIGNED_INT_TO_ARG(mode)};
  if (IS_VALID_MATRIX_MODE(mode))
    state->current_server_state.matrixMode = mode;
  do_opengl_call(glMatrixMode_func, NULL, args, NULL);
}

GLAPI void APIENTRY glPushMatrix()
{
  GET_CURRENT_STATE();
  int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
  if (index_mode >= 0)
  {
    if (state->matrix[index_mode].sp < MAX_MATRIX_STACK_SIZE)
    {
      memcpy(state->matrix[index_mode].stack[state->matrix[index_mode].sp].val,
            state->matrix[index_mode].current.val,
            16 * sizeof(double));
      state->matrix[index_mode].sp++;
    }
    else
    {
      log_gl("matrix[mode].sp >= MAX_MATRIX_STACK_SIZE\n");
    }
  }

  do_opengl_call(glPushMatrix_func, NULL, NULL, NULL);
}

GLAPI void APIENTRY glPopMatrix()
{
  GET_CURRENT_STATE();
  int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
  if (index_mode >= 0)
  {
    if (state->matrix[index_mode].sp > 0)
    {
      state->matrix[index_mode].sp--;
      memcpy(state->matrix[index_mode].current.val,
            state->matrix[index_mode].stack[state->matrix[index_mode].sp].val,
            16 * sizeof(double));
    }
    else
    {
      log_gl("matrix[mode].sp <= 0\n");
    }
  }

  do_opengl_call(glPopMatrix_func, NULL, NULL, NULL);
}

GLAPI void APIENTRY glLoadIdentity()
{
  GET_CURRENT_STATE();
  int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
  int j;
  if (index_mode >= 0)
  {
    for(j=0;j<16;j++)
    {
      state->matrix[index_mode].current.val[j] = (j == 0 || j == 5 || j == 10 || j == 15);
    }
  }
  do_opengl_call(glLoadIdentity_func, NULL, NULL, NULL);
}

static void _internal_glLoadMatrixd(const GLdouble matrix[16])
{
  GET_CURRENT_STATE();
  int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
  if (index_mode >= 0)
    memcpy(state->matrix[index_mode].current.val, matrix, 16 * sizeof(double));
}

static void _internal_glLoadMatrixf(const GLfloat matrix[16])
{
  GET_CURRENT_STATE();
  int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
  if (index_mode >= 0)
  {
    int i;
    for(i=0;i<16;i++)
      state->matrix[index_mode].current.val[i] = matrix[i];
  }
}

GLAPI void APIENTRY glLoadMatrixd(const GLdouble matrix[16])
{
  _internal_glLoadMatrixd(matrix);
  long args[] = { POINTER_TO_ARG(matrix)};
  do_opengl_call(glLoadMatrixd_func, NULL, args, NULL);
}

static void matrixfToMatrixd(const GLfloat matrixf[16], GLdouble matrix[16])
{
  int i;
  for(i=0;i<16;i++)
  {
    matrix[i] = matrixf[i];
  }
}

GLAPI void APIENTRY glLoadMatrixf(const GLfloat matrix[16])
{
  _internal_glLoadMatrixf(matrix);

  long args[] = { POINTER_TO_ARG(matrix)};
  do_opengl_call(glLoadMatrixf_func, NULL, args, NULL);
}

static void _internal_glMultMatrixd(const GLdouble matrix[16])
{
  GET_CURRENT_STATE();
  GLdouble destMatrix[16];
  int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
  if (index_mode >= 0)
  {
    GLdouble* oriMatrix = state->matrix[index_mode].current.val;

    /* t(C)=t(A.B)=t(B).t(A) */

    destMatrix[0*4+0] = matrix[0*4+0] * oriMatrix[0*4+0] +
                        matrix[0*4+1] * oriMatrix[1*4+0] +
                        matrix[0*4+2] * oriMatrix[2*4+0] +
                        matrix[0*4+3] * oriMatrix[3*4+0];
    destMatrix[0*4+1] = matrix[0*4+0] * oriMatrix[0*4+1] +
                        matrix[0*4+1] * oriMatrix[1*4+1] +
                        matrix[0*4+2] * oriMatrix[2*4+1] +
                        matrix[0*4+3] * oriMatrix[3*4+1];
    destMatrix[0*4+2] = matrix[0*4+0] * oriMatrix[0*4+2] +
                        matrix[0*4+1] * oriMatrix[1*4+2] +
                        matrix[0*4+2] * oriMatrix[2*4+2] +
                        matrix[0*4+3] * oriMatrix[3*4+2];
    destMatrix[0*4+3] = matrix[0*4+0] * oriMatrix[0*4+3] +
                        matrix[0*4+1] * oriMatrix[1*4+3] +
                        matrix[0*4+2] * oriMatrix[2*4+3] +
                        matrix[0*4+3] * oriMatrix[3*4+3];

    destMatrix[1*4+0] = matrix[1*4+0] * oriMatrix[0*4+0] +
                        matrix[1*4+1] * oriMatrix[1*4+0] +
                        matrix[1*4+2] * oriMatrix[2*4+0] +
                        matrix[1*4+3] * oriMatrix[3*4+0];
    destMatrix[1*4+1] = matrix[1*4+0] * oriMatrix[0*4+1] +
                        matrix[1*4+1] * oriMatrix[1*4+1] +
                        matrix[1*4+2] * oriMatrix[2*4+1] +
                        matrix[1*4+3] * oriMatrix[3*4+1];
    destMatrix[1*4+2] = matrix[1*4+0] * oriMatrix[0*4+2] +
                        matrix[1*4+1] * oriMatrix[1*4+2] +
                        matrix[1*4+2] * oriMatrix[2*4+2] +
                        matrix[1*4+3] * oriMatrix[3*4+2];
    destMatrix[1*4+3] = matrix[1*4+0] * oriMatrix[0*4+3] +
                        matrix[1*4+1] * oriMatrix[1*4+3] +
                        matrix[1*4+2] * oriMatrix[2*4+3] +
                        matrix[1*4+3] * oriMatrix[3*4+3];

    destMatrix[2*4+0] = matrix[2*4+0] * oriMatrix[0*4+0] +
                        matrix[2*4+1] * oriMatrix[1*4+0] +
                        matrix[2*4+2] * oriMatrix[2*4+0] +
                        matrix[2*4+3] * oriMatrix[3*4+0];
    destMatrix[2*4+1] = matrix[2*4+0] * oriMatrix[0*4+1] +
                        matrix[2*4+1] * oriMatrix[1*4+1] +
                        matrix[2*4+2] * oriMatrix[2*4+1] +
                        matrix[2*4+3] * oriMatrix[3*4+1];
    destMatrix[2*4+2] = matrix[2*4+0] * oriMatrix[0*4+2] +
                        matrix[2*4+1] * oriMatrix[1*4+2] +
                        matrix[2*4+2] * oriMatrix[2*4+2] +
                        matrix[2*4+3] * oriMatrix[3*4+2];
    destMatrix[2*4+3] = matrix[2*4+0] * oriMatrix[0*4+3] +
                        matrix[2*4+1] * oriMatrix[1*4+3] +
                        matrix[2*4+2] * oriMatrix[2*4+3] +
                        matrix[2*4+3] * oriMatrix[3*4+3];

    destMatrix[3*4+0] = matrix[3*4+0] * oriMatrix[0*4+0] +
                        matrix[3*4+1] * oriMatrix[1*4+0] +
                        matrix[3*4+2] * oriMatrix[2*4+0] +
                        matrix[3*4+3] * oriMatrix[3*4+0];
    destMatrix[3*4+1] = matrix[3*4+0] * oriMatrix[0*4+1] +
                        matrix[3*4+1] * oriMatrix[1*4+1] +
                        matrix[3*4+2] * oriMatrix[2*4+1] +
                        matrix[3*4+3] * oriMatrix[3*4+1];
    destMatrix[3*4+2] = matrix[3*4+0] * oriMatrix[0*4+2] +
                        matrix[3*4+1] * oriMatrix[1*4+2] +
                        matrix[3*4+2] * oriMatrix[2*4+2] +
                        matrix[3*4+3] * oriMatrix[3*4+2];
    destMatrix[3*4+3] = matrix[3*4+0] * oriMatrix[0*4+3] +
                        matrix[3*4+1] * oriMatrix[1*4+3] +
                        matrix[3*4+2] * oriMatrix[2*4+3] +
                        matrix[3*4+3] * oriMatrix[3*4+3];

    memcpy(oriMatrix, destMatrix, 16 * sizeof(double));
  }
}

GLAPI void APIENTRY glMultMatrixd(const GLdouble matrix[16])
{
  _internal_glMultMatrixd(matrix);

  long args[] = { POINTER_TO_ARG(matrix)};
  do_opengl_call(glMultMatrixd_func, NULL, args, NULL);
}

GLAPI void APIENTRY glMultMatrixf(const GLfloat matrix[16])
{
  GLdouble matrixd[16];
  matrixfToMatrixd(matrix, matrixd);
  _internal_glMultMatrixd(matrixd);

  long args[] = { POINTER_TO_ARG(matrix)};
  do_opengl_call(glMultMatrixf_func, NULL, args, NULL);
}

/**
 * Transpose a GLfloat matrix.
 *
 * \param to destination array.
 * \param from source array.
 */
static void
_math_transposef( GLfloat to[16], const GLfloat from[16] )
{
   to[0] = from[0];
   to[1] = from[4];
   to[2] = from[8];
   to[3] = from[12];
   to[4] = from[1];
   to[5] = from[5];
   to[6] = from[9];
   to[7] = from[13];
   to[8] = from[2];
   to[9] = from[6];
   to[10] = from[10];
   to[11] = from[14];
   to[12] = from[3];
   to[13] = from[7];
   to[14] = from[11];
   to[15] = from[15];
}


/**
 * Transpose a GLdouble matrix.
 *
 * \param to destination array.
 * \param from source array.
 */
static void
_math_transposed( GLdouble to[16], const GLdouble from[16] )
{
   to[0] = from[0];
   to[1] = from[4];
   to[2] = from[8];
   to[3] = from[12];
   to[4] = from[1];
   to[5] = from[5];
   to[6] = from[9];
   to[7] = from[13];
   to[8] = from[2];
   to[9] = from[6];
   to[10] = from[10];
   to[11] = from[14];
   to[12] = from[3];
   to[13] = from[7];
   to[14] = from[11];
   to[15] = from[15];
}

GLAPI void APIENTRY glLoadTransposeMatrixf (const GLfloat m[16])
{
  GLfloat dest[16];
  _math_transposef(dest, m);
  glLoadMatrixf(dest);
}

GLAPI void APIENTRY glLoadTransposeMatrixd (const GLdouble m[16])
{
  GLdouble dest[16];
  _math_transposed(dest, m);
  glLoadMatrixd(dest);
}

GLAPI void APIENTRY glMultTransposeMatrixf (const GLfloat m[16])
{
  GLfloat dest[16];
  _math_transposef(dest, m);
  glMultMatrixf(dest);
}

GLAPI void APIENTRY glMultTransposeMatrixd (const GLdouble m[16])
{
  GLdouble dest[16];
  _math_transposed(dest, m);
  glMultMatrixd(dest);
}

GLAPI void APIENTRY glLoadTransposeMatrixfARB (const GLfloat* m)
{
  GLfloat dest[16];
  _math_transposef(dest, m);
  glLoadMatrixf(dest);
}

GLAPI void APIENTRY glLoadTransposeMatrixdARB (const GLdouble* m)
{
  GLdouble dest[16];
  _math_transposed(dest, m);
  glLoadMatrixd(dest);
}

GLAPI void APIENTRY glMultTransposeMatrixfARB (const GLfloat* m)
{
  GLfloat dest[16];
  _math_transposef(dest, m);
  glMultMatrixf(dest);
}

GLAPI void APIENTRY glMultTransposeMatrixdARB (const GLdouble* m)
{
  GLdouble dest[16];
  _math_transposed(dest, m);
  glMultMatrixd(dest);
}

GLAPI void APIENTRY glOrtho( GLdouble left,
              GLdouble right,
              GLdouble bottom,
              GLdouble top,
              GLdouble near_val,
              GLdouble far_val)
{
  double tx, ty, tz;
  tx = -(right + left) / (right - left);
  ty = -(top + bottom) / (top - bottom);
  tz = -(far_val + near_val)   / (far_val - near_val);
  double matrix[16] = { 2/(right - left),      0,            0,          0,
                            0,       2/(top-bottom),        0,          0,
                            0,            0,  -2/(far_val - near_val),   0,
                            tx,            ty,               tz,         1 };
  _internal_glMultMatrixd(matrix);

  long args[] = { DOUBLE_TO_ARG(left), DOUBLE_TO_ARG(right), DOUBLE_TO_ARG(bottom), DOUBLE_TO_ARG(top), DOUBLE_TO_ARG(near_val), DOUBLE_TO_ARG(far_val)};
  do_opengl_call(glOrtho_func, NULL, args, NULL);
}

GLAPI void APIENTRY glFrustum( GLdouble left,
                GLdouble right,
                GLdouble bottom,
                GLdouble top,
                GLdouble near_val,
                GLdouble far_val)
{
  double V1, V2, A, B, C, D;
  V1 = 2 * near_val / (right - left);
  V2 = 2 * near_val / (top - bottom);
  A = (right + left) / (right - left);
  B = (top + bottom) / (top - bottom);
  C = -(far_val + near_val)   / (far_val - near_val);
  D = -2 * far_val * near_val / (far_val - near_val);
  double matrix[16] = { V1, 0, 0, 0,
                        0, V2, 0, 0,
                        A,  B, C, -1,
                        0,  0, D, 0};
  _internal_glMultMatrixd(matrix);

  long args[] = { DOUBLE_TO_ARG(left), DOUBLE_TO_ARG(right), DOUBLE_TO_ARG(bottom), DOUBLE_TO_ARG(top), DOUBLE_TO_ARG(near_val), DOUBLE_TO_ARG(far_val)};
  do_opengl_call(glFrustum_func, NULL, args, NULL);

}

static void _glRotate_internal(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
  double c = cos(angle / 180. * M_PI);
  double s = sin(angle / 180. * M_PI);
  if (x == 1 && y == 0 && z == 0)
  {
    GET_CURRENT_STATE();
    int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
    if (index_mode >= 0)
    {
      GLdouble* matrix = state->matrix[index_mode].current.val;
      double t, u;

      t = matrix[1*4+0];
      u = matrix[2*4+0];
      matrix[1*4+0] = c * t + s * u;
      matrix[2*4+0] = c * u - s * t;

      t = matrix[1*4+1];
      u = matrix[2*4+1];
      matrix[1*4+1] = c * t + s * u;
      matrix[2*4+1] = c * u - s * t;

      t = matrix[1*4+2];
      u = matrix[2*4+2];
      matrix[1*4+2] = c * t + s * u;
      matrix[2*4+2] = c * u - s * t;

      t = matrix[1*4+3];
      u = matrix[2*4+3];
      matrix[1*4+3] = c * t + s * u;
      matrix[2*4+3] = c * u - s * t;
    }
  }
  else if (x == 0 && y == 1 && z == 0)
  {
    GET_CURRENT_STATE();
    int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
    if (index_mode >= 0)
    {
      GLdouble* matrix = state->matrix[index_mode].current.val;
      double t, u;

      t = matrix[0*4+0];
      u = matrix[2*4+0];
      matrix[0*4+0] = c * t - s * u;
      matrix[2*4+0] = s * t + c * u;

      t = matrix[0*4+1];
      u = matrix[2*4+1];
      matrix[0*4+1] = c * t - s * u;
      matrix[2*4+1] = s * t + c * u;

      t = matrix[0*4+2];
      u = matrix[2*4+2];
      matrix[0*4+2] = c * t - s * u;
      matrix[2*4+2] = s * t + c * u;

      t = matrix[0*4+3];
      u = matrix[2*4+3];
      matrix[0*4+3] = c * t - s * u;
      matrix[2*4+3] = s * t + c * u;
    }
  }
  else if (x == 0 && y == 0 && z == 1)
  {
    GET_CURRENT_STATE();
    int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
    if (index_mode >= 0)
    {
      GLdouble* matrix = state->matrix[index_mode].current.val;
      double t, u;

      t = matrix[0*4+0];
      u = matrix[1*4+0];
      matrix[0*4+0] = c * t + s * u;
      matrix[1*4+0] = c * u - s * t;

      t = matrix[0*4+1];
      u = matrix[1*4+1];
      matrix[0*4+1] = c * t + s * u;
      matrix[1*4+1] = c * u - s * t;

      t = matrix[0*4+2];
      u = matrix[1*4+2];
      matrix[0*4+2] = c * t + s * u;
      matrix[1*4+2] = c * u - s * t;

      t = matrix[0*4+3];
      u = matrix[1*4+3];
      matrix[0*4+3] = c * t + s * u;
      matrix[1*4+3] = c * u - s * t;
    }
  }
  else
  {
    double sqrt_sum_sqr = sqrt(x*x+y*y+z*z);
    if (sqrt_sum_sqr < 1e-4) return;
    x /= sqrt_sum_sqr;
    y /= sqrt_sum_sqr;
    z /= sqrt_sum_sqr;
    double matrix[16] = { x*x*(1-c)+c, y*x*(1-c)+z*s, x*z*(1-c)-y*s, 0,
                          x*y*(1-c)-z*s, y*y*(1-c)+c, y*z*(1-c)+x*s, 0,
                          x*z*(1-c)+y*s, y*z*(1-c)-x*s, z*z*(1-c)+c, 0,
                          0, 0, 0, 1};
    _internal_glMultMatrixd(matrix);
  }
}

GLAPI void APIENTRY glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
  _glRotate_internal(angle, x, y, z);

  long args[] = { DOUBLE_TO_ARG(angle), DOUBLE_TO_ARG(x), DOUBLE_TO_ARG(y), DOUBLE_TO_ARG(z)};
  do_opengl_call(glRotated_func, NULL, args, NULL);
}

GLAPI void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
  _glRotate_internal(angle, x, y, z);

  long args[] = { FLOAT_TO_ARG(angle), FLOAT_TO_ARG(x), FLOAT_TO_ARG(y), FLOAT_TO_ARG(z)};
  do_opengl_call(glRotatef_func, NULL, args, NULL);
}

static void _glScale_internal(double a, double b, double c)
{
  GET_CURRENT_STATE();
  int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
  if (index_mode >= 0)
  {
    GLdouble* matrix = state->matrix[index_mode].current.val;
    matrix[0*4+0] *= a;
    matrix[0*4+1] *= a;
    matrix[0*4+2] *= a;
    matrix[0*4+3] *= a;
    matrix[1*4+0] *= b;
    matrix[1*4+1] *= b;
    matrix[1*4+2] *= b;
    matrix[1*4+3] *= b;
    matrix[2*4+0] *= c;
    matrix[2*4+1] *= c;
    matrix[2*4+2] *= c;
    matrix[2*4+3] *= c;
  }
}

GLAPI void APIENTRY glScaled(GLdouble x, GLdouble y, GLdouble z)
{
  _glScale_internal(x, y, z);

  long args[] = { DOUBLE_TO_ARG(x), DOUBLE_TO_ARG(y), DOUBLE_TO_ARG(z)};
  do_opengl_call(glScaled_func, NULL, args, NULL);
}

GLAPI void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z)
{
  _glScale_internal(x, y, z);

  long args[] = { FLOAT_TO_ARG(x), FLOAT_TO_ARG(y), FLOAT_TO_ARG(z)};
  do_opengl_call(glScalef_func, NULL, args, NULL);
}

static void _glTranslate_internal(double a, double b, double c)
{
  GET_CURRENT_STATE();
  int index_mode = MATRIX_MODE_TO_MATRIX_INDEX(state->current_server_state.matrixMode);
  if (index_mode >= 0)
  {
    GLdouble* matrix = state->matrix[index_mode].current.val;

    matrix[3*4+0] += a * matrix[0*4+0] + b * matrix[1*4+0] + c * matrix[2*4+0];
    matrix[3*4+1] += a * matrix[0*4+1] + b * matrix[1*4+1] + c * matrix[2*4+1];
    matrix[3*4+2] += a * matrix[0*4+2] + b * matrix[1*4+2] + c * matrix[2*4+2];
    matrix[3*4+3] += a * matrix[0*4+3] + b * matrix[1*4+3] + c * matrix[2*4+3];
  }
}

GLAPI void APIENTRY glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
  _glTranslate_internal(x, y, z);

  long args[] = { DOUBLE_TO_ARG(x), DOUBLE_TO_ARG(y), DOUBLE_TO_ARG(z)};
  do_opengl_call(glTranslated_func, NULL, args, NULL);
}

GLAPI void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
   _glTranslate_internal(x, y, z);

  long args[] = { FLOAT_TO_ARG(x), FLOAT_TO_ARG(y), FLOAT_TO_ARG(z)};
  do_opengl_call(glTranslatef_func, NULL, args, NULL);
}
#endif
/* End of matrix optimization */

void glBindBufferARB_no_lock(GLenum target, GLuint buffer)
{
  CHECK_PROC(glBindBufferARB);
  GET_CURRENT_STATE();
  if (buffer >= 32768)
  {
    log_gl("buffer >= 32768\n");
    return;
  }
  long args[] = {INT_TO_ARG(target), INT_TO_ARG(buffer)};
  if (target == GL_ARRAY_BUFFER_ARB)
  {
    //log_gl("glBindBufferARB(GL_ARRAY_BUFFER,%d)\n", buffer);
    state->arrayBuffer = buffer;
  }
  else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB)
  {
    //log_gl("glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,%d)\n", buffer);
    state->elementArrayBuffer = buffer;
  }
  else if (target == GL_PIXEL_UNPACK_BUFFER_EXT)
  {
    //log_gl("glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT,%d)\n", buffer);
    state->pixelUnpackBuffer = buffer;
  }
  else if (target == GL_PIXEL_PACK_BUFFER_EXT)
  {
    //log_gl("glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT,%d)\n", buffer);
    state->pixelPackBuffer = buffer;
  }
  do_opengl_call_no_lock(glBindBufferARB_func, NULL, args, NULL);
}

GLAPI void APIENTRY EXT_FUNC(glBindBufferARB) (GLenum target, GLuint buffer)
{
  LOCK(glBindBufferARB_func);
  glBindBufferARB_no_lock(target, buffer);
  UNLOCK(glBindBufferARB_func);
}

GLAPI void APIENTRY EXT_FUNC(glBindBuffer) (GLenum target, GLuint buffer)
{
  glBindBufferARB(target, buffer);
}

GLAPI void APIENTRY glGenBuffersARB (GLsizei n, GLuint * tab)
{
  CHECK_PROC(glGenBuffersARB);
  GET_CURRENT_STATE();
  if (n <= 0) { log_gl("n <= 0\n"); return; }
  alloc_range(state->bufferAllocator, n, tab);
  long args[] = { INT_TO_ARG(n) };
  do_opengl_call(glGenBuffersARB_fake_func, NULL, args, NULL);
}

GLAPI void APIENTRY glGenBuffers (GLsizei n, GLuint * tab)
{
  glGenBuffersARB(n, tab);
}

GLAPI void APIENTRY glDeleteBuffersARB (GLsizei n, const GLuint * tab)
{
  CHECK_PROC(glDeleteBuffersARB);
  GET_CURRENT_STATE();
  if (n <= 0) { log_gl("n <= 0\n"); return; }
  delete_range(state->bufferAllocator, n, tab);
  long args[] = { INT_TO_ARG(n), POINTER_TO_ARG(tab) };
  do_opengl_call(glDeleteBuffersARB_func, NULL, args, NULL);
}

GLAPI void APIENTRY glDeleteBuffers(GLsizei n, const GLuint * tab)
{
  glDeleteBuffersARB(n, tab);
}

static Buffer* _get_buffer(GLenum target)
{
  GET_CURRENT_STATE();
  if (target == GL_ARRAY_BUFFER_ARB)
  {
    if (state->arrayBuffer)
      return &state->arrayBuffers[state->arrayBuffer];
    else
      return NULL;
  }
  else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB)
  {
    if (state->elementArrayBuffer)
      return &state->elementArrayBuffers[state->elementArrayBuffer];
    else
      return NULL;
  }
  else if (target == GL_PIXEL_UNPACK_BUFFER_EXT)
  {
    if (state->pixelUnpackBuffer)
      return &state->pixelUnpackBuffers[state->pixelUnpackBuffer];
    else
      return NULL;
  }
  else if (target == GL_PIXEL_PACK_BUFFER_EXT)
  {
    if (state->pixelPackBuffer)
      return &state->pixelPackBuffers[state->pixelPackBuffer];
    else
      return NULL;
  }
  else
  {
    return NULL;
  }
}

GLAPI GLenum APIENTRY glGetError()
{
  int ret = 0;
  if (disable_optim)
  {
    do_opengl_call(glGetError_func, &ret, NULL, NULL);
    log_gl("glGetError() = %d\n", ret);
  }
  else
    do_opengl_call(_glGetError_fake_func, NULL, NULL, NULL);
  return ret;
}

GLAPI void APIENTRY glBufferDataARB (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage)
{
  CHECK_PROC(glBufferDataARB);

  Buffer* buffer = _get_buffer(target);
  if (buffer)
  {
    buffer->usage = usage;
    buffer->size = size;
    buffer->ptr = realloc(buffer->ptr, size);
    if (data) memcpy(buffer->ptr, data, size);
  }
  else
  {
    fprintf(stderr, "unknown buffer/buffer target : %d\n", target);
  }
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(size), POINTER_TO_ARG(data), INT_TO_ARG(usage) };
  int args_size[] = { 0, 0, (data) ? size : 0, 0 };
  do_opengl_call(glBufferDataARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glBufferData (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage)
{
  glBufferDataARB(target, size, data, usage);
}

GLAPI void APIENTRY glBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data)
{
  CHECK_PROC(glBufferSubDataARB);

  //log_gl("glBufferSubDataARB %d %d\n", offset, size);

  Buffer* buffer = _get_buffer(target);
  if (buffer)
  {
    assert(offset + size <= buffer->size);
    assert(buffer->ptr);
    memcpy(buffer->ptr + offset, data, size);
  }
  else
  {
    fprintf(stderr, "unknown buffer/buffer target : %d\n", target);
  }
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(offset), INT_TO_ARG(size), POINTER_TO_ARG(data) };
  int args_size[] = { 0, 0, 0, size };
  do_opengl_call(glBufferSubDataARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glBufferSubData(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data)
{
  glBufferSubDataARB(target, offset, size, data);
}

GLAPI void APIENTRY glGetBufferSubDataARB (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data)
{
  CHECK_PROC(glGetBufferSubDataARB);

  Buffer* buffer = _get_buffer(target);
  if (!buffer) return;

  assert(offset + size <= buffer->size);
  assert(buffer->ptr);

  memcpy(data, buffer->ptr + offset, size);
}

GLAPI void APIENTRY glGetBufferSubData (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data)
{
  glGetBufferSubDataARB(target, offset, size, data);
}

GLvoid* glMapBufferARB (GLenum target, GLenum access)
{
  CHECK_PROC_WITH_RET(glMapBufferARB);

  Buffer* buffer = _get_buffer(target);
  if (!buffer) return NULL;
  if (target == GL_PIXEL_PACK_BUFFER_EXT && (access == GL_READ_ONLY || access == GL_READ_WRITE))
  {
    int ret_int = 0;
    long args[] = { INT_TO_ARG(target), INT_TO_ARG(buffer->size), POINTER_TO_ARG(buffer->ptr) };
    int args_size[] = { 0, 0, buffer->size };
    do_opengl_call(_glMapBufferARB_fake_func, &ret_int, CHECK_ARGS(args, args_size));
    if (ret_int == 0)
      return NULL;
  }
  buffer->access = access;
  buffer->mapped = 1;
  return buffer->ptr;
}

GLvoid* glMapBuffer(GLenum target, GLenum access)
{
  return glMapBufferARB(target, access);
}

GLAPI void APIENTRY glGetBufferParameterivARB (GLenum target, GLenum pname, GLint *params)
{
  CHECK_PROC(glGetBufferParameterivARB);

  Buffer* buffer = _get_buffer(target);
  if (!buffer) return;

  switch (pname)
  {
    case GL_BUFFER_SIZE_ARB: *params = buffer->size; break;
    case GL_BUFFER_USAGE_ARB: *params = buffer->usage; break;
    case GL_BUFFER_ACCESS_ARB: *params = buffer->access; break;
    case GL_BUFFER_MAPPED_ARB: *params = buffer->mapped; break;
    default:
      log_gl("unknown pname = 0x%X\n", pname);
  }
}

GLAPI void APIENTRY glGetBufferParameteriv (GLenum target, GLenum pname, GLint *params)
{
  glGetBufferParameterivARB(target, pname, params);
}

GLAPI void APIENTRY glGetBufferPointervARB (GLenum target, GLenum pname, GLvoid* *params)
{
  CHECK_PROC(glGetBufferPointervARB);
  if (pname != GL_BUFFER_MAP_POINTER_ARB)
  {
    log_gl("glGetBufferPointervARB : unknown buffer data pname : %x\n", pname);
    return;
  }
  Buffer* buffer = _get_buffer(target);
  if (!buffer) return;
  if (buffer->mapped) *params = buffer->ptr; else *params = NULL;
}

GLAPI void APIENTRY glGetBufferPointerv (GLenum target, GLenum pname, GLvoid* *params)
{
  glGetBufferPointervARB(target, pname, params);
}

GLAPI GLboolean APIENTRY glUnmapBufferARB(GLenum target)
{
  CHECK_PROC_WITH_RET(glUnmapBufferARB);
  Buffer* buffer = _get_buffer(target);
  if (!buffer) return 0;
  if (!buffer->mapped)
  {
    log_gl("unmapped buffer");
    return 0;
  }
  buffer->mapped = 0;
  if (buffer->access != GL_READ_ONLY)
  {
    glBufferSubDataARB(target, 0, buffer->size, buffer->ptr);
  }
  buffer->access = 0;
  return 1;
}

GLAPI GLboolean APIENTRY glUnmapBuffer(GLenum target)
{
  return glUnmapBufferARB(target);
}

GLAPI void GLAPIENTRY glNewList( GLuint list, GLenum mode )
{
  long args[] = { INT_TO_ARG(list), INT_TO_ARG(mode) };
  GET_CURRENT_STATE();
  alloc_value(state->listAllocator, list);
  do_opengl_call(glNewList_func, NULL, args, NULL);
}

GLAPI void GLAPIENTRY glDeleteLists( GLuint list, GLsizei range )
{
  long args[] = { INT_TO_ARG(list), INT_TO_ARG(range) };
  GET_CURRENT_STATE();
  delete_consecutive_values(state->listAllocator, list, range);
  do_opengl_call(glDeleteLists_func, NULL, args, NULL);
}

GLAPI GLuint GLAPIENTRY glGenLists( GLsizei range )
{
  GET_CURRENT_STATE();
  unsigned int firstValue = alloc_range(state->listAllocator, range, NULL);
  long args[] = { INT_TO_ARG(range) };
  do_opengl_call(glGenLists_fake_func, NULL, args, NULL);
  return firstValue;
}

GLAPI void APIENTRY glCallLists( GLsizei n,
                  GLenum type,
                  const GLvoid *lists )
{
  long args[] = { INT_TO_ARG(n), INT_TO_ARG(type), POINTER_TO_ARG(lists) };
  int args_size[] = { 0, 0, 0 };
  int size = n;
  if (n <= 0) { log_gl("n <= 0\n"); return; }
  switch(type)
  {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
      break;

    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_2_BYTES:
      size *= 2;
      break;

    case GL_3_BYTES:
      size *= 3;
      break;

    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
    case GL_4_BYTES:
      size *= 4;
      break;

    default:
      log_gl("unsupported type = %d\n", type);
      return;
  }
  args_size[2] = size;
  do_opengl_call(glCallLists_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI const GLubyte * APIENTRY glGetString( GLenum name )
{
  int i;
  static GLubyte* glStrings[6] = {NULL};
  static const char* glGetStringsName[] = {
  "GL_VENDOR",
  "GL_RENDERER",
  "GL_VERSION",
  "GL_EXTENSIONS",
  "GL_SHADING_LANGUAGE_VERSION",
  };

  if (name >= GL_VENDOR && name <= GL_EXTENSIONS)
    i = name - GL_VENDOR;
  else if (name == GL_SHADING_LANGUAGE_VERSION)
    i = 4;
  else if (name == GL_PROGRAM_ERROR_STRING_NV)
    i = 5;
  else
  {
    log_gl("assert(name >= GL_VENDOR && name <= GL_EXTENSIONS || name == GL_SHADING_LANGUAGE_VERSION  || name == GL_PROGRAM_ERROR_STRING_NV)\n");
    return NULL;
  }
  LOCK(glGetString_func);
  if (glStrings[i] == NULL)
  {
    if (i <= 4 && getenv(glGetStringsName[i]))
    {
      glStrings[i] = (GLubyte*)getenv(glGetStringsName[i]);
    }
    else
    {
      long args[] = { INT_TO_ARG(name) };
      do_opengl_call_no_lock(glGetString_func, &glStrings[i], args, NULL);
    }

    if(debug_gl) log_gl("glGetString(0x%X) = %s\n", name, glStrings[i]);
    glStrings[name - GL_VENDOR] = (GLubyte*)strdup((char *)glStrings[i]);
    if (name == GL_EXTENSIONS)
    {
      removeUnwantedExtensions((char *)glStrings[i]);
    }
  }
  UNLOCK(glGetString_func);
  return glStrings[i];
}

#define CASE_GL_PIXEL_MAP(x) case GL_PIXEL_MAP_##x: glGetIntegerv(CONCAT(GL_PIXEL_MAP_##x,_SIZE), &value); return value;

static int get_glgetpixelmapv_size(int map)
{
  int value;
  switch (map)
  {
    CASE_GL_PIXEL_MAP(I_TO_I);
    CASE_GL_PIXEL_MAP(S_TO_S);
    CASE_GL_PIXEL_MAP(I_TO_R);
    CASE_GL_PIXEL_MAP(I_TO_G);
    CASE_GL_PIXEL_MAP(I_TO_B);
    CASE_GL_PIXEL_MAP(I_TO_A);
    CASE_GL_PIXEL_MAP(R_TO_R);
    CASE_GL_PIXEL_MAP(G_TO_G);
    CASE_GL_PIXEL_MAP(B_TO_B);
    CASE_GL_PIXEL_MAP(A_TO_A);
    default :
    {
      log_gl("unhandled map = %d\n", map);
      return 0;
    }
  }
}

GLAPI void APIENTRY glGetPixelMapfv( GLenum map, GLfloat *values )
{
  long args[] = { INT_TO_ARG(map), POINTER_TO_ARG(values) };
  int args_size[] = { 0, get_glgetpixelmapv_size(map) * sizeof(float) };
  if (args_size[1] == 0) return;
  do_opengl_call(glGetPixelMapfv_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glGetPixelMapuiv( GLenum map, GLuint *values )
{
  long args[] = { INT_TO_ARG(map), POINTER_TO_ARG(values) };
  int args_size[] = { 0, get_glgetpixelmapv_size(map) * sizeof(int) };
  if (args_size[1] == 0) return;
  do_opengl_call(glGetPixelMapuiv_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glGetPixelMapusv( GLenum map, GLushort *values )
{
  long args[] = { INT_TO_ARG(map), POINTER_TO_ARG(values) };
  int args_size[] = { 0, get_glgetpixelmapv_size(map) * sizeof(short) };
  if (args_size[1] == 0) return;
  do_opengl_call(glGetPixelMapusv_func, NULL, CHECK_ARGS(args, args_size));
}

static int glMap1_get_multiplier(GLenum target)
{
  switch (target)
  {
    case GL_MAP1_VERTEX_3:
    case GL_MAP1_NORMAL:
    case GL_MAP1_TEXTURE_COORD_3:
      return 3;
      break;

    case GL_MAP1_VERTEX_4:
    case GL_MAP1_COLOR_4:
    case GL_MAP1_TEXTURE_COORD_4:
      return 4;
      break;

    case GL_MAP1_INDEX:
    case GL_MAP1_TEXTURE_COORD_1:
      return 1;
      break;

    case GL_MAP1_TEXTURE_COORD_2:
      return 2;
      break;

    default:
      if (target >= GL_MAP1_VERTEX_ATTRIB0_4_NV && target <= GL_MAP1_VERTEX_ATTRIB15_4_NV)
        return 4;
      log_gl("unhandled target = %d\n", target);
      return 0;
  }
}


static int glMap2_get_multiplier(GLenum target)
{
  switch (target)
  {
    case GL_MAP2_VERTEX_3:
    case GL_MAP2_NORMAL:
    case GL_MAP2_TEXTURE_COORD_3:
      return 3;
      break;

    case GL_MAP2_VERTEX_4:
    case GL_MAP2_COLOR_4:
    case GL_MAP2_TEXTURE_COORD_4:
      return 4;
      break;

    case GL_MAP2_INDEX:
    case GL_MAP2_TEXTURE_COORD_1:
      return 1;
      break;

    case GL_MAP2_TEXTURE_COORD_2:
      return 2;
      break;

    default:
      if (target >= GL_MAP2_VERTEX_ATTRIB0_4_NV && target <= GL_MAP2_VERTEX_ATTRIB15_4_NV)
        return 4;
      log_gl("unhandled target = %d\n", target);
      return 0;
  }
}

static int get_dimensionnal_evaluator(GLenum target)
{
  switch(target)
  {
    case GL_MAP1_COLOR_4:
    case GL_MAP1_INDEX:
    case GL_MAP1_NORMAL:
    case GL_MAP1_TEXTURE_COORD_1:
    case GL_MAP1_TEXTURE_COORD_2:
    case GL_MAP1_TEXTURE_COORD_3:
    case GL_MAP1_TEXTURE_COORD_4:
    case GL_MAP1_VERTEX_3:
    case GL_MAP1_VERTEX_4:
      return 1;

    case GL_MAP2_COLOR_4:
    case GL_MAP2_INDEX:
    case GL_MAP2_NORMAL:
    case GL_MAP2_TEXTURE_COORD_1:
    case GL_MAP2_TEXTURE_COORD_2:
    case GL_MAP2_TEXTURE_COORD_3:
    case GL_MAP2_TEXTURE_COORD_4:
    case GL_MAP2_VERTEX_3:
    case GL_MAP2_VERTEX_4:
      return 2;

    default:
      log_gl("unhandled target %d\n", target);
      return 0;
  }
}

GLAPI void APIENTRY glMap1f( GLenum target,
              GLfloat u1,
              GLfloat u2,
              GLint stride,
              GLint order,
              const GLfloat *points )
{
  long args[] = { INT_TO_ARG(target), FLOAT_TO_ARG(u1), FLOAT_TO_ARG(u2),
                  INT_TO_ARG(stride), INT_TO_ARG(order), POINTER_TO_ARG(points) };
  int args_size[] = { 0, 0, 0, 0, 0, 0 };
  int num_points = order;
  int multiplier = glMap1_get_multiplier(target);
  if (multiplier)
  {
    num_points *= multiplier;
    args_size[5] = num_points * sizeof(float);
    do_opengl_call(glMap1f_func, NULL, CHECK_ARGS(args, args_size));
  }
}

GLAPI void APIENTRY glMap1d( GLenum target,
              GLdouble u1,
              GLdouble u2,
              GLint stride,
              GLint order,
              const GLdouble *points )
{
  long args[] = { INT_TO_ARG(target), DOUBLE_TO_ARG(u1), DOUBLE_TO_ARG(u2),
                  INT_TO_ARG(stride), INT_TO_ARG(order), POINTER_TO_ARG(points) };
  int args_size[] = { 0, 0, 0, 0, 0, 0 };
  int num_points = order;
  int multiplier = glMap1_get_multiplier(target);
  if (multiplier)
  {
    num_points *= multiplier;
    args_size[5] = num_points * sizeof(double);
    do_opengl_call(glMap1d_func, NULL,  CHECK_ARGS(args, args_size));
  }
}

GLAPI void APIENTRY glMap2f( GLenum target,
              GLfloat u1,
              GLfloat u2,
              GLint ustride,
              GLint uorder,
              GLfloat v1,
              GLfloat v2,
              GLint vstride,
              GLint vorder,
              const GLfloat *points )
{
  long args[] = { INT_TO_ARG(target),
                  FLOAT_TO_ARG(u1), FLOAT_TO_ARG(u2),
                  INT_TO_ARG(ustride), INT_TO_ARG(uorder),
                  FLOAT_TO_ARG(v1), FLOAT_TO_ARG(v2),
                  INT_TO_ARG(vstride), INT_TO_ARG(vorder),
                  POINTER_TO_ARG(points) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int num_points = uorder * vorder;
  int multiplier = glMap2_get_multiplier(target);
  if (multiplier)
  {
    num_points *= multiplier;
    args_size[9] = num_points * sizeof(float);
    do_opengl_call(glMap2f_func, NULL, CHECK_ARGS(args, args_size));
  }
}

GLAPI void APIENTRY glMap2d( GLenum target,
              GLdouble u1,
              GLdouble u2,
              GLint ustride,
              GLint uorder,
              GLdouble v1,
              GLdouble v2,
              GLint vstride,
              GLint vorder,
              const GLdouble *points )
{
  long args[] = { INT_TO_ARG(target),
                  DOUBLE_TO_ARG(u1), DOUBLE_TO_ARG(u2),
                  INT_TO_ARG(ustride), INT_TO_ARG(uorder),
                  DOUBLE_TO_ARG(v1), DOUBLE_TO_ARG(v2),
                  INT_TO_ARG(vstride), INT_TO_ARG(vorder),
                  POINTER_TO_ARG(points) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  int num_points = uorder * vorder;
  int multiplier = glMap2_get_multiplier(target);
  if (multiplier)
  {
    num_points *= multiplier;
    args_size[9] = num_points * sizeof(double);
    do_opengl_call(glMap2d_func, NULL, CHECK_ARGS(args, args_size));
  }
}

static int _glGetMapv_get_n_components( GLenum target, GLenum query)
{
  int dim = get_dimensionnal_evaluator(target);
  if (query == GL_COEFF)
  {
    int orders[2] = { 1, 1 };
    glGetMapiv(target, GL_ORDER, orders);
    return orders[0] * orders[1] * ((dim == 1) ? glMap1_get_multiplier(target) : glMap2_get_multiplier(target));
  }
  else if (query == GL_ORDER)
  {
    return dim;
  }
  else if (query == GL_DOMAIN)
  {
    return 2 * dim;
  }
  else
    return 0;
}


GLAPI void APIENTRY glGetMapdv( GLenum target, GLenum query, GLdouble *v )
{
  int dim = get_dimensionnal_evaluator(target);
  if (dim == 0) return;
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(query), POINTER_TO_ARG(v) };
  int args_size[] = { 0, 0, _glGetMapv_get_n_components(target, query) * sizeof(double) };
  do_opengl_call(glGetMapdv_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glGetMapfv( GLenum target, GLenum query, GLfloat *v )
{
  int dim = get_dimensionnal_evaluator(target);
  if (dim == 0) return;
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(query), POINTER_TO_ARG(v) };
  int args_size[] = { 0, 0, _glGetMapv_get_n_components(target, query) * sizeof(float) };
  do_opengl_call(glGetMapfv_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glGetMapiv( GLenum target, GLenum query, GLint *v )
{
  int dim = get_dimensionnal_evaluator(target);
  if (dim == 0) return;
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(query), POINTER_TO_ARG(v) };
  int args_size[] = { 0, 0, _glGetMapv_get_n_components(target, query) * sizeof(int) };
  do_opengl_call(glGetMapiv_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glBindTexture(GLenum target, GLuint texture)
{
  CHECK_PROC(glBindTexture);
  GET_CURRENT_STATE();
  alloc_value(state->textureAllocator, texture);
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(texture) };
  if (target == GL_TEXTURE_2D)
  {
    state->current_server_state.bindTexture2D = texture;
  }
  else if (target == GL_TEXTURE_RECTANGLE_ARB)
  {
    state->current_server_state.bindTextureRectangle = texture;
  }
  do_opengl_call(glBindTexture_func, NULL, args, NULL);
}

GLAPI void APIENTRY EXT_FUNC(glBindTextureEXT) (GLenum target, GLuint texture)
{
  glBindTexture(target, texture);
}


GLAPI void APIENTRY glGenTextures( GLsizei n, GLuint *textures )
{
  CHECK_PROC(glGenTextures);
  GET_CURRENT_STATE();
  if (n <= 0) { log_gl("n <= 0\n"); return; }
  alloc_range(state->textureAllocator, n, textures);
  long args[] = { n };
  do_opengl_call(glGenTextures_fake_func, NULL, args, NULL);
}

GLAPI void APIENTRY glGenTexturesEXT( GLsizei n, GLuint *textures )
{
  glGenTextures(n, textures);
}

GLAPI void APIENTRY glDeleteTextures ( GLsizei n, const GLuint *textures )
{
  CHECK_PROC(glDeleteTextures);
  GET_CURRENT_STATE();
  if (n <= 0) { log_gl("n <= 0\n"); return; }
  delete_range(state->textureAllocator, n, textures);
  long args[] = { INT_TO_ARG(n), POINTER_TO_ARG(textures) };
  do_opengl_call(glDeleteTextures_func, NULL, args, NULL);
}

GLAPI void APIENTRY glDeleteTexturesEXT ( GLsizei n, const GLuint *textures )
{
  glDeleteTextures(n, textures);
}

static int getTexImageTypeSizeSimple(int format, int type)
{
  switch (type)
  {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
      return 1;

    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
      return 2;

    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_UNSIGNED_INT_24_8_EXT:
    case GL_FLOAT:
      return 4;

    default:
      log_gl("unknown texture type %d for texture format %d\n", type, format);
      return 0;
  }
}

static int getTexImageFactorFromFormatAndType(int format, int type)
{
  switch (format)
  {
    case GL_COLOR_INDEX:
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_INTENSITY:
    case GL_DEPTH_COMPONENT:
    case GL_STENCIL_INDEX:
    case GL_DEPTH_STENCIL_EXT:
      return 1 * getTexImageTypeSizeSimple(format, type);
      break;

    case GL_LUMINANCE_ALPHA:
      return 2 * getTexImageTypeSizeSimple(format, type);
      break;

    case GL_YCBCR_MESA:
    {
      switch (type)
      {
        case GL_UNSIGNED_SHORT_8_8_MESA:
        case GL_UNSIGNED_SHORT_8_8_REV_MESA:
          return 2;

        default:
          log_gl("unknown texture type %d for texture format %d\n", type, format);
          return 0;
      }
    }

    case GL_RGB:
    case GL_BGR:
    {
      switch (type)
      {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
          return 1 * 3;

        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
          return 2 * 3;

        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FLOAT:
          return 4 * 3;

        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
          return 1;

        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
        case GL_UNSIGNED_SHORT_8_8_MESA:
        case GL_UNSIGNED_SHORT_8_8_REV_MESA:
          return 2;

        default:
          log_gl("unknown texture type %d for texture format %d\n", type, format);
          return 0;
      }
    }

    case GL_RGBA:
    case GL_BGRA:
    case GL_ABGR_EXT:
    {
      switch (type)
      {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
          return 1 * 4;

        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
          return 2 * 4;

        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FLOAT:
          return 4 * 4;

        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
          return 2;

        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
          return 4;

        default:
          log_gl("unknown texture type %d for texture format %d\n", type, format);
          return 0;
      }
    }

    default:
      log_gl("unknown texture format : %d\n", format);
      return 0;
  }
}

static void* _calcReadSize(int width, int height, int depth, GLenum format, GLenum type, void* pixels, int* p_size)
{
  int pack_row_length, pack_alignment, pack_skip_rows, pack_skip_pixels;

  glGetIntegerv(GL_PACK_ROW_LENGTH, &pack_row_length);
  glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment);
  glGetIntegerv(GL_PACK_SKIP_ROWS, &pack_skip_rows);
  glGetIntegerv(GL_PACK_SKIP_PIXELS, &pack_skip_pixels);

  int w = (pack_row_length == 0) ? width : pack_row_length;
  int size = ((width * getTexImageFactorFromFormatAndType(format, type) + pack_alignment - 1) & (~(pack_alignment-1))) * depth;
  if (height >= 1)
    size += ((w * getTexImageFactorFromFormatAndType(format, type) + pack_alignment - 1) & (~(pack_alignment-1)))* (height-1)  * depth ;
  *p_size = size;

  pixels += (pack_skip_pixels + pack_skip_rows * w) * getTexImageFactorFromFormatAndType(format, type);

  return pixels;
}

static const void* _calcWriteSize(int width, int height, int depth, GLenum format, GLenum type, const void* pixels, int* p_size)
{
  int unpack_row_length, unpack_alignment, unpack_skip_rows, unpack_skip_pixels;

  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpack_row_length);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
  glGetIntegerv(GL_UNPACK_SKIP_ROWS, &unpack_skip_rows);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &unpack_skip_pixels);

  int w = (unpack_row_length == 0) ? width : unpack_row_length;
  int size = ((width * getTexImageFactorFromFormatAndType(format, type) + unpack_alignment - 1) & (~(unpack_alignment-1))) * depth;
  if (height >= 1)
    size += ((w * getTexImageFactorFromFormatAndType(format, type) + unpack_alignment - 1) & (~(unpack_alignment-1))) * (height-1) * depth;
  *p_size = size;

  pixels += (unpack_skip_pixels + unpack_skip_rows * w) * getTexImageFactorFromFormatAndType(format, type);

  return pixels;
}

GLAPI void APIENTRY glTexImage1D(GLenum target,
                  GLint level,
                  GLint internalFormat,
                  GLsizei width,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const GLvoid *pixels )
{
  int size = 0;
  if (pixels)
    pixels = _calcWriteSize(width, 1, 1, format, type, pixels, &size);

  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
                  INT_TO_ARG(width), INT_TO_ARG(border), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, (pixels == NULL) ? 0 : size };
  do_opengl_call(glTexImage1D_func, NULL, CHECK_ARGS(args, args_size));

}

GLAPI void APIENTRY glTexImage1DEXT(GLenum target,
                  GLint level,
                  GLint internalFormat,
                  GLsizei width,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const GLvoid *pixels )
{
  glTexImage1D(target, level, internalFormat, width, border, format, type, pixels);
}

GLAPI GLint GLAPIENTRY gluBuild2DMipmaps (GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
  int size = 0;
  pixels = _calcWriteSize(width, height, 1, format, type, pixels, &size);
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(internalFormat),
                  INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, size };
  do_opengl_call(fake_gluBuild2DMipmaps_func, NULL, CHECK_ARGS(args, args_size));
  return 0;
}


GLAPI void APIENTRY glTexImage2D( GLenum target,
                   GLint level,
                   GLint internalFormat,
                   GLsizei width,
                   GLsizei height,
                   GLint border,
                   GLenum format,
                   GLenum type,
                   const GLvoid *pixels )
{
  GET_CURRENT_STATE();
  int i;
  int size = 0;
  if (pixels)
    pixels = _calcWriteSize(width, height, 1, format, type, pixels, &size);

  if (target == GL_TEXTURE_2D)
  {
    for(i=0;i<state->current_server_state.texture2DCacheDim;i++)
    {
      if (state->current_server_state.texture2DCache[i].texture == state->current_server_state.bindTexture2D &&
          state->current_server_state.texture2DCache[i].level == level)
      {
        state->current_server_state.texture2DCache[i].width = width;
        state->current_server_state.texture2DCache[i].height = height;
        break;
      }
    }
    if (i == state->current_server_state.texture2DCacheDim)
    {
      state->current_server_state.texture2DCache =
          realloc(state->current_server_state.texture2DCache, sizeof(Texture2DDim) *
                  (state->current_server_state.texture2DCacheDim + 1));
      i = state->current_server_state.texture2DCacheDim;
      state->current_server_state.texture2DCache[i].texture = state->current_server_state.bindTexture2D;
      state->current_server_state.texture2DCache[i].level = level;
      state->current_server_state.texture2DCache[i].width = width;
      state->current_server_state.texture2DCache[i].height = height;
      state->current_server_state.texture2DCacheDim++;
    }
  }
  else if (target == GL_PROXY_TEXTURE_2D_EXT)
  {
    for(i=0;i<state->current_server_state.textureProxy2DCacheDim;i++)
    {
      if (state->current_server_state.textureProxy2DCache[i].level == level)
      {
        state->current_server_state.textureProxy2DCache[i].width = width;
        state->current_server_state.textureProxy2DCache[i].height = height;
        break;
      }
    }
    if (i == state->current_server_state.textureProxy2DCacheDim)
    {
      state->current_server_state.textureProxy2DCache =
          realloc(state->current_server_state.textureProxy2DCache, sizeof(Texture2DDim) *
                  (state->current_server_state.textureProxy2DCacheDim + 1));
      i = state->current_server_state.textureProxy2DCacheDim;
      state->current_server_state.textureProxy2DCache[i].level = level;
      state->current_server_state.textureProxy2DCache[i].width = width;
      state->current_server_state.textureProxy2DCache[i].height = height;
      state->current_server_state.textureProxy2DCacheDim++;
    }
  }
  else if (target == GL_TEXTURE_RECTANGLE_ARB)
  {
    for(i=0;i<state->current_server_state.textureRectangleCacheDim;i++)
    {
      if (state->current_server_state.textureRectangleCache[i].texture == state->current_server_state.bindTextureRectangle &&
          state->current_server_state.textureRectangleCache[i].level == level)
      {
        state->current_server_state.textureRectangleCache[i].width = width;
        state->current_server_state.textureRectangleCache[i].height = height;
        break;
      }
    }
    if (i == state->current_server_state.textureRectangleCacheDim)
    {
      state->current_server_state.textureRectangleCache =
          realloc(state->current_server_state.textureRectangleCache, sizeof(Texture2DDim) *
                  (state->current_server_state.textureRectangleCacheDim + 1));
      i = state->current_server_state.textureRectangleCacheDim;
      state->current_server_state.textureRectangleCache[i].texture = state->current_server_state.bindTextureRectangle;
      state->current_server_state.textureRectangleCache[i].level = level;
      state->current_server_state.textureRectangleCache[i].width = width;
      state->current_server_state.textureRectangleCache[i].height = height;
      state->current_server_state.textureRectangleCacheDim++;
    }
  }
  else if (target == GL_PROXY_TEXTURE_RECTANGLE_ARB)
  {
    for(i=0;i<state->current_server_state.textureProxyRectangleCacheDim;i++)
    {
      if (state->current_server_state.textureProxyRectangleCache[i].level == level)
      {
        state->current_server_state.textureProxyRectangleCache[i].width = width;
        state->current_server_state.textureProxyRectangleCache[i].height = height;
        break;
      }
    }
    if (i == state->current_server_state.textureProxyRectangleCacheDim)
    {
      state->current_server_state.textureProxyRectangleCache =
          realloc(state->current_server_state.textureProxyRectangleCache, sizeof(Texture2DDim) *
                  (state->current_server_state.textureProxyRectangleCacheDim + 1));
      i = state->current_server_state.textureProxyRectangleCacheDim;
      state->current_server_state.textureProxyRectangleCache[i].level = level;
      state->current_server_state.textureProxyRectangleCache[i].width = width;
      state->current_server_state.textureProxyRectangleCache[i].height = height;
      state->current_server_state.textureProxyRectangleCacheDim++;
    }
  }


  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
                  INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(border), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, (pixels == NULL) ? 0 : size };
  do_opengl_call(glTexImage2D_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glTexImage2DEXT(GLenum target,
                   GLint level,
                   GLint internalFormat,
                   GLsizei width,
                   GLsizei height,
                   GLint border,
                   GLenum format,
                   GLenum type,
                   const GLvoid *pixels )
{
  glTexImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
}

GLAPI void APIENTRY glTexImage3D( GLenum target,
                   GLint level,
                   GLint internalFormat,
                   GLsizei width,
                   GLsizei height,
                   GLsizei depth,
                   GLint border,
                   GLenum format,
                   GLenum type,
                   const GLvoid *pixels )
{
  int size = 0;
  if (pixels)
    pixels = _calcWriteSize(width, height, depth, format, type, pixels, &size);

  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
                  INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(depth), INT_TO_ARG(border), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, (pixels == NULL) ? 0 : size };
  do_opengl_call(glTexImage3D_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glTexImage3DEXT)(GLenum target,
                  GLint level,
                  GLint internalFormat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const GLvoid *pixels )
{
  CHECK_PROC(glTexImage3DEXT);
  glTexImage3D(target, level, internalFormat, width, height, depth, border, format, type, pixels);
}

GLAPI void APIENTRY glTexSubImage1D( GLenum target,
                      GLint level,
                      GLint xoffset,
                      GLsizei width,
                      GLenum format,
                      GLenum type,
                      const GLvoid *pixels )
{
  int size = 0;
  if (pixels)
    pixels = _calcWriteSize(width, 1, 1, format, type, pixels, &size);

  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset),
                  INT_TO_ARG(width), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, size };
  do_opengl_call(glTexSubImage1D_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glTexSubImage1DEXT)( GLenum target,
                      GLint level,
                      GLint xoffset,
                      GLsizei width,
                      GLenum format,
                      GLenum type,
                      const GLvoid *pixels )
{
  CHECK_PROC(glTexSubImage1DEXT);
  glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
}

GLAPI void APIENTRY glTexSubImage2D( GLenum target,
                      GLint level,
                      GLint xoffset,
                      GLint yoffset,
                      GLsizei width,
                      GLsizei height,
                      GLenum format,
                      GLenum type,
                      const GLvoid *pixels )
{
  int size = 0;
  if (pixels)
    pixels = _calcWriteSize(width, height, 1, format, type, pixels, &size);

  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset), INT_TO_ARG(yoffset),
                  INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, size };
  do_opengl_call(glTexSubImage2D_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glTexSubImage2DEXT)( GLenum target,
                      GLint level,
                      GLint xoffset,
                      GLint yoffset,
                      GLsizei width,
                      GLsizei height,
                      GLenum format,
                      GLenum type,
                      const GLvoid *pixels )
{
  CHECK_PROC(glTexSubImage2DEXT);
  glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

GLAPI void APIENTRY glTexSubImage3D( GLenum target,
                      GLint level,
                      GLint xoffset,
                      GLint yoffset,
                      GLint zoffset,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth,
                      GLenum format,
                      GLenum type,
                      const GLvoid *pixels )
{
  int size = 0;
  if (pixels)
    pixels = _calcWriteSize(width, height, depth, format, type, pixels, &size);

  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset), INT_TO_ARG(yoffset), INT_TO_ARG(zoffset),
                  INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(depth), INT_TO_ARG(format), INT_TO_ARG(type),
                  POINTER_TO_ARG(pixels) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, size };
  do_opengl_call(glTexSubImage3D_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glTexSubImage3DEXT)( GLenum target,
                      GLint level,
                      GLint xoffset,
                      GLint yoffset,
                      GLint zoffset,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth,
                      GLenum format,
                      GLenum type,
                      const GLvoid *pixels )
{
  CHECK_PROC(glTexSubImage3DEXT);
  glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}
GLAPI void APIENTRY glSelectBuffer( GLsizei size, GLuint *buffer )
{
  if (size <= 0) return;
  GET_CURRENT_STATE();
  state->client_state.selectBufferSize = size;
  state->client_state.selectBufferPtr = buffer;
  long args[] = { INT_TO_ARG(size) };
  do_opengl_call(_glSelectBuffer_fake_func, NULL, args, NULL);
}

GLAPI void APIENTRY glFeedbackBuffer( GLsizei size, GLenum type, GLfloat *buffer )
{
  if (size <= 0) return;
  GET_CURRENT_STATE();
  state->client_state.feedbackBufferSize = size;
  state->client_state.feedbackBufferPtr = buffer;
  long args[] = { INT_TO_ARG(size), INT_TO_ARG(type) };
  do_opengl_call(_glFeedbackBuffer_fake_func, NULL, args, NULL);
}

GLAPI GLint APIENTRY glRenderMode(GLenum mode)
{
  GLint ret;
  GET_CURRENT_STATE();
  long args[] = { UNSIGNED_INT_TO_ARG(mode)};
  do_opengl_call(glRenderMode_func, &ret, args, NULL);
  if (mode == GL_SELECT && state->client_state.selectBufferPtr)
  {
    long args[] = { POINTER_TO_ARG(state->client_state.selectBufferPtr) };
    int args_size[] = { state->client_state.selectBufferSize * 4 };
    do_opengl_call(_glGetSelectBuffer_fake_func, NULL, CHECK_ARGS(args, args_size));
  }
  else if (mode == GL_FEEDBACK && state->client_state.selectBufferPtr)
  {
    long args[] = {  POINTER_TO_ARG(state->client_state.feedbackBufferPtr) };
    int args_size[] = { state->client_state.feedbackBufferSize * 4 };
    do_opengl_call(_glGetFeedbackBuffer_fake_func, NULL, CHECK_ARGS(args, args_size));
  }
  return ret;
}


GLAPI void APIENTRY EXT_FUNC(glGetCompressedTexImageARB)(GLenum target, GLint level, GLvoid *img)
{
  CHECK_PROC(glGetCompressedTexImageARB);

  int imageSize = 0;
  glGetTexLevelParameteriv(target, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &imageSize);

  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), POINTER_TO_ARG(img) };
  int args_size[] = { 0, 0, imageSize };
  do_opengl_call(glGetCompressedTexImageARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetCompressedTexImage)(GLenum target, GLint level, GLvoid *img)
{
  glGetCompressedTexImageARB(target, level, img);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage1DARB)(GLenum target,
                               GLint level,
                               GLint internalFormat,
                               GLsizei width,
                               GLint border,
                               GLsizei imageSize,
                               const GLvoid * data)
{
  CHECK_PROC(glCompressedTexImage1DARB);
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
                  INT_TO_ARG(width), INT_TO_ARG(border), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, imageSize };
  do_opengl_call(glCompressedTexImage1DARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage1D)(GLenum target,
                               GLint level,
                               GLenum internalFormat,
                               GLsizei width,
                               GLint border,
                               GLsizei imageSize,
                               const GLvoid * data)
{
  glCompressedTexImage1DARB(target, level, internalFormat, width, border, imageSize, data);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage2DARB)(GLenum target,
                               GLint level,
                               GLint internalFormat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLsizei imageSize,
                               const GLvoid * data)
{
  CHECK_PROC(glCompressedTexImage2DARB);
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
                  INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(border), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, imageSize };
  do_opengl_call(glCompressedTexImage2DARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage2D)(GLenum target,
                               GLint level,
                               GLenum internalFormat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLsizei imageSize,
                               const GLvoid * data)
{
  glCompressedTexImage2DARB(target, level, internalFormat, width, height, border, imageSize, data);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage3DARB)(GLenum target,
                               GLint level,
                               GLint internalFormat,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLint border,
                               GLsizei imageSize,
                               const GLvoid * data)
{
  CHECK_PROC(glCompressedTexImage3DARB);
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(internalFormat),
                  INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(depth), INT_TO_ARG(border), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, imageSize };
  do_opengl_call(glCompressedTexImage3DARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexImage3D)(GLenum target,
                               GLint level,
                               GLenum internalFormat,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLint border,
                               GLsizei imageSize,
                               const GLvoid * data)
{
  glCompressedTexImage3DARB(target, level, internalFormat, width, height, depth, border, imageSize, data);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage1DARB)(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLsizei width,
                                  GLenum format,
                                  GLsizei imageSize,
                                  const GLvoid * data)
{
  CHECK_PROC(glCompressedTexSubImage1DARB);
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset),
                  INT_TO_ARG(width), INT_TO_ARG(format), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, imageSize };
  do_opengl_call(glCompressedTexSubImage1DARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage1D)(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLsizei width,
                                  GLenum format,
                                  GLsizei imageSize,
                                  const GLvoid * data)
{
  glCompressedTexSubImage1DARB(target, level, xoffset, width, format, imageSize, data);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage2DARB)(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLsizei imageSize,
                                  const GLvoid * data)
{
  CHECK_PROC(glCompressedTexSubImage2DARB);
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset), INT_TO_ARG(yoffset),
                  INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(format), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, imageSize };
  do_opengl_call(glCompressedTexSubImage2DARB_func, NULL, CHECK_ARGS(args, args_size));
}


GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage2D)(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLsizei imageSize,
                                  const GLvoid * data)
{
  glCompressedTexSubImage2DARB(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage3DARB)(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint zoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLenum format,
                                  GLsizei imageSize,
                                  const GLvoid * data)
{
  CHECK_PROC(glCompressedTexSubImage3DARB);
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(xoffset), INT_TO_ARG(yoffset), INT_TO_ARG(zoffset),
                  INT_TO_ARG(width), INT_TO_ARG(height), INT_TO_ARG(depth), INT_TO_ARG(format), INT_TO_ARG(imageSize), POINTER_TO_ARG(data) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, imageSize };
  do_opengl_call(glCompressedTexSubImage3DARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glCompressedTexSubImage3D)(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint zoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLenum format,
                                  GLsizei imageSize,
                                  const GLvoid * data)
{
  glCompressedTexSubImage3DARB(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

GLAPI void APIENTRY glGetTexLevelParameteriv( GLenum target,
                               GLint level,
                               GLenum pname,
                               GLint *params )
{
    int i;
  GET_CURRENT_STATE();

  if (target == GL_TEXTURE_2D && (pname == GL_TEXTURE_WIDTH || pname == GL_TEXTURE_HEIGHT))
  {
    for(i=0;i<state->current_server_state.texture2DCacheDim;i++)
    {
      if (state->current_server_state.texture2DCache[i].texture == state->current_server_state.bindTexture2D &&
          state->current_server_state.texture2DCache[i].level == level)
      {
        if (pname == GL_TEXTURE_WIDTH)
        {
          *params = state->current_server_state.texture2DCache[i].width;
          return;
        }
        if (pname == GL_TEXTURE_HEIGHT)
        {
          *params = state->current_server_state.texture2DCache[i].height;
          return;
        }
      }
    }
  }
  else if (target == GL_PROXY_TEXTURE_2D_EXT && (pname == GL_TEXTURE_WIDTH || pname == GL_TEXTURE_HEIGHT))
  {
    for(i=0;i<state->current_server_state.textureProxy2DCacheDim;i++)
    {
      if (state->current_server_state.textureProxy2DCache[i].level == level)
      {
        if (pname == GL_TEXTURE_WIDTH)
        {
          *params = state->current_server_state.textureProxy2DCache[i].width;
          return;
        }
        if (pname == GL_TEXTURE_HEIGHT)
        {
          *params = state->current_server_state.textureProxy2DCache[i].height;
          return;
        }
      }
    }
  }
  else if (target == GL_TEXTURE_RECTANGLE_ARB && (pname == GL_TEXTURE_WIDTH || pname == GL_TEXTURE_HEIGHT))
  {
    for(i=0;i<state->current_server_state.textureRectangleCacheDim;i++)
    {
      if (state->current_server_state.textureRectangleCache[i].texture == state->current_server_state.bindTextureRectangle &&
          state->current_server_state.textureRectangleCache[i].level == level)
      {
        if (pname == GL_TEXTURE_WIDTH)
        {
          *params = state->current_server_state.textureRectangleCache[i].width;
          return;
        }
        if (pname == GL_TEXTURE_HEIGHT)
        {
          *params = state->current_server_state.textureRectangleCache[i].height;
          return;
        }
      }
    }
  }
  else if (target == GL_PROXY_TEXTURE_RECTANGLE_ARB && (pname == GL_TEXTURE_WIDTH || pname == GL_TEXTURE_HEIGHT))
  {
    for(i=0;i<state->current_server_state.textureProxyRectangleCacheDim;i++)
    {
      if (state->current_server_state.textureProxyRectangleCache[i].level == level)
      {
        if (pname == GL_TEXTURE_WIDTH)
        {
          *params = state->current_server_state.textureProxyRectangleCache[i].width;
          return;
        }
        if (pname == GL_TEXTURE_HEIGHT)
        {
          *params = state->current_server_state.textureProxyRectangleCache[i].height;
          return;
        }
      }
    }
  }

  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(pname), POINTER_TO_ARG(params) };
  do_opengl_call(glGetTexLevelParameteriv_func, NULL, args, NULL);

}

GLAPI void APIENTRY glGetTexParameterfv( GLenum target, GLenum pname, GLfloat *params )
{
  if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
    *params = 1;
  else
  {
    int size = __glTexParameter_size(get_err_file(), pname);
    long args[] = { INT_TO_ARG(target), INT_TO_ARG(pname), POINTER_TO_ARG(params) };
    int args_size[] = { 0, 0, size * sizeof(GLfloat) };
    do_opengl_call(glGetTexParameterfv_func, NULL, CHECK_ARGS(args, args_size));
  }
}

GLAPI void APIENTRY glFogf(GLenum pname, GLfloat param)
{
  GET_CURRENT_STATE();
  if (pname == GL_FOG_MODE)
    state->current_server_state.fog.mode = param;
  else if (pname == GL_FOG_DENSITY)
    state->current_server_state.fog.density = param;
  else if (pname == GL_FOG_START)
    state->current_server_state.fog.start = param;
  else if (pname == GL_FOG_END)
    state->current_server_state.fog.end = param;
  else if (pname == GL_FOG_INDEX)
    state->current_server_state.fog.index = param;
  long args[] = { UNSIGNED_INT_TO_ARG(pname), FLOAT_TO_ARG(param)};
  do_opengl_call(glFogf_func, NULL, args, NULL);
}

GLAPI void APIENTRY glFogi(GLenum pname, GLint param)
{
  GET_CURRENT_STATE();
  if (pname == GL_FOG_MODE)
    state->current_server_state.fog.mode = param;
  else if (pname == GL_FOG_DENSITY)
    state->current_server_state.fog.density = param;
  else if (pname == GL_FOG_START)
    state->current_server_state.fog.start = param;
  else if (pname == GL_FOG_END)
    state->current_server_state.fog.end = param;
  else if (pname == GL_FOG_INDEX)
    state->current_server_state.fog.index = param;
  long args[] = { UNSIGNED_INT_TO_ARG(pname), INT_TO_ARG(param)};
  do_opengl_call(glFogi_func, NULL, args, NULL);
}


GLAPI void APIENTRY glFogfv( GLenum pname, const GLfloat *params )
{
  if (pname != GL_FOG_COLOR)
  {
    glFogf(pname, *params);
    return;
  }
  GET_CURRENT_STATE();
  long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(params) };
  memcpy(state->current_server_state.fog.color, params, 4 * sizeof(float));
  do_opengl_call(glFogfv_func, NULL, args, NULL);
}

GLAPI void APIENTRY glFogiv( GLenum pname, const GLint *params )
{
  if (pname != GL_FOG_COLOR)
  {
    glFogi(pname, *params);
    return;
  }
  long args[] = { INT_TO_ARG(pname), POINTER_TO_ARG(params) };
  do_opengl_call(glFogiv_func, NULL, args, NULL);
}

GLAPI void APIENTRY glRectdv( const GLdouble *v1, const GLdouble *v2 )
{
  glRectd(v1[0], v1[1], v2[0], v2[1]);
}
GLAPI void APIENTRY glRectfv( const GLfloat *v1, const GLfloat *v2 )
{
  glRectf(v1[0], v1[1], v2[0], v2[1]);
}
GLAPI void APIENTRY glRectiv( const GLint *v1, const GLint *v2 )
{
  glRecti(v1[0], v1[1], v2[0], v2[1]);
}
GLAPI void APIENTRY glRectsv( const GLshort *v1, const GLshort *v2 )
{
  glRects(v1[0], v1[1], v2[0], v2[1]);
}


GLAPI void APIENTRY glBitmap(GLsizei width,
              GLsizei height,
              GLfloat xorig,
              GLfloat yorig,
              GLfloat xmove,
              GLfloat ymove,
              const GLubyte *bitmap )
{
  GET_CURRENT_STATE();
  int unpack_alignment, unpack_row_length;
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpack_row_length);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
  int w = (unpack_row_length == 0) ? width : unpack_row_length;
  int size = ((w + unpack_alignment - 1) & (~(unpack_alignment-1))) * height;
  long args[] = { INT_TO_ARG(width), INT_TO_ARG(height), FLOAT_TO_ARG(xorig), FLOAT_TO_ARG(yorig),
                  FLOAT_TO_ARG(xmove), FLOAT_TO_ARG(ymove), POINTER_TO_ARG(bitmap) };
  int args_size[] = { 0, 0, 0, 0, 0, 0, size };
  do_opengl_call(glBitmap_func, NULL, CHECK_ARGS(args, args_size));

  state->currentRasterPos[0] += xmove;
  state->currentRasterPos[1] += ymove;
}

GLAPI void APIENTRY glGetTexImage( GLenum target,
                    GLint level,
                    GLenum format,
                    GLenum type,
                    GLvoid *pixels )
{
  int size = 0, width, height = 1, depth = 1;
  if (target == GL_PROXY_TEXTURE_1D || target == GL_PROXY_TEXTURE_2D || target == GL_PROXY_TEXTURE_3D)
  {
    log_gl("unhandled target : %d\n", target);
    return;
  }
  glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
  if (target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE_ARB || target == GL_TEXTURE_3D)
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
  if (target == GL_TEXTURE_3D)
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH, &depth);

  pixels = _calcReadSize(width, height, depth, format, type, pixels, &size);

  long args[] = { INT_TO_ARG(target), INT_TO_ARG(level), INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
  int args_size[] = { 0, 0, 0, 0, size };
  do_opengl_call(glGetTexImage_func, NULL, CHECK_ARGS(args, args_size));
}


void glReadPixels_no_lock(GLint x,
                  GLint y,
                  GLsizei width,
                  GLsizei height,
                  GLenum format,
                  GLenum type,
                  GLvoid *pixels )
{
  GET_CURRENT_STATE();
  if (state->pixelPackBuffer)
  {
    int fake_ret;
    long args[] = { INT_TO_ARG(x), INT_TO_ARG(y), INT_TO_ARG(width), INT_TO_ARG(height),
                    INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
    /* Make it synchronous, otherwise it floods server */
    do_opengl_call_no_lock(_glReadPixels_pbo_func, &fake_ret, args, NULL);
  }
  else
  {
    int size = 0;

    pixels = _calcReadSize(width, height, 1, format, type, pixels, &size);

    long args[] = { INT_TO_ARG(x), INT_TO_ARG(y), INT_TO_ARG(width), INT_TO_ARG(height),
                    INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, size };

    do_opengl_call_no_lock(glReadPixels_func, NULL, CHECK_ARGS(args, args_size));
  }
}

GLAPI void APIENTRY glReadPixels(GLint x,
                  GLint y,
                  GLsizei width,
                  GLsizei height,
                  GLenum format,
                  GLenum type,
                  GLvoid *pixels )
{
  LOCK(glReadPixels_func);
  glReadPixels_no_lock(x, y, width, height, format, type, pixels);
  UNLOCK(glReadPixels_func);
}

GLAPI void APIENTRY glDrawPixels(GLsizei width,
                  GLsizei height,
                  GLenum format,
                  GLenum type,
                  const GLvoid *pixels )
{
  GET_CURRENT_STATE();
  if (state->pixelUnpackBuffer)
  {
    long args[] = { INT_TO_ARG(width), INT_TO_ARG(height),
                    INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
    do_opengl_call(_glDrawPixels_pbo_func, NULL, args, NULL);
  }
  else
  {
    int size = 0;

    pixels = _calcWriteSize(width, height, 1, format, type, pixels, &size);

    long args[] = { INT_TO_ARG(width), INT_TO_ARG(height),
                    INT_TO_ARG(format), INT_TO_ARG(type), POINTER_TO_ARG(pixels) };
    int args_size[] = { 0, 0, 0, 0, size };
    do_opengl_call(glDrawPixels_func, NULL, CHECK_ARGS(args, args_size));
  }
}

GLAPI void APIENTRY glInterleavedArrays( GLenum format,
                                         GLsizei stride,
                                         const GLvoid *ptr )
{
  CHECK_PROC(glInterleavedArrays);
  GET_CURRENT_STATE();
  if (debug_array_ptr)
      log_gl("glInterleavedArrays format=%d stride=%d ptr=%p\n",  format, stride, ptr);
  state->client_state.arrays.interleavedArrays.format = format;
  state->client_state.arrays.interleavedArrays.stride = stride;
  state->client_state.arrays.interleavedArrays.ptr = ptr;
}

GLAPI void APIENTRY glVertexPointer( GLint size,
                      GLenum type,
                      GLsizei stride,
                      const GLvoid *ptr )
{
  CHECK_PROC(glVertexPointer);
  GET_CURRENT_STATE();

  state->client_state.arrays.vertexArray.vbo_name = state->arrayBuffer;
  if (state->client_state.arrays.vertexArray.vbo_name)
  {
    long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
    do_opengl_call(_glVertexPointer_buffer_func, NULL, args, NULL);
    return;
  }

  if (state->client_state.arrays.vertexArray.size == size &&
      state->client_state.arrays.vertexArray.type == type &&
      state->client_state.arrays.vertexArray.stride == stride &&
      state->client_state.arrays.vertexArray.ptr == ptr)
  {
  }
  else
  {
    if (debug_array_ptr)
      log_gl("glVertexPointer size=%d type=%d stride=%d ptr=%p\n",  size, type, stride, ptr);
    state->client_state.arrays.vertexArray.size = size;
    state->client_state.arrays.vertexArray.type = type;
    state->client_state.arrays.vertexArray.stride = stride;
    state->client_state.arrays.vertexArray.ptr = ptr;
  }
}

DEFINE_EXT(glVertexPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr));

GLAPI void APIENTRY glNormalPointer( GLenum type,
                      GLsizei stride,
                      const GLvoid *ptr )
{
  CHECK_PROC(glNormalPointer);
  GET_CURRENT_STATE();

  state->client_state.arrays.normalArray.vbo_name = state->arrayBuffer;
  if (state->client_state.arrays.normalArray.vbo_name)
  {
    long args[] = { INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
    do_opengl_call(_glNormalPointer_buffer_func, NULL, args, NULL);
    return;
  }

  if (state->client_state.arrays.normalArray.type == type &&
      state->client_state.arrays.normalArray.stride == stride &&
      state->client_state.arrays.normalArray.ptr == ptr)
  {
  }
  else
  {
    if (debug_array_ptr)
      log_gl("glNormalPointer type=%d stride=%d ptr=%p\n", type, stride, ptr);
    state->client_state.arrays.normalArray.size = 3;
    state->client_state.arrays.normalArray.type = type;
    state->client_state.arrays.normalArray.stride = stride;
    state->client_state.arrays.normalArray.ptr = ptr;
  }
}

DEFINE_EXT(glNormalPointer, (GLenum type, GLsizei stride, const GLvoid *ptr), (type, stride, ptr));

GLAPI void APIENTRY glIndexPointer( GLenum type,
                      GLsizei stride,
                      const GLvoid *ptr )
{
  CHECK_PROC(glIndexPointer);
  GET_CURRENT_STATE();

  state->client_state.arrays.indexArray.vbo_name = state->arrayBuffer;
  if (state->client_state.arrays.indexArray.vbo_name)
  {
    long args[] = { INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
    do_opengl_call(_glIndexPointer_buffer_func, NULL, args, NULL);
    return;
  }

  if (state->client_state.arrays.indexArray.type == type &&
      state->client_state.arrays.indexArray.stride == stride &&
      state->client_state.arrays.indexArray.ptr == ptr)
  {
  }
  else
  {
    if (debug_array_ptr)
      log_gl("glIndexPointer type=%d stride=%d ptr=%p\n", type, stride, ptr);
    state->client_state.arrays.indexArray.size = 1;
    state->client_state.arrays.indexArray.type = type;
    state->client_state.arrays.indexArray.stride = stride;
    state->client_state.arrays.indexArray.ptr = ptr;
  }
}

DEFINE_EXT(glIndexPointer, (GLenum type, GLsizei stride, const GLvoid *ptr), (type, stride, ptr));

GLAPI void APIENTRY glColorPointer( GLint size,
                     GLenum type,
                     GLsizei stride,
                     const GLvoid *ptr )
{
  CHECK_PROC(glColorPointer);
  GET_CURRENT_STATE();

  state->client_state.arrays.colorArray.vbo_name = state->arrayBuffer;
  if (state->client_state.arrays.colorArray.vbo_name)
  {
    long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
    do_opengl_call(_glColorPointer_buffer_func, NULL, args, NULL);
    return;
  }

  if (state->client_state.arrays.colorArray.size == size &&
      state->client_state.arrays.colorArray.type == type &&
      state->client_state.arrays.colorArray.stride == stride &&
      state->client_state.arrays.colorArray.ptr == ptr)
  {
  }
  else
  {
    if (debug_array_ptr)
      log_gl("glColorPointer size=%d type=%d stride=%d ptr=%p\n", size, type, stride, ptr);
    state->client_state.arrays.colorArray.size = size;
    state->client_state.arrays.colorArray.type = type;
    state->client_state.arrays.colorArray.stride = stride;
    state->client_state.arrays.colorArray.ptr = ptr;
  }
}

DEFINE_EXT(glColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr));

GLAPI void APIENTRY glSecondaryColorPointer( GLint size,
                     GLenum type,
                     GLsizei stride,
                     const GLvoid *ptr )
{
  CHECK_PROC(glSecondaryColorPointer);
  GET_CURRENT_STATE();

  state->client_state.arrays.secondaryColorArray.vbo_name = state->arrayBuffer;
  if (state->client_state.arrays.secondaryColorArray.vbo_name)
  {
    long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
    do_opengl_call(_glSecondaryColorPointer_buffer_func, NULL, args, NULL);
    return;
  }

  if (state->client_state.arrays.secondaryColorArray.size == size &&
      state->client_state.arrays.secondaryColorArray.type == type &&
      state->client_state.arrays.secondaryColorArray.stride == stride &&
      state->client_state.arrays.secondaryColorArray.ptr == ptr)
  {
  }
  else
  {
    if (debug_array_ptr)
      log_gl("glSecondaryColorPointer size=%d type=%d stride=%d ptr=%p\n", size, type, stride, ptr);
    state->client_state.arrays.secondaryColorArray.size = size;
    state->client_state.arrays.secondaryColorArray.type = type;
    state->client_state.arrays.secondaryColorArray.stride = stride;
    state->client_state.arrays.secondaryColorArray.ptr = ptr;
  }
}

DEFINE_EXT(glSecondaryColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr));

GLAPI void APIENTRY glTexCoordPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr )
{
  CHECK_PROC(glTexCoordPointer);
  GET_CURRENT_STATE();

  state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].vbo_name = state->arrayBuffer;
  if (state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].vbo_name)
  {
    long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
    do_opengl_call(_glTexCoordPointer_buffer_func, NULL, args, NULL);
    return;
  }

  if (state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].size == size &&
      state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].type == type &&
      state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].stride == stride &&
      state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].ptr == ptr)
  {
  }
  else
  {
    if (debug_array_ptr)
      log_gl("glTexCoordPointer[%d] size=%d type=%d stride=%d ptr=%p\n",
              state->client_state.clientActiveTexture, size, type, stride, ptr);
    state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].index =
        state->client_state.clientActiveTexture;
    state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].size = size;
    state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].type = type;
    state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].stride = stride;
    state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].ptr = ptr;
  }
}

DEFINE_EXT(glTexCoordPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr));

GLAPI void APIENTRY glEdgeFlagPointer( GLsizei stride, const GLvoid *ptr )
{
  CHECK_PROC(glEdgeFlagPointer);
  GET_CURRENT_STATE();

  state->client_state.arrays.edgeFlagArray.vbo_name = state->arrayBuffer;
  if (state->client_state.arrays.edgeFlagArray.vbo_name)
  {
    long args[] = { INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
    do_opengl_call(_glEdgeFlagPointer_buffer_func, NULL, args, NULL);
    return;
  }

  if (state->client_state.arrays.edgeFlagArray.stride == stride &&
      state->client_state.arrays.edgeFlagArray.ptr == ptr)
  {
  }
  else
  {
    if (debug_array_ptr)
      log_gl("edgeFlagArray stride=%d ptr=%p\n", stride, ptr);
    state->client_state.arrays.edgeFlagArray.size = 1;
    state->client_state.arrays.edgeFlagArray.type = GL_BYTE;
    state->client_state.arrays.edgeFlagArray.stride = stride;
    state->client_state.arrays.edgeFlagArray.ptr = ptr;
  }
}

DEFINE_EXT(glEdgeFlagPointer, (GLsizei stride, const GLvoid *ptr), (stride, ptr));

GLAPI void APIENTRY glFogCoordPointer(GLenum type, GLsizei stride, const GLvoid *ptr)
{
  CHECK_PROC(glFogCoordPointer);
  GET_CURRENT_STATE();

  state->client_state.arrays.fogCoordArray.vbo_name = state->arrayBuffer;
  if (state->client_state.arrays.fogCoordArray.vbo_name)
  {
    long args[] = { INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
    do_opengl_call(_glFogCoordPointer_buffer_func, NULL, args, NULL);
    return;
  }

  if (state->client_state.arrays.fogCoordArray.type == type &&
      state->client_state.arrays.fogCoordArray.stride == stride &&
      state->client_state.arrays.fogCoordArray.ptr == ptr)
  {
  }
  else
  {
    if (debug_array_ptr)
      log_gl("glFogCoordPointer type=%d stride=%d ptr=%p\n", type, stride, ptr);
    state->client_state.arrays.fogCoordArray.size = 1;
    state->client_state.arrays.fogCoordArray.type = type;
    state->client_state.arrays.fogCoordArray.stride = stride;
    state->client_state.arrays.fogCoordArray.ptr = ptr;
  }
}

DEFINE_EXT(glFogCoordPointer, (GLenum type, GLsizei stride, const GLvoid *ptr), (type, stride, ptr));

GLAPI void APIENTRY glWeightPointerARB (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
  CHECK_PROC(glWeightPointerARB);
  GET_CURRENT_STATE();

  state->client_state.arrays.weightArray.vbo_name = state->arrayBuffer;
  if (state->client_state.arrays.weightArray.vbo_name)
  {
    long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
    do_opengl_call(_glWeightPointerARB_buffer_func, NULL, args, NULL);
    return;
  }

  log_gl("glWeightPointerARB\n");
  fflush(get_err_file());
  if (debug_array_ptr)
      log_gl("weightArray size=%d type=%d stride=%d ptr=%p\n", size, type, stride, ptr);
  state->client_state.arrays.weightArray.size = size;
  state->client_state.arrays.weightArray.type = type;
  state->client_state.arrays.weightArray.stride = stride;
  state->client_state.arrays.weightArray.ptr = ptr;
}


GLAPI void APIENTRY glMatrixIndexPointerARB (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
  CHECK_PROC(glMatrixIndexPointerARB);
  GET_CURRENT_STATE();

  state->client_state.arrays.matrixIndexArray.vbo_name = state->arrayBuffer;
  if (state->client_state.arrays.matrixIndexArray.vbo_name)
  {
    long args[] = { INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
    do_opengl_call(_glMatrixIndexPointerARB_buffer_func, NULL, args, NULL);
    return;
  }

  log_gl("glMatrixIndexPointerARB\n");
  fflush(get_err_file());
  if (debug_array_ptr)
      log_gl("matrixIndexArray size=%d type=%d stride=%d ptr=%p\n", size, type, stride, ptr);
  state->client_state.arrays.matrixIndexArray.size = size;
  state->client_state.arrays.matrixIndexArray.type = type;
  state->client_state.arrays.matrixIndexArray.stride = stride;
  state->client_state.arrays.matrixIndexArray.ptr = ptr;
}

#define glMatrixIndexvARB(name, type) \
GLAPI void APIENTRY name (GLint size, const type *indices) \
{ \
  CHECK_PROC(name); \
  long args[] = { INT_TO_ARG(size), POINTER_TO_ARG(indices) }; \
  int args_size[] = { 0, size * sizeof(type) }; \
  do_opengl_call(CONCAT(name, _func), NULL, CHECK_ARGS(args, args_size)); \
}

glMatrixIndexvARB(glMatrixIndexubvARB, GLubyte);
glMatrixIndexvARB(glMatrixIndexusvARB, GLushort);
glMatrixIndexvARB(glMatrixIndexuivARB, GLuint);


GLAPI void APIENTRY glVertexWeightfEXT (GLfloat weight)
{
  log_gl("glVertexWeightfEXT : deprecated API. unimplemented\n");
}

GLAPI void APIENTRY glVertexWeightfvEXT (const GLfloat *weight)
{
  log_gl("glVertexWeightfvEXT : deprecated API. unimplemented\n");
}

GLAPI void APIENTRY glVertexWeightPointerEXT (GLsizei size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
  log_gl("glVertexWeightPointerEXT : deprecated API. unimplemented\n");
}



GLAPI void APIENTRY EXT_FUNC(glVertexAttribPointerARB)(GLuint index,
                                        GLint size,
                                        GLenum type,
                                        GLboolean normalized,
                                        GLsizei stride,
                                        const GLvoid *ptr)
{
  CHECK_PROC(glVertexAttribPointerARB);

  GET_CURRENT_STATE();
  if (index < MY_GL_MAX_VERTEX_ATTRIBS_ARB)
  {
    state->client_state.arrays.vertexAttribArray[index].vbo_name = state->arrayBuffer;
    if (state->client_state.arrays.vertexAttribArray[index].vbo_name)
    {
      long args[] = { INT_TO_ARG(index), INT_TO_ARG(size), INT_TO_ARG(type), INT_TO_ARG(normalized), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
      do_opengl_call(_glVertexAttribPointerARB_buffer_func, NULL, args, NULL);
      return;
    }

    if (debug_array_ptr)
      log_gl("glVertexAttribPointerARB[%d] size=%d type=%d normalized=%d stride=%d ptr=%p\n",
              index, size, type, normalized, stride, ptr);
    state->client_state.arrays.vertexAttribArray[index].index = index;
    state->client_state.arrays.vertexAttribArray[index].size = size;
    state->client_state.arrays.vertexAttribArray[index].type = type;
    state->client_state.arrays.vertexAttribArray[index].normalized = normalized;
    state->client_state.arrays.vertexAttribArray[index].stride = stride;
    state->client_state.arrays.vertexAttribArray[index].ptr = ptr;
  }
  else
  {
    log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
  }
}

GLAPI void APIENTRY EXT_FUNC(glVertexAttribPointer)(GLuint index,
                                        GLint size,
                                        GLenum type,
                                        GLboolean normalized,
                                        GLsizei stride,
                                        const GLvoid *ptr)
{
  glVertexAttribPointerARB(index, size, type, normalized, stride, ptr);
}

GLAPI void APIENTRY EXT_FUNC(glVertexAttribPointerNV)(GLuint index,
                                                      GLint size,
                                                      GLenum type,
                                                      GLsizei stride,
                                                      const GLvoid *ptr)
{
  CHECK_PROC(glVertexAttribPointerNV);

  GET_CURRENT_STATE();
  if (index < MY_GL_MAX_VERTEX_ATTRIBS_NV)
  {
    if (debug_array_ptr)
      log_gl("glVertexAttribPointerNV[%d] size=%d type=%d stride=%d ptr=%p\n",
              index, size, type, stride, ptr);
    state->client_state.arrays.vertexAttribArrayNV[index].index = index;
    state->client_state.arrays.vertexAttribArrayNV[index].size = size;
    state->client_state.arrays.vertexAttribArrayNV[index].type = type;
    state->client_state.arrays.vertexAttribArrayNV[index].stride = stride;
    state->client_state.arrays.vertexAttribArrayNV[index].ptr = ptr;
  }
  else
  {
    log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_NV\n");
  }
}

GLAPI void APIENTRY EXT_FUNC(glElementPointerATI) (GLenum type, const GLvoid * ptr)
{
  CHECK_PROC(glElementPointerATI);
  GET_CURRENT_STATE();
  state->client_state.arrays.elementArrayATI.size = 1;
  state->client_state.arrays.elementArrayATI.type = type;
  state->client_state.arrays.elementArrayATI.stride = 0;
  state->client_state.arrays.elementArrayATI.ptr = ptr;
}

GLAPI void APIENTRY glGetPointerv( GLenum pname, void **params )
{
  GET_CURRENT_STATE();
  switch (pname)
  {
    case GL_COLOR_ARRAY_POINTER: *params = (void*)state->client_state.arrays.colorArray.ptr; break;
    case GL_SECONDARY_COLOR_ARRAY_POINTER: *params = (void*)state->client_state.arrays.secondaryColorArray.ptr; break;
    case GL_NORMAL_ARRAY_POINTER: *params = (void*)state->client_state.arrays.normalArray.ptr; break;
    case GL_INDEX_ARRAY_POINTER: *params = (void*)state->client_state.arrays.indexArray.ptr; break;
    case GL_TEXTURE_COORD_ARRAY_POINTER: *params = (void*)state->client_state.arrays.texCoordArray[state->client_state.clientActiveTexture].ptr; break;
    case GL_VERTEX_ARRAY_POINTER: *params = (void*)state->client_state.arrays.vertexArray.ptr; break;
    case GL_EDGE_FLAG_ARRAY_POINTER: *params = (void*)state->client_state.arrays.edgeFlagArray.ptr; break;
    case GL_WEIGHT_ARRAY_POINTER_ARB: *params = (void*)state->client_state.arrays.weightArray.ptr; break;
    case GL_MATRIX_INDEX_ARRAY_POINTER_ARB: *params = (void*)state->client_state.arrays.matrixIndexArray.ptr; break;
    case GL_FOG_COORD_ARRAY_POINTER: *params = (void*)state->client_state.arrays.fogCoordArray.ptr; break;
    case GL_ELEMENT_ARRAY_POINTER_ATI: *params = (void*)state->client_state.arrays.elementArrayATI.ptr; break;
    case GL_SELECTION_BUFFER_POINTER: *params = (void*)state->client_state.selectBufferPtr; break;
    case GL_FEEDBACK_BUFFER_POINTER: *params = (void*)state->client_state.feedbackBufferPtr; break;
    default:
    {
      log_gl("not yet handled pname %d\n", pname);
      *params = NULL;
    }
  }
}

GLAPI void APIENTRY glGetPointervEXT( GLenum pname, void **params )
{
  glGetPointerv(pname, params);
}

GLAPI void APIENTRY EXT_FUNC(glGetVertexAttribPointervARB)(GLuint index,
                                            GLenum pname,
                                            GLvoid **pointer)
{
  GET_CURRENT_STATE();

  if (index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB)
  {
    log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n");
    return;
  }

  switch (pname)
  {
    case GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB:
    {
      *pointer = (void*)state->client_state.arrays.vertexAttribArray[index].ptr;
      break;
    }
    default:
      log_gl("glGetVertexAttribPointervARB : bad pname=0x%X", pname);
      break;
  }
}

GLAPI void APIENTRY EXT_FUNC(glGetVertexAttribPointerv)(GLuint index,
                                            GLenum pname,
                                            GLvoid **pointer)
{
  glGetVertexAttribPointervARB(index, pname, pointer);
}

GLAPI void APIENTRY EXT_FUNC(_glGetVertexAttribiv)(int func_number, GLuint index, GLenum pname, GLint *params)
{
  if (index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB)
  {
    log_gl("%s: index >= MY_GL_MAX_VERTEX_ATTRIBS_ARB\n", tab_opengl_calls_name[func_number]);
    return;
  }

  GET_CURRENT_STATE();
  switch (pname)
  {
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB:
      *params = state->client_state.arrays.vertexAttribArray[index].enabled;
      break;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB:
      *params = state->client_state.arrays.vertexAttribArray[index].size;
      break;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB:
      *params = state->client_state.arrays.vertexAttribArray[index].stride;
      break;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB:
      *params = state->client_state.arrays.vertexAttribArray[index].type;
      break;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB:
      *params = state->client_state.arrays.vertexAttribArray[index].normalized;
      break;
    case GL_CURRENT_VERTEX_ATTRIB_ARB:
      *params = state->client_state.arrays.vertexAttribArray[index].vbo_name;
      break;
    default:
      log_gl("%s : bad pname=0x%X", tab_opengl_calls_name[func_number], pname);
      break;
  }
}

#define DEFINE_glGetVertexAttribv(funcName, type) \
GLAPI void APIENTRY EXT_FUNC(funcName)(GLuint index, GLenum pname, type *params) \
{ \
  CHECK_PROC(funcName); \
  if (pname == GL_CURRENT_VERTEX_ATTRIB_ARB) \
  { \
    long args[] = { UNSIGNED_INT_TO_ARG(index), UNSIGNED_INT_TO_ARG(pname), POINTER_TO_ARG(params)}; \
    do_opengl_call(CONCAT(funcName,_func), NULL, args, NULL); \
    return; \
  } \
  int i_params; \
  _glGetVertexAttribiv(CONCAT(funcName,_func), index, pname, &i_params); \
  *params = i_params; \
}

DEFINE_glGetVertexAttribv(glGetVertexAttribivARB, GLint);
DEFINE_glGetVertexAttribv(glGetVertexAttribiv, GLint);
DEFINE_glGetVertexAttribv(glGetVertexAttribfvARB, GLfloat);
DEFINE_glGetVertexAttribv(glGetVertexAttribfv, GLfloat);
DEFINE_glGetVertexAttribv(glGetVertexAttribdvARB, GLdouble);
DEFINE_glGetVertexAttribv(glGetVertexAttribdv, GLdouble);


GLAPI void APIENTRY EXT_FUNC(glGetVertexAttribPointervNV)(GLuint index,
                                            GLenum pname,
                                            GLvoid **pointer)
{
  CHECK_PROC(glGetVertexAttribPointervNV);
  GET_CURRENT_STATE();
  if (index >= MY_GL_MAX_VERTEX_ATTRIBS_NV)
  {
    log_gl("index >= MY_GL_MAX_VERTEX_ATTRIBS_NV\n");
    return;
  }
  switch (pname)
  {
    case GL_ATTRIB_ARRAY_POINTER_NV:
    {
      *pointer = (void*)state->client_state.arrays.vertexAttribArrayNV[index].ptr;
      break;
    }
    default:
      log_gl("glGetVertexAttribPointervNV : bad pname=0x%X", pname);
      break;
  }
}

static int getGlTypeByteSize(int type)
{
  switch(type)
  {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
      return 1;
      break;

    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
      return 2;
      break;

    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
      return 4;
      break;

    case GL_DOUBLE:
      return 8;
      break;

    default:
      log_gl("unsupported type = %d\n", type);
      return 0;
  }
}

static int getMulFactorFromPointerArray(ClientArray* array)
{
  if (array->stride)
    return array->stride;

  return getGlTypeByteSize(array->type) * array->size;
}


static void _glArraySend(GLState* state, const char* func_name, int func, ClientArray* array, int first, int last)
{
  if (array->ptr == NULL || array->vbo_name || array->enabled == 0) return;
  int offset = first * getMulFactorFromPointerArray(array);
  int size = (last - first + 1) * getMulFactorFromPointerArray(array);
  if (size == 0) return;

#if 0
  unsigned int crc = calc_checksum(array->ptr + offset, size, 0xFFFFFFFF);
  crc = calc_checksum(&offset, sizeof(int), crc);
  crc = calc_checksum(&size, sizeof(int), crc);
  crc = calc_checksum(&array->size, sizeof(int), crc);
  crc = calc_checksum(&array->type, sizeof(int), crc);

  if (crc == 0)
  {
    /*int i;
    unsigned char* ptr = (unsigned char*)(array->ptr + offset);
    for(i=0;i<size;i++)
    {
      log_gl("%d ", (int)ptr[i]);
    }*/
    log_gl("strange : crc = 0\n");
  }

  if (crc == array->last_crc)
  {
    if (debug_array_ptr)
    {
      log_gl("%s : same crc. Saving %d bytes\n", func_name, size);
    }
    return;
  }

  array->last_crc = crc;
#endif

  if (debug_array_ptr)
  {
    unsigned int crc = calc_checksum(array->ptr + offset, size, 0xFFFFFFFF);
    crc = calc_checksum(&offset, sizeof(int), crc);
    crc = calc_checksum(&size, sizeof(int), crc);
    crc = calc_checksum(&array->size, sizeof(int), crc);
    crc = calc_checksum(&array->type, sizeof(int), crc);

    log_gl("%s sending %d bytes from %d : crc = %d\n", func_name, size, offset, crc);
  }

  int currentArrayBuffer = state->arrayBuffer;
  if (currentArrayBuffer)
  {
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
  }
  switch(func)
  {
    case glEdgeFlagPointer_fake_func:
    {
      long args[] = { INT_TO_ARG(offset),
                      INT_TO_ARG(array->stride),
                      INT_TO_ARG(size),
                      POINTER_TO_ARG(array->ptr + offset) };
      int args_size[] = { 0, 0, 0, size};
      do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
      break;
    }

    case glNormalPointer_fake_func:
    case glIndexPointer_fake_func:
    case glFogCoordPointer_fake_func:
    {
      long args[] = { INT_TO_ARG(offset),
                      INT_TO_ARG(array->type),
                      INT_TO_ARG(array->stride),
                      INT_TO_ARG(size),
                      POINTER_TO_ARG(array->ptr + offset) };
      int args_size[] = { 0, 0, 0, 0, size};
      do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
      break;
    }

    case glVertexPointer_fake_func:
    case glColorPointer_fake_func:
    case glSecondaryColorPointer_fake_func:
    case glWeightPointerARB_fake_func:
    case glMatrixIndexPointerARB_fake_func:
    {
      long args[] = { INT_TO_ARG(offset),
                      INT_TO_ARG(array->size),
                      INT_TO_ARG(array->type),
                      INT_TO_ARG(array->stride),
                      INT_TO_ARG(size),
                      POINTER_TO_ARG(array->ptr + offset) };
      int args_size[] = { 0, 0, 0, 0, 0, size};
      do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
      break;
    }

    case glTexCoordPointer_fake_func:
    case glVertexAttribPointerNV_fake_func:
    {
      long args[] = { INT_TO_ARG(offset),
                      INT_TO_ARG(array->index),
                      INT_TO_ARG(array->size),
                      INT_TO_ARG(array->type),
                      INT_TO_ARG(array->stride),
                      INT_TO_ARG(size),
                      POINTER_TO_ARG(array->ptr + offset) };
      int args_size[] = { 0, 0, 0, 0, 0, 0, size};
      do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
      break;
    }

    case glVertexAttribPointerARB_fake_func:
    {
      long args[] = { INT_TO_ARG(offset),
                      INT_TO_ARG(array->index),
                      INT_TO_ARG(array->size),
                      INT_TO_ARG(array->type),
                      INT_TO_ARG(array->normalized),
                      INT_TO_ARG(array->stride),
                      INT_TO_ARG(size),
                      POINTER_TO_ARG(array->ptr + offset) };
      int args_size[] = { 0, 0, 0, 0, 0, 0, 0, size};
      do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
      break;
    }

    case glVariantPointerEXT_fake_func:
    {
      long args[] = { INT_TO_ARG(offset),
                      INT_TO_ARG(array->index),
                      INT_TO_ARG(array->type),
                      INT_TO_ARG(array->stride),
                      INT_TO_ARG(size),
                      POINTER_TO_ARG(array->ptr + offset) };
      int args_size[] = { 0, 0, 0, 0, 0, size};
      do_opengl_call(func, NULL, CHECK_ARGS(args, args_size));
      break;
    }

    default:
      log_gl("shoudln't reach that point\n");
      break;
  }

  if (currentArrayBuffer)
  {
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, currentArrayBuffer);
  }
}


#define ARRAY_SEND_FUNC(x)  #x, CONCAT(x,_fake_func)

static int _calc_interleaved_arrays_stride(GLenum format)
{
  switch(format)
  {
    case GL_V2F: return 2 * sizeof(float); break;
    case GL_V3F: return 3 * sizeof(float); break;
    case GL_C4UB_V2F: return 4 * sizeof(char) + 2 * sizeof(float); break;
    case GL_C4UB_V3F: return 4 * sizeof(char) + 3 * sizeof(float); break;
    case GL_C3F_V3F: return 3 * sizeof(float) + 3 * sizeof(float); break;
    case GL_N3F_V3F: return 3 * sizeof(float) + 3 * sizeof(float); break;
    case GL_C4F_N3F_V3F: return 4 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float); break;
    case GL_T2F_V3F: return 2 * sizeof(float) + 3 * sizeof(float); break;
    case GL_T4F_V4F: return 4 * sizeof(float) + 4 * sizeof(float); break;
    case GL_T2F_C4UB_V3F: return 2 * sizeof(float) + 4 * sizeof(char) + 3 * sizeof(float); break;
    case GL_T2F_C3F_V3F: return 2 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float); break;
    case GL_T2F_N3F_V3F: return 2 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float); break;
    case GL_T2F_C4F_N3F_V3F: return 2 * sizeof(float) + 4 * sizeof(float) + 3 * sizeof(float) + 3 * sizeof(float); break;
    case GL_T4F_C4F_N3F_V4F: return 4 * sizeof(float) + 4 * sizeof(float) + 3 * sizeof(float) + 4 * sizeof(float); break;
    default: log_gl("unknown interleaved array format : %d\n", format); return 0;
  }
}

enum
{
  TEXCOORD_FUNC,
  COLOR_FUNC,
  NORMAL_FUNC,
  INDEX_FUNC,
  VERTEX_FUNC
};

typedef void (*vector_func_type)(const void*);

typedef struct
{
  int type;
  int size;
  int typeFunc;
  vector_func_type vector_func;
} VectorFuncStruct;

VectorFuncStruct vectorFuncArray[] =
{
  { GL_DOUBLE, 1, TEXCOORD_FUNC, (vector_func_type)glTexCoord1dv },
  { GL_DOUBLE, 2, TEXCOORD_FUNC, (vector_func_type)glTexCoord2dv },
  { GL_DOUBLE, 3, TEXCOORD_FUNC, (vector_func_type)glTexCoord3dv },
  { GL_DOUBLE, 4, TEXCOORD_FUNC, (vector_func_type)glTexCoord4dv },
  { GL_FLOAT, 1, TEXCOORD_FUNC, (vector_func_type)glTexCoord1fv },
  { GL_FLOAT, 2, TEXCOORD_FUNC, (vector_func_type)glTexCoord2fv },
  { GL_FLOAT, 3, TEXCOORD_FUNC, (vector_func_type)glTexCoord3fv },
  { GL_FLOAT, 4, TEXCOORD_FUNC, (vector_func_type)glTexCoord4fv },
  { GL_INT, 1, TEXCOORD_FUNC, (vector_func_type)glTexCoord1iv },
  { GL_INT, 2, TEXCOORD_FUNC, (vector_func_type)glTexCoord2iv },
  { GL_INT, 3, TEXCOORD_FUNC, (vector_func_type)glTexCoord3iv },
  { GL_INT, 4, TEXCOORD_FUNC, (vector_func_type)glTexCoord4iv },
  { GL_SHORT, 1, TEXCOORD_FUNC, (vector_func_type)glTexCoord1sv },
  { GL_SHORT, 2, TEXCOORD_FUNC, (vector_func_type)glTexCoord2sv },
  { GL_SHORT, 3, TEXCOORD_FUNC, (vector_func_type)glTexCoord3sv },
  { GL_SHORT, 4, TEXCOORD_FUNC, (vector_func_type)glTexCoord4sv },

  { GL_BYTE, 3, COLOR_FUNC, (vector_func_type)glColor3bv },
  { GL_DOUBLE, 3, COLOR_FUNC, (vector_func_type)glColor3dv },
  { GL_FLOAT, 3, COLOR_FUNC, (vector_func_type)glColor3fv },
  { GL_INT, 3, COLOR_FUNC, (vector_func_type)glColor3iv },
  { GL_SHORT, 3, COLOR_FUNC, (vector_func_type)glColor3sv },
  { GL_UNSIGNED_BYTE, 3, COLOR_FUNC, (vector_func_type)glColor3ubv },
  { GL_UNSIGNED_INT, 3, COLOR_FUNC, (vector_func_type)glColor3uiv },
  { GL_UNSIGNED_SHORT, 3, COLOR_FUNC, (vector_func_type)glColor3usv },
  { GL_BYTE, 4, COLOR_FUNC, (vector_func_type)glColor4bv },
  { GL_DOUBLE, 4, COLOR_FUNC, (vector_func_type)glColor4dv },
  { GL_FLOAT, 4, COLOR_FUNC, (vector_func_type)glColor4fv },
  { GL_INT, 4, COLOR_FUNC, (vector_func_type)glColor4iv },
  { GL_SHORT, 4, COLOR_FUNC, (vector_func_type)glColor4sv },
  { GL_UNSIGNED_BYTE, 4, COLOR_FUNC, (vector_func_type)glColor4ubv },
  { GL_UNSIGNED_INT, 4, COLOR_FUNC, (vector_func_type)glColor4uiv },
  { GL_UNSIGNED_SHORT, 4, COLOR_FUNC, (vector_func_type)glColor4usv },

  { GL_BYTE, 3, NORMAL_FUNC, (vector_func_type)glNormal3bv },
  { GL_DOUBLE, 3, NORMAL_FUNC, (vector_func_type)glNormal3dv },
  { GL_FLOAT, 3, NORMAL_FUNC, (vector_func_type)glNormal3fv },
  { GL_INT, 3, NORMAL_FUNC, (vector_func_type)glNormal3iv },
  { GL_SHORT, 3, NORMAL_FUNC, (vector_func_type)glNormal3sv },

  { GL_DOUBLE, 1, INDEX_FUNC, (vector_func_type)glIndexdv },
  { GL_FLOAT, 1, INDEX_FUNC, (vector_func_type)glIndexfv },
  { GL_INT, 1, INDEX_FUNC, (vector_func_type)glIndexiv },
  { GL_SHORT, 1, INDEX_FUNC, (vector_func_type)glIndexsv },
  { GL_UNSIGNED_BYTE, 1, INDEX_FUNC, (vector_func_type)glIndexubv },

  { GL_DOUBLE, 2, VERTEX_FUNC, (vector_func_type)glVertex2dv },
  { GL_FLOAT, 2, VERTEX_FUNC, (vector_func_type)glVertex2fv },
  { GL_INT, 2, VERTEX_FUNC, (vector_func_type)glVertex2iv },
  { GL_SHORT, 2, VERTEX_FUNC, (vector_func_type)glVertex2sv },
  { GL_DOUBLE, 3, VERTEX_FUNC, (vector_func_type)glVertex3dv },
  { GL_FLOAT, 3, VERTEX_FUNC, (vector_func_type)glVertex3fv },
  { GL_INT, 3, VERTEX_FUNC, (vector_func_type)glVertex3iv },
  { GL_SHORT, 4, VERTEX_FUNC, (vector_func_type)glVertex4sv },
  { GL_DOUBLE, 4, VERTEX_FUNC, (vector_func_type)glVertex4dv },
  { GL_FLOAT, 4, VERTEX_FUNC, (vector_func_type)glVertex4fv },
  { GL_INT, 4, VERTEX_FUNC, (vector_func_type)glVertex4iv },
  { GL_SHORT, 4, VERTEX_FUNC, (vector_func_type)glVertex4sv },
};

static vector_func_type _get_vector_func(int type, int size, int typeFunc)
{
  int i;
  for(i=0;i<sizeof(vectorFuncArray)/sizeof(vectorFuncArray[0]);i++)
  {
    if (vectorFuncArray[i].type == type &&
        vectorFuncArray[i].size == size &&
        vectorFuncArray[i].typeFunc == typeFunc)
      return vectorFuncArray[i].vector_func;
  }
  log_gl("can't find vector_func(type=%X, size=%d, typeFunc=%d)\n", type, size, typeFunc);
  return NULL;
}

static void _glElementArrayImmediate_one(ClientArray* array, int indice, int typeFunc)
{
  if (array->enabled)
  {
    vector_func_type vector_func =
          _get_vector_func(array->type, array->size, typeFunc);
    if (vector_func)
    {
      vector_func(array->ptr + getMulFactorFromPointerArray(array) * indice);
    }
  }
}

static void _glElementArrayImmediate(int indice)
{
  GET_CURRENT_STATE();

  if (state->client_state.arrays.interleavedArrays.ptr != NULL &&
      state->client_state.arrays.vertexArray.ptr == NULL)
  {
    GLboolean tflag, cflag, nflag;  /* enable/disable flags */
    GLint tcomps, ccomps, vcomps;   /* components per texcoord, color, vertex */
    GLenum ctype = 0;               /* color type */
    GLint coffset = 0, noffset = 0, voffset;/* color, normal, vertex offsets */
    const GLint toffset = 0;        /* always zero */
    GLint defstride;                /* default stride */
    GLint c, f;

    int stride = state->client_state.arrays.interleavedArrays.stride;

    f = sizeof(GLfloat);
    c = f * ((4 * sizeof(GLubyte) + (f - 1)) / f);

    switch (state->client_state.arrays.interleavedArrays.format) {
        case GL_V2F:
          tflag = GL_FALSE;  cflag = GL_FALSE;  nflag = GL_FALSE;
          tcomps = 0;  ccomps = 0;  vcomps = 2;
          voffset = 0;
          defstride = 2*f;
          break;
        case GL_V3F:
          tflag = GL_FALSE;  cflag = GL_FALSE;  nflag = GL_FALSE;
          tcomps = 0;  ccomps = 0;  vcomps = 3;
          voffset = 0;
          defstride = 3*f;
          break;
        case GL_C4UB_V2F:
          tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_FALSE;
          tcomps = 0;  ccomps = 4;  vcomps = 2;
          ctype = GL_UNSIGNED_BYTE;
          coffset = 0;
          voffset = c;
          defstride = c + 2*f;
          break;
        case GL_C4UB_V3F:
          tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_FALSE;
          tcomps = 0;  ccomps = 4;  vcomps = 3;
          ctype = GL_UNSIGNED_BYTE;
          coffset = 0;
          voffset = c;
          defstride = c + 3*f;
          break;
        case GL_C3F_V3F:
          tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_FALSE;
          tcomps = 0;  ccomps = 3;  vcomps = 3;
          ctype = GL_FLOAT;
          coffset = 0;
          voffset = 3*f;
          defstride = 6*f;
          break;
        case GL_N3F_V3F:
          tflag = GL_FALSE;  cflag = GL_FALSE;  nflag = GL_TRUE;
          tcomps = 0;  ccomps = 0;  vcomps = 3;
          noffset = 0;
          voffset = 3*f;
          defstride = 6*f;
          break;
        case GL_C4F_N3F_V3F:
          tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_TRUE;
          tcomps = 0;  ccomps = 4;  vcomps = 3;
          ctype = GL_FLOAT;
          coffset = 0;
          noffset = 4*f;
          voffset = 7*f;
          defstride = 10*f;
          break;
        case GL_T2F_V3F:
          tflag = GL_TRUE;  cflag = GL_FALSE;  nflag = GL_FALSE;
          tcomps = 2;  ccomps = 0;  vcomps = 3;
          voffset = 2*f;
          defstride = 5*f;
          break;
        case GL_T4F_V4F:
          tflag = GL_TRUE;  cflag = GL_FALSE;  nflag = GL_FALSE;
          tcomps = 4;  ccomps = 0;  vcomps = 4;
          voffset = 4*f;
          defstride = 8*f;
          break;
        case GL_T2F_C4UB_V3F:
          tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_FALSE;
          tcomps = 2;  ccomps = 4;  vcomps = 3;
          ctype = GL_UNSIGNED_BYTE;
          coffset = 2*f;
          voffset = c+2*f;
          defstride = c+5*f;
          break;
        case GL_T2F_C3F_V3F:
          tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_FALSE;
          tcomps = 2;  ccomps = 3;  vcomps = 3;
          ctype = GL_FLOAT;
          coffset = 2*f;
          voffset = 5*f;
          defstride = 8*f;
          break;
        case GL_T2F_N3F_V3F:
          tflag = GL_TRUE;  cflag = GL_FALSE;  nflag = GL_TRUE;
          tcomps = 2;  ccomps = 0;  vcomps = 3;
          noffset = 2*f;
          voffset = 5*f;
          defstride = 8*f;
          break;
        case GL_T2F_C4F_N3F_V3F:
          tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_TRUE;
          tcomps = 2;  ccomps = 4;  vcomps = 3;
          ctype = GL_FLOAT;
          coffset = 2*f;
          noffset = 6*f;
          voffset = 9*f;
          defstride = 12*f;
          break;
        case GL_T4F_C4F_N3F_V4F:
          tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_TRUE;
          tcomps = 4;  ccomps = 4;  vcomps = 4;
          ctype = GL_FLOAT;
          coffset = 4*f;
          noffset = 8*f;
          voffset = 11*f;
          defstride = 15*f;
          break;
        default:
          log_gl("unknown interleaved array format : %d\n", state->client_state.arrays.interleavedArrays.format);
          return;
    }
    if (stride==0) {
        stride = defstride;
    }

    const void* ptr = state->client_state.arrays.interleavedArrays.ptr + indice * stride;

    if (tflag)
    {
      if (tcomps == 2)
        glTexCoord2fv(ptr + toffset);
      else if (tcomps == 4)
        glTexCoord4fv(ptr + toffset);
      else
        assert(0);
    }

    if (cflag)
    {
      if (ctype == GL_FLOAT)
      {
        if (ccomps == 3)
          glColor3fv(ptr + coffset);
        else if (ccomps == 4)
          glColor4fv(ptr + coffset);
        else
          assert(0);
      }
      else if (ctype == GL_UNSIGNED_BYTE)
      {
        if (ccomps == 4)
          glColor4ubv(ptr + coffset);
        else
          assert(0);
      }
      else
        assert(0);
    }

    if (nflag)
      glNormal3fv(ptr + noffset);

    if (vcomps == 2)
      glVertex2fv(ptr + voffset);
    else if (vcomps == 3)
      glVertex3fv(ptr + voffset);
    else if (vcomps == 4)
      glVertex4fv(ptr + voffset);
    else
      assert(0);

    return;
  }
  int i;

  int prevActiveTexture = state->activeTexture;
  for(i=0;i<NB_MAX_TEXTURES;i++)
  {
    if (state->client_state.arrays.texCoordArray[i].enabled)
    {
      if (i != 0 || state->activeTexture != GL_TEXTURE0_ARB)
        glActiveTextureARB(GL_TEXTURE0_ARB + i);
      _glElementArrayImmediate_one(&state->client_state.arrays.texCoordArray[i], indice, TEXCOORD_FUNC);
    }
  }
  glActiveTextureARB(prevActiveTexture);

  _glElementArrayImmediate_one(&state->client_state.arrays.normalArray, indice, NORMAL_FUNC);
  _glElementArrayImmediate_one(&state->client_state.arrays.colorArray, indice, COLOR_FUNC);
  _glElementArrayImmediate_one(&state->client_state.arrays.indexArray, indice, INDEX_FUNC);
  _glElementArrayImmediate_one(&state->client_state.arrays.vertexArray, indice, VERTEX_FUNC);
}

static void _glArraysSend(int first, int last)
{
  GET_CURRENT_STATE();
  if (debug_array_ptr)
  {
    log_gl("_glArraysSend from %d to %d\n", first, last);
  }


  int startIndiceTextureToDealtWith = 0;
  int i;
  int nbElts = 1 + last;

  if (glIsEnabled(GL_VERTEX_PROGRAM_NV))
  {
    int vertexProgramNV = 0;
    for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_NV;i++)
    {
      ClientArray* array = &state->client_state.arrays.vertexAttribArrayNV[i];
      if (!(array->ptr == NULL || array->enabled == 0))
      {
        _glArraySend(state, ARRAY_SEND_FUNC(glVertexAttribPointerNV), array, first, last);
        vertexProgramNV = 1;
      }
    }
    if (vertexProgramNV)
      return;
  }

  if (state->client_state.arrays.interleavedArrays.ptr != NULL &&
      state->client_state.arrays.vertexArray.ptr == NULL)
  {
    int stride;
    if (state->client_state.arrays.interleavedArrays.stride)
      stride = state->client_state.arrays.interleavedArrays.stride;
    else
      stride = _calc_interleaved_arrays_stride(state->client_state.arrays.interleavedArrays.format);
    if (stride == 0) return;
    int offset = stride * first;
    int size = (last - first + 1) * stride;
    long args[] = { offset,
                    state->client_state.arrays.interleavedArrays.format,
                    state->client_state.arrays.interleavedArrays.stride,
                    size,
                    POINTER_TO_ARG(state->client_state.arrays.interleavedArrays.ptr + offset) };
    int args_size[] = { 0, 0, 0, 0, size };
    do_opengl_call(glInterleavedArrays_fake_func, NULL, CHECK_ARGS(args, args_size));
    return;
  }
  int normalArrayOffset = (long)state->client_state.arrays.normalArray.ptr -
                            (long)state->client_state.arrays.vertexArray.ptr;
  int colorArrayOffset = (long)state->client_state.arrays.colorArray.ptr -
                            (long)state->client_state.arrays.vertexArray.ptr;
  int texCoord0PointerOffset = (long)state->client_state.arrays.texCoordArray[0].ptr -
                            (long)state->client_state.arrays.vertexArray.ptr;
  int texCoord1PointerOffset = (long)state->client_state.arrays.texCoordArray[1].ptr -
                            (long)state->client_state.arrays.vertexArray.ptr;
  int texCoord2PointerOffset = (long)state->client_state.arrays.texCoordArray[2].ptr -
                            (long)state->client_state.arrays.vertexArray.ptr;

  if (state->client_state.arrays.vertexArray.vbo_name ||
      state->client_state.arrays.colorArray.vbo_name ||
      state->client_state.arrays.normalArray.vbo_name ||
      state->client_state.arrays.texCoordArray[0].vbo_name ||
      state->client_state.arrays.texCoordArray[1].vbo_name ||
      state->client_state.arrays.texCoordArray[2].vbo_name)
  {
    _glArraySend(state, ARRAY_SEND_FUNC(glVertexPointer), &state->client_state.arrays.vertexArray, first, last);
    _glArraySend(state, ARRAY_SEND_FUNC(glNormalPointer), &state->client_state.arrays.normalArray, first, last);
    _glArraySend(state, ARRAY_SEND_FUNC(glColorPointer), &state->client_state.arrays.colorArray, first, last);
  }
  else
  if (state->client_state.arrays.vertexArray.enabled &&
      state->client_state.arrays.normalArray.enabled == 0 &&
      state->client_state.arrays.colorArray.enabled &&
      state->client_state.arrays.texCoordArray[0].enabled &&
      state->client_state.arrays.vertexArray.ptr != NULL &&
      state->client_state.arrays.vertexArray.stride != 0 &&
      state->client_state.arrays.vertexArray.stride == state->client_state.arrays.colorArray.stride &&
      state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
      colorArrayOffset >= 0 &&
      colorArrayOffset < state->client_state.arrays.vertexArray.stride &&
      texCoord0PointerOffset >= 0 &&
      texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride)
  {
    /* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
    int offset = first * state->client_state.arrays.vertexArray.stride;
    int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

    if (debug_array_ptr)
    {
      log_gl("glVertexColorTexCoord0PointerInterlaced_fake_func sending %d bytes from %d\n",
              data_size,
              offset);
    }
    long args[] = { INT_TO_ARG(offset),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.size),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.type),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
                    INT_TO_ARG(colorArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.colorArray.size),
                    INT_TO_ARG(state->client_state.arrays.colorArray.type),
                    INT_TO_ARG(texCoord0PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
    do_opengl_call(glVertexColorTexCoord0PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

    startIndiceTextureToDealtWith = 1;
  }
  else if (state->client_state.arrays.vertexArray.enabled &&
           state->client_state.arrays.normalArray.enabled &&
           state->client_state.arrays.colorArray.enabled &&
           state->client_state.arrays.texCoordArray[0].enabled &&
           state->client_state.arrays.texCoordArray[1].enabled &&
           state->client_state.arrays.texCoordArray[2].enabled &&
           state->client_state.arrays.vertexArray.ptr != NULL &&
           state->client_state.arrays.vertexArray.stride != 0 &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[1].stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[2].stride &&
           normalArrayOffset >= 0 &&
           normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
           colorArrayOffset >= 0 &&
           colorArrayOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord0PointerOffset >= 0 &&
           texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord1PointerOffset >= 0 &&
           texCoord1PointerOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord2PointerOffset >= 0 &&
           texCoord2PointerOffset < state->client_state.arrays.vertexArray.stride)
  {
    /* For unreal tournament 4 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
    int offset = first * state->client_state.arrays.vertexArray.stride;
    int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

    if (debug_array_ptr)
    {
      log_gl("glVertexNormalColorTexCoord012PointerInterlaced_fake_func sending %d bytes from %d\n",
              data_size,
              offset);
    }
    long args[] = { INT_TO_ARG(offset),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.size),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.type),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
                    INT_TO_ARG(normalArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.normalArray.type),
                    INT_TO_ARG(colorArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.colorArray.size),
                    INT_TO_ARG(state->client_state.arrays.colorArray.type),
                    INT_TO_ARG(texCoord0PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
                    INT_TO_ARG(texCoord1PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[1].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[1].type),
                    INT_TO_ARG(texCoord2PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[2].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[2].type),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
    do_opengl_call(glVertexNormalColorTexCoord012PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

    startIndiceTextureToDealtWith = 3;
  }
  else if (state->client_state.arrays.vertexArray.enabled &&
           state->client_state.arrays.normalArray.enabled &&
           state->client_state.arrays.colorArray.enabled &&
           state->client_state.arrays.texCoordArray[0].enabled &&
           state->client_state.arrays.texCoordArray[1].enabled &&
           state->client_state.arrays.vertexArray.ptr != NULL &&
           state->client_state.arrays.vertexArray.stride != 0 &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[1].stride &&
           normalArrayOffset >= 0 &&
           normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
           colorArrayOffset >= 0 &&
           colorArrayOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord0PointerOffset >= 0 &&
           texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord1PointerOffset >= 0 &&
           texCoord1PointerOffset < state->client_state.arrays.vertexArray.stride)
  {
    /* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
    int offset = first * state->client_state.arrays.vertexArray.stride;
    int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

    if (debug_array_ptr)
    {
      log_gl("glVertexNormalColorTexCoord01PointerInterlaced_fake_func sending %d bytes from %d\n",
              data_size,
              offset);
    }
    long args[] = { INT_TO_ARG(offset),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.size),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.type),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
                    INT_TO_ARG(normalArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.normalArray.type),
                    INT_TO_ARG(colorArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.colorArray.size),
                    INT_TO_ARG(state->client_state.arrays.colorArray.type),
                    INT_TO_ARG(texCoord0PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
                    INT_TO_ARG(texCoord1PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[1].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[1].type),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
    do_opengl_call(glVertexNormalColorTexCoord01PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

    startIndiceTextureToDealtWith = 2;
  }
  else if (state->client_state.arrays.vertexArray.enabled &&
           state->client_state.arrays.normalArray.enabled &&
           state->client_state.arrays.colorArray.enabled == 0 &&
           state->client_state.arrays.texCoordArray[0].enabled &&
           state->client_state.arrays.texCoordArray[1].enabled &&
           state->client_state.arrays.texCoordArray[2].enabled &&
           state->client_state.arrays.vertexArray.ptr != NULL &&
           state->client_state.arrays.vertexArray.stride != 0 &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[1].stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[2].stride &&
           normalArrayOffset >= 0 &&
           normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord0PointerOffset >= 0 &&
           texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord1PointerOffset >= 0 &&
           texCoord1PointerOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord2PointerOffset >= 0 &&
           texCoord2PointerOffset < state->client_state.arrays.vertexArray.stride)
  {
    /* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
    int offset = first * state->client_state.arrays.vertexArray.stride;
    int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

    if (debug_array_ptr)
    {
      log_gl("glVertexNormalTexCoord012PointerInterlaced_fake_func sending %d bytes from %d\n",
              data_size,
              offset);
    }
    long args[] = { INT_TO_ARG(offset),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.size),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.type),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
                    INT_TO_ARG(normalArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.normalArray.type),
                    INT_TO_ARG(texCoord0PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
                    INT_TO_ARG(texCoord1PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[1].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[1].type),
                    INT_TO_ARG(texCoord2PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[2].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[2].type),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
    do_opengl_call(glVertexNormalTexCoord012PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

    startIndiceTextureToDealtWith = 3;
  }
  else if (state->client_state.arrays.vertexArray.enabled &&
           state->client_state.arrays.normalArray.enabled &&
           state->client_state.arrays.colorArray.enabled == 0 &&
           state->client_state.arrays.texCoordArray[0].enabled &&
           state->client_state.arrays.texCoordArray[1].enabled &&
           state->client_state.arrays.vertexArray.ptr != NULL &&
           state->client_state.arrays.vertexArray.stride != 0 &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[1].stride &&
           normalArrayOffset >= 0 &&
           normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord0PointerOffset >= 0 &&
           texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord1PointerOffset >= 0 &&
           texCoord1PointerOffset < state->client_state.arrays.vertexArray.stride)
  {
    /* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
    int offset = first * state->client_state.arrays.vertexArray.stride;
    int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

    if (debug_array_ptr)
    {
      log_gl("glVertexNormalTexCoord01PointerInterlaced_fake_func sending %d bytes from %d\n",
              data_size,
              offset);
    }
    long args[] = { INT_TO_ARG(offset),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.size),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.type),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
                    INT_TO_ARG(normalArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.normalArray.type),
                    INT_TO_ARG(texCoord0PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
                    INT_TO_ARG(texCoord1PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[1].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[1].type),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
    do_opengl_call(glVertexNormalTexCoord01PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

    startIndiceTextureToDealtWith = 2;
  }
  else if (state->client_state.arrays.vertexArray.enabled &&
           state->client_state.arrays.normalArray.enabled &&
           state->client_state.arrays.colorArray.enabled == 0 &&
           state->client_state.arrays.texCoordArray[0].enabled &&
           state->client_state.arrays.vertexArray.ptr != NULL &&
           state->client_state.arrays.vertexArray.stride != 0 &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
           normalArrayOffset >= 0 &&
           normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord0PointerOffset >= 0 &&
           texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride)
  {
    /* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
    int offset = first * state->client_state.arrays.vertexArray.stride;
    int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

    if (debug_array_ptr)
    {
      log_gl("glVertexNormalTexCoord0PointerInterlaced_fake_func sending %d bytes from %d\n",
              data_size,
              offset);
    }
    long args[] = { INT_TO_ARG(offset),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.size),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.type),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
                    INT_TO_ARG(normalArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.normalArray.type),
                    INT_TO_ARG(texCoord0PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
    do_opengl_call(glVertexNormalTexCoord0PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

    startIndiceTextureToDealtWith = 1;
  }
  else if (state->client_state.arrays.vertexArray.enabled &&
           state->client_state.arrays.normalArray.enabled &&
           state->client_state.arrays.colorArray.enabled &&
           state->client_state.arrays.texCoordArray[0].enabled &&
           state->client_state.arrays.vertexArray.ptr != NULL &&
           state->client_state.arrays.vertexArray.stride != 0 &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.colorArray.stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.texCoordArray[0].stride &&
           normalArrayOffset >= 0 &&
           normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
           colorArrayOffset >= 0 &&
           colorArrayOffset < state->client_state.arrays.vertexArray.stride &&
           texCoord0PointerOffset >= 0 &&
           texCoord0PointerOffset < state->client_state.arrays.vertexArray.stride)
  {
    /* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
    int offset = first * state->client_state.arrays.vertexArray.stride;
    int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

    if (debug_array_ptr)
    {
      log_gl("glVertexNormalColorTexCoord0PointerInterlaced_fake_func sending %d bytes from %d\n",
              data_size,
              offset);
    }
    long args[] = { INT_TO_ARG(offset),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.size),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.type),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
                    INT_TO_ARG(normalArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.normalArray.type),
                    INT_TO_ARG(colorArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.colorArray.size),
                    INT_TO_ARG(state->client_state.arrays.colorArray.type),
                    INT_TO_ARG(texCoord0PointerOffset),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, data_size};
    do_opengl_call(glVertexNormalColorTexCoord0PointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

    startIndiceTextureToDealtWith = 1;
  }
  else if (state->client_state.arrays.vertexArray.enabled &&
           state->client_state.arrays.normalArray.enabled &&
           state->client_state.arrays.vertexArray.ptr != NULL &&
           getMulFactorFromPointerArray(&state->client_state.arrays.vertexArray) ==
           getMulFactorFromPointerArray(&state->client_state.arrays.normalArray) &&
           state->client_state.arrays.vertexArray.ptr == state->client_state.arrays.normalArray.ptr)
  {
    /* Special optimization for earth3d */
    int data_size = nbElts * getMulFactorFromPointerArray(&state->client_state.arrays.vertexArray);

    if (debug_array_ptr)
    {
      log_gl("glVertexAndNormalPointer_fake_func sending %d bytes from %d\n",
              data_size,
              0);
    }

    long args[] = { INT_TO_ARG(state->client_state.arrays.vertexArray.size),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.type),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
                    INT_TO_ARG(state->client_state.arrays.normalArray.type),
                    INT_TO_ARG(state->client_state.arrays.normalArray.stride),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, data_size };
    do_opengl_call(glVertexAndNormalPointer_fake_func, NULL, CHECK_ARGS(args, args_size));
  }

  else if (state->client_state.arrays.vertexArray.enabled &&
           state->client_state.arrays.normalArray.enabled &&
           /*state->client_state.arrays.colorArray.enabled && */
           state->client_state.arrays.vertexArray.ptr != NULL &&
           state->client_state.arrays.vertexArray.stride != 0 &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.colorArray.stride &&
           normalArrayOffset >= 0 &&
           normalArrayOffset < state->client_state.arrays.vertexArray.stride &&
           colorArrayOffset >= 0 &&
           colorArrayOffset < state->client_state.arrays.vertexArray.stride)
  {
    /* Special optimization for tuxracer */
    /*
      glEnableClientState(GL_VERTEX_ARRAY);
      glVertexPointer( 3, GL_FLOAT, STRIDE_GL_ARRAY, vnc_array );

      glEnableClientState(GL_NORMAL_ARRAY);
      glNormalPointer( GL_FLOAT, STRIDE_GL_ARRAY,
          vnc_array + 4*sizeof(GLfloat) );

      glEnableClientState(GL_COLOR_ARRAY);
      glColorPointer( 4, GL_UNSIGNED_BYTE, STRIDE_GL_ARRAY,
          vnc_array + 8*sizeof(GLfloat) );
    */
    int offset = first * state->client_state.arrays.vertexArray.stride;
    int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

    if (debug_array_ptr)
    {
      log_gl("glVertexNormalColorPointerInterlaced_fake_func sending %d bytes from %d\n",
              data_size,
              offset);
    }
    long args[] = { INT_TO_ARG(offset),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.size),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.type),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
                    INT_TO_ARG(normalArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.normalArray.type),
                    INT_TO_ARG(colorArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.colorArray.size),
                    INT_TO_ARG(state->client_state.arrays.colorArray.type),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        data_size};
    do_opengl_call(glVertexNormalColorPointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));
  }
  else if (state->client_state.arrays.vertexArray.enabled &&
           state->client_state.arrays.normalArray.enabled &&
           state->client_state.arrays.vertexArray.ptr != NULL &&
           state->client_state.arrays.vertexArray.stride != 0 &&
           state->client_state.arrays.vertexArray.stride == state->client_state.arrays.normalArray.stride &&
           normalArrayOffset >= 0 &&
           normalArrayOffset < state->client_state.arrays.vertexArray.stride)
  {
    /* For unreal tournament 3 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
    int offset = first * state->client_state.arrays.vertexArray.stride;
    int data_size = (last - first + 1) * state->client_state.arrays.vertexArray.stride;

    if (debug_array_ptr)
    {
      log_gl("glVertexNormalPointerInterlaced_fake_func sending %d bytes from %d\n",
              data_size,
              offset);
    }
    long args[] = { INT_TO_ARG(offset),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.size),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.type),
                    INT_TO_ARG(state->client_state.arrays.vertexArray.stride),
                    INT_TO_ARG(normalArrayOffset),
                    INT_TO_ARG(state->client_state.arrays.normalArray.type),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.vertexArray.ptr + offset) };
    int args_size[] = { 0, 0, 0, 0, 0, 0, 0, data_size};
    do_opengl_call(glVertexNormalPointerInterlaced_fake_func, NULL, CHECK_ARGS(args, args_size));

    _glArraySend(state, ARRAY_SEND_FUNC(glColorPointer), &state->client_state.arrays.colorArray, first, last);
  }
  else
  {
    _glArraySend(state, ARRAY_SEND_FUNC(glVertexPointer), &state->client_state.arrays.vertexArray, first, last);
    _glArraySend(state, ARRAY_SEND_FUNC(glNormalPointer), &state->client_state.arrays.normalArray, first, last);
    _glArraySend(state, ARRAY_SEND_FUNC(glColorPointer), &state->client_state.arrays.colorArray, first, last);
  }
  _glArraySend(state, ARRAY_SEND_FUNC(glSecondaryColorPointer), &state->client_state.arrays.secondaryColorArray, first, last);
  _glArraySend(state, ARRAY_SEND_FUNC(glEdgeFlagPointer), &state->client_state.arrays.edgeFlagArray, first, last);
  _glArraySend(state, ARRAY_SEND_FUNC(glIndexPointer), &state->client_state.arrays.indexArray, first, last);

  for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_ARB;i++)
  {
    _glArraySend(state, ARRAY_SEND_FUNC(glVertexAttribPointerARB), &state->client_state.arrays.vertexAttribArray[i], first, last);
  }
  for(i=0;i<MY_GL_MAX_VARIANT_POINTER_EXT;i++)
  {
    _glArraySend(state, ARRAY_SEND_FUNC(glVariantPointerEXT), &state->client_state.arrays.variantPointer[i], first, last);
  }

  if (startIndiceTextureToDealtWith == 0 &&
      state->client_state.arrays.texCoordArray[0].vbo_name == 0 &&
      state->client_state.arrays.texCoordArray[1].vbo_name == 0 &&
      state->client_state.arrays.texCoordArray[2].vbo_name == 0 &&
      state->client_state.arrays.texCoordArray[0].enabled &&
      state->client_state.arrays.texCoordArray[1].enabled &&
      state->client_state.arrays.texCoordArray[2].enabled &&
      state->client_state.arrays.texCoordArray[0].ptr &&
      state->client_state.arrays.texCoordArray[0].ptr == state->client_state.arrays.texCoordArray[1].ptr &&
      state->client_state.arrays.texCoordArray[0].size == state->client_state.arrays.texCoordArray[1].size &&
      state->client_state.arrays.texCoordArray[0].type == state->client_state.arrays.texCoordArray[1].type &&
      state->client_state.arrays.texCoordArray[0].stride == state->client_state.arrays.texCoordArray[1].stride &&
      state->client_state.arrays.texCoordArray[0].ptr == state->client_state.arrays.texCoordArray[2].ptr &&
      state->client_state.arrays.texCoordArray[0].size == state->client_state.arrays.texCoordArray[2].size &&
      state->client_state.arrays.texCoordArray[0].type == state->client_state.arrays.texCoordArray[2].type &&
      state->client_state.arrays.texCoordArray[0].stride == state->client_state.arrays.texCoordArray[2].stride)
  {
    /* For unreal tournament 4 with GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" */
    int data_size = nbElts * getMulFactorFromPointerArray(&state->client_state.arrays.texCoordArray[0]);
    if (debug_array_ptr)
    {
      log_gl("glTexCoordPointer012_fake_func sending %d bytes from %d\n",
              data_size,
              0);
    }

    long args[] = { INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].stride),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.texCoordArray[0].ptr) };
    int args_size[] = { 0, 0, 0, 0, data_size };
    do_opengl_call(glTexCoordPointer012_fake_func, NULL, CHECK_ARGS(args, args_size));

    startIndiceTextureToDealtWith = 3;
  }
  else
  if (startIndiceTextureToDealtWith == 0 &&
      state->client_state.arrays.texCoordArray[0].vbo_name == 0 &&
      state->client_state.arrays.texCoordArray[1].vbo_name == 0 &&
      state->client_state.arrays.texCoordArray[0].enabled &&
      state->client_state.arrays.texCoordArray[1].enabled &&
      state->client_state.arrays.texCoordArray[0].ptr &&
      state->client_state.arrays.texCoordArray[0].ptr == state->client_state.arrays.texCoordArray[1].ptr &&
      state->client_state.arrays.texCoordArray[0].size == state->client_state.arrays.texCoordArray[1].size &&
      state->client_state.arrays.texCoordArray[0].type == state->client_state.arrays.texCoordArray[1].type &&
      state->client_state.arrays.texCoordArray[0].stride == state->client_state.arrays.texCoordArray[1].stride)
  {
    /* Special optimization for earth3d */
    int data_size = nbElts * getMulFactorFromPointerArray(&state->client_state.arrays.texCoordArray[0]);
    if (debug_array_ptr)
    {
      log_gl("glTexCoordPointer01_fake_func sending %d bytes from %d\n",
              data_size,
              0);
    }

    long args[] = { INT_TO_ARG(state->client_state.arrays.texCoordArray[0].size),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].type),
                    INT_TO_ARG(state->client_state.arrays.texCoordArray[0].stride),
                    INT_TO_ARG(data_size),
                    POINTER_TO_ARG(state->client_state.arrays.texCoordArray[0].ptr) };
    int args_size[] = { 0, 0, 0, 0, data_size };
    do_opengl_call(glTexCoordPointer01_fake_func, NULL, CHECK_ARGS(args, args_size));

    startIndiceTextureToDealtWith = 2;
  }

  for(i=startIndiceTextureToDealtWith;i<NB_MAX_TEXTURES;i++)
  {
    _glArraySend(state, ARRAY_SEND_FUNC(glTexCoordPointer), &state->client_state.arrays.texCoordArray[i], first, last);
  }

  _glArraySend(state, ARRAY_SEND_FUNC(glWeightPointerARB), &state->client_state.arrays.weightArray, first, last);
  _glArraySend(state, ARRAY_SEND_FUNC(glMatrixIndexPointerARB), &state->client_state.arrays.matrixIndexArray, first, last);
  _glArraySend(state, ARRAY_SEND_FUNC(glFogCoordPointer), &state->client_state.arrays.fogCoordArray, first, last);
}

#define ASSERT_COND(cond, val)  do { if (!(val)) { state->lastGlError = val; return; } } while(0)
#define CLEAR_ERROR_STATE()  do { state->lastGlError = 0; } while(0)


GLAPI void APIENTRY EXT_FUNC(glLockArraysEXT) (GLint first, GLsizei count)
{
  /* Quite difficult to be correctly implemented IMHO for a doubtful performance gain */
  /* You need to send data for enabled arrays at this moment, but then for */
  /* the glDrawArrays, glDrawElements, etc... you need to check if enough data has been sent */
  /* For ex consider the following code :
      glVertexPointer(.....);
      glColorPointer(......);
      glNormalPointer(.....);
      glEnableClientState(GL_VERTEX_ARRAY);
      glLockArraysEXT(0, 16);                  (1)
      glEnableClientState(GL_COLOR_ARRAY);
      glEnableClientState(GL_NORMAL_ARRAY);
      glDrawArrays(GL_TRIANGLE, 0, 20);        (2)
      glUnlockArraysEXT();
  */
  /* At (1), you need to send 16 vertices, but at (2) you need to send the */
  /* 4 following ones, or either the whole 20... */
  CHECK_PROC(glLockArraysEXT);
  GET_CURRENT_STATE();
  ASSERT_COND(first >= 0, GL_INVALID_VALUE);
  ASSERT_COND(count > 0, GL_INVALID_VALUE);
  ASSERT_COND(state->isBetweenLockArrays == 0, GL_INVALID_OPERATION);
  ASSERT_COND(state->isBetweenBegin == 0, GL_INVALID_OPERATION);

  if (debug_array_ptr)
  {
    log_gl("glLockArraysEXT(%d,%d)\n", first, count);
  }

  state->isBetweenLockArrays = 1;

  state->locked_first = first;
  state->locked_count = count;

  CLEAR_ERROR_STATE();
}

GLAPI void APIENTRY EXT_FUNC(glUnlockArraysEXT) ()
{
  CHECK_PROC(glUnlockArraysEXT);
  GET_CURRENT_STATE();
  ASSERT_COND(state->isBetweenLockArrays == 1, GL_INVALID_OPERATION);
  ASSERT_COND(state->isBetweenBegin == 0, GL_INVALID_OPERATION);

  if (debug_array_ptr)
  {
    log_gl("glUnlockArraysEXT()\n");
  }

  state->isBetweenLockArrays = 0;

  CLEAR_ERROR_STATE();
}

GLAPI void APIENTRY glArrayElement( GLint i )
{
  Buffer* buffer = _get_buffer(GL_ARRAY_BUFFER_ARB);
  if (buffer)
  {
    long args[] = { INT_TO_ARG(i) };
    do_opengl_call(glArrayElement_func, NULL, args, NULL);
  }
  else
  {
    _glElementArrayImmediate(i);
  }
}

GLAPI void APIENTRY glDrawArrays( GLenum mode,
                   GLint first,
                   GLsizei count )
{
  if (debug_array_ptr) log_gl("glDrawArrays(%d,%d,%d)\n", mode, first, count);

  _glArraysSend(first, first + count - 1);

  long args[] = { INT_TO_ARG(mode), INT_TO_ARG(first), INT_TO_ARG(count) };
  do_opengl_call(glDrawArrays_func, NULL, args, NULL);
}

static int _isTuxRacer(GLState* state)
{
  int i;
  ClientArrays* arrays = &state->client_state.arrays;
  if (arrays->vertexArray.vbo_name ||
      arrays->normalArray.vbo_name ||
      arrays->colorArray.vbo_name ||
      arrays->vertexArray.enabled == 0 ||
      arrays->normalArray.enabled == 0)
    return 0;

  int normalArrayOffset = (long)arrays->normalArray.ptr - (long)arrays->vertexArray.ptr;
  int colorArrayOffset = (long)arrays->colorArray.ptr - (long)arrays->vertexArray.ptr;

  if (!(arrays->vertexArray.ptr != NULL &&
        arrays->vertexArray.stride != 0 &&
        arrays->vertexArray.stride == arrays->normalArray.stride &&
        arrays->vertexArray.stride == arrays->colorArray.stride &&
        normalArrayOffset >= 0 &&
        normalArrayOffset < arrays->vertexArray.stride &&
        colorArrayOffset >= 0 &&
        colorArrayOffset < arrays->vertexArray.stride))
    return 0;

  if (!(arrays->vertexArray.type == GL_FLOAT && arrays->vertexArray.size == 3 &&
        arrays->normalArray.type == GL_FLOAT &&
        arrays->colorArray.type == GL_UNSIGNED_BYTE && arrays->colorArray.size == 4))
    return 0;

  if (arrays->secondaryColorArray.enabled ||
      arrays->indexArray.enabled ||
      arrays->edgeFlagArray.enabled ||
      arrays->weightArray.enabled ||
      arrays->matrixIndexArray.enabled ||
      arrays->fogCoordArray.enabled)
    return 0;

  for(i=0;i<NB_MAX_TEXTURES;i++)
  {
    if (arrays->texCoordArray[i].enabled)
      return 0;
  }
  for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_ARB;i++)
  {
    if (arrays->vertexAttribArray[i].enabled)
      return 0;
  }
  return 1;
}

static int _check_if_enabled_non_vbo_array()
{
  int i;
  GET_CURRENT_STATE();
  ClientArrays* arrays = &state->client_state.arrays;

  if (arrays->vertexArray.vbo_name == 0 && arrays->vertexArray.enabled)
    return 1;
  if (arrays->normalArray.vbo_name == 0 && arrays->normalArray.enabled)
    return 1;
  if (arrays->colorArray.vbo_name == 0 && arrays->colorArray.enabled)
    return 1;
  if (arrays->secondaryColorArray.vbo_name == 0 && arrays->secondaryColorArray.enabled)
    return 1;
  if (arrays->indexArray.vbo_name == 0 && arrays->indexArray.enabled)
    return 1;
  if (arrays->edgeFlagArray.vbo_name == 0 && arrays->edgeFlagArray.enabled)
    return 1;
  if (arrays->weightArray.vbo_name == 0 && arrays->weightArray.enabled)
    return 1;
  if (arrays->matrixIndexArray.vbo_name == 0 && arrays->matrixIndexArray.enabled)
    return 1;
  if (arrays->fogCoordArray.vbo_name == 0 && arrays->fogCoordArray.enabled)
    return 1;
  for(i=0;i<NB_MAX_TEXTURES;i++)
  {
    if (arrays->texCoordArray[i].vbo_name == 0 && arrays->texCoordArray[i].enabled)
      return 1;
  }
  for(i=0;i<MY_GL_MAX_VERTEX_ATTRIBS_ARB;i++)
  {
    if (arrays->vertexAttribArray[i].vbo_name == 0 && arrays->vertexAttribArray[i].enabled)
      return 1;
  }
  for(i=0;i<MY_GL_MAX_VARIANT_POINTER_EXT;i++)
  {
    if (arrays->variantPointer[i].vbo_name == 0 && arrays->variantPointer[i].enabled)
      return 1;
  }

  return 0;
}

GLAPI void APIENTRY glDrawElements( GLenum mode,
                     GLsizei count,
                     GLenum type,
                     const GLvoid *indices )
{
  CHECK_PROC(glDrawElements);
  int i;
  int size = count;
  //Buffer* bufferArray;
  Buffer* bufferElement;

  if (debug_array_ptr) log_gl("glDrawElements(%d,%d,%d,%p)\n", mode, count, type, indices);

  if (count == 0) return;

  //bufferArray = _get_buffer(GL_ARRAY_BUFFER_ARB);
  bufferElement = _get_buffer(GL_ELEMENT_ARRAY_BUFFER_ARB);
  if (bufferElement)
  {
    if (_check_if_enabled_non_vbo_array())
    {
      log_gl("sorry : unsupported : glDrawElements in EBO with a non VBO array enabled\n");
      return;
    }
    long args[] = { INT_TO_ARG(mode), INT_TO_ARG(count), INT_TO_ARG(type), POINTER_TO_ARG(indices) };
    do_opengl_call(_glDrawElements_buffer_func, NULL, args, NULL);
    return;
  }


  int minIndice = 0;
  int maxIndice = -1;

  switch(type)
  {
    case GL_UNSIGNED_BYTE:
    {
      //if (!bufferArray)
      {
        unsigned char* tab_indices = (unsigned char*)indices;
        minIndice = tab_indices[0];
        maxIndice = tab_indices[0];
        for(i=1;i<count;i++)
        {
          if (tab_indices[i] < minIndice) minIndice = tab_indices[i];
          if (tab_indices[i] > maxIndice) maxIndice = tab_indices[i];
        }
      }
      break;
    }

    case GL_UNSIGNED_SHORT:
    {
      size *= 2;
      //if (!bufferArray)
      {
        unsigned short* tab_indices = (unsigned short*)indices;
        minIndice = tab_indices[0];
        maxIndice = tab_indices[0];
        for(i=1;i<count;i++)
        {
          if (tab_indices[i] < minIndice) minIndice = tab_indices[i];
          if (tab_indices[i] > maxIndice) maxIndice = tab_indices[i];
        }
      }
      break;
    }

    case GL_UNSIGNED_INT:
    {
      size *= 4;
      //if (!bufferArray)
      {
        unsigned int* tab_indices = (unsigned int*)indices;
        minIndice = tab_indices[0];
        maxIndice = tab_indices[0];
        for(i=1;i<count;i++)
        {
          if (tab_indices[i] < minIndice) minIndice = tab_indices[i];
          if (tab_indices[i] > maxIndice) maxIndice = tab_indices[i];
        }
        //log_gl("maxIndice = %d\n", maxIndice);
        if (maxIndice > 100 * 1024 * 1024)
        {
          log_gl("too big indice : %d\n",maxIndice);
          return;
        }
      }
      break;
    }

    default:
      log_gl("unsupported type = %d\n", type);
      return;
  }

  GET_CURRENT_STATE();
  if (_isTuxRacer(state) && type == GL_UNSIGNED_INT)
  {
    /* Special optimization for tuxracer */
    /*
      glEnableClientState(GL_VERTEX_ARRAY);
      glVertexPointer( 3, GL_FLOAT, STRIDE_GL_ARRAY, vnc_array );

      glEnableClientState(GL_NORMAL_ARRAY);
      glNormalPointer( GL_FLOAT, STRIDE_GL_ARRAY,
          vnc_array + 4*sizeof(GLfloat) );

      glEnableClientState(GL_COLOR_ARRAY);
      glColorPointer( 4, GL_UNSIGNED_BYTE, STRIDE_GL_ARRAY,
          vnc_array + 8*sizeof(GLfloat) );
    */
    ClientArrays* arrays = &state->client_state.arrays;
    int stride = arrays->vertexArray.stride;
    int bufferSize = (maxIndice - minIndice + 1) * stride + sizeof(int) * count;
    int eltSize = 6 * sizeof(float) + ((arrays->colorArray.enabled) ? 4 * sizeof(unsigned char) : 0);
    int singleCommandSize = count * eltSize;
    if (count < bufferSize)
    {
      unsigned int* tab_indices = (unsigned int*)indices;
      const char* vertexArray = (const char*)arrays->vertexArray.ptr;
      int normalArrayOffset = (long)arrays->normalArray.ptr - (long)arrays->vertexArray.ptr;
      int colorArrayOffset = (long)arrays->colorArray.ptr - (long)arrays->vertexArray.ptr;
      state->tuxRacerBuffer = realloc(state->tuxRacerBuffer, singleCommandSize);

      for(i=0;i<count;i++)
      {
        int ind = tab_indices[i];
        memcpy(&state->tuxRacerBuffer[i * eltSize], &vertexArray[ind * stride], 3 * sizeof(float));
        memcpy(&state->tuxRacerBuffer[i * eltSize + 3 * sizeof(float)],
                &vertexArray[ind * stride + normalArrayOffset], 3 * sizeof(float));
        if (arrays->colorArray.enabled)
          memcpy(&state->tuxRacerBuffer[i * eltSize + 6 * sizeof(float)],
                &vertexArray[ind * stride + colorArrayOffset], 4 * sizeof(unsigned char));
      }

      long args[] = { INT_TO_ARG(mode), INT_TO_ARG(count), INT_TO_ARG(arrays->colorArray.enabled), POINTER_TO_ARG(state->tuxRacerBuffer) };
      int args_size[] = { 0, 0, 0, singleCommandSize} ;
      do_opengl_call(glTuxRacerDrawElements_fake_func, NULL, CHECK_ARGS(args, args_size));
      return;
    }
  }
  _glArraysSend(minIndice, maxIndice);

  long args[] = { INT_TO_ARG(mode), INT_TO_ARG(count), INT_TO_ARG(type), POINTER_TO_ARG(indices) };
  int args_size[] = { 0, 0, 0, (indices) ? size : 0 } ;
  do_opengl_call(glDrawElements_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glDrawRangeElements( GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices )
{
  CHECK_PROC(glDrawRangeElements);
  int size = count;
  Buffer* bufferElement;

  if (debug_array_ptr) log_gl("glDrawRangeElements(0x%X, %d, %d, %d, %d, %p)\n", mode, start, end, count, type, indices);

  if (count == 0) return;

  /* Yes : try to send regular arrays even if GL_ELEMENT_ARRAY_BUFFER_ARB is enabled.
     This is necessary for nexuiz 2.3 where in GL_ELEMENT_ARRAY_BUFFER_ARB some arrays are regular array pointers,
     and some others are stored in VBO... */
  _glArraysSend(start, end);

  bufferElement = _get_buffer(GL_ELEMENT_ARRAY_BUFFER_ARB);
  if (bufferElement)
  {
    long args[] = { INT_TO_ARG(mode), INT_TO_ARG(start), INT_TO_ARG(end), INT_TO_ARG(count), INT_TO_ARG(type), POINTER_TO_ARG(indices) };
    do_opengl_call(_glDrawRangeElements_buffer_func, NULL, args, NULL);
    return;
  }
#if 0
  int i;
  switch(type)
  {
    case GL_UNSIGNED_BYTE:
    {
      unsigned char* tab_indices = (unsigned char*)indices;
      for(i=0;i<count;i++)
      {
        if (tab_indices[i] < start || tab_indices[i] > end)
        {
          log_gl("indice out of bounds at offset %d\n", i);
        }
      }
      break;
    }

    case GL_UNSIGNED_SHORT:
    {
      size *= 2;
      unsigned short* tab_indices = (unsigned short*)indices;
      for(i=0;i<count;i++)
      {
        if (tab_indices[i] < start || tab_indices[i] > end)
        {
          log_gl("indice out of bounds at offset %d\n", i);
        }
      }
      break;
    }

    case GL_UNSIGNED_INT:
    {
      size *= 4;
      unsigned int* tab_indices = (unsigned int*)indices;
      for(i=0;i<count;i++)
      {
        if (tab_indices[i] < start || tab_indices[i] > end)
        {
          log_gl("indice out of bounds at offset %d\n", i);
        }
      }
      break;
    }

    default:
      log_gl("unsupported type = %d\n", type);
      return;
  }
#else
  switch(type)
  {
    case GL_UNSIGNED_BYTE:
    {
    }

    case GL_UNSIGNED_SHORT:
    {
      size *= 2;
      break;
    }

    case GL_UNSIGNED_INT:
    {
      size *= 4;
      break;
    }

    default:
      log_gl("unsupported type = %d\n", type);
      return;
  }
#endif
  long args[] = { INT_TO_ARG(mode), INT_TO_ARG(start), INT_TO_ARG(end),
                  INT_TO_ARG(count), INT_TO_ARG(type), POINTER_TO_ARG(indices) };
  int args_size[] = { 0, 0, 0, 0, 0, size } ;
  do_opengl_call(glDrawRangeElements_func, NULL, CHECK_ARGS(args, args_size));

}

DEFINE_EXT(glDrawRangeElements, ( GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices ),
                             (mode, start, end, count, type, indices) )


GLAPI void APIENTRY glMultiDrawArrays( GLenum mode, GLint *first,
                                       GLsizei *count, GLsizei primcount )
{
  GLint i;

  for (i = 0; i < primcount; i++) {
    if (count[i] > 0) {
        glDrawArrays(mode, first[i], count[i]);
    }
  }
}

DEFINE_EXT(glMultiDrawArrays, ( GLenum mode, GLint *first,
                                          GLsizei *count, GLsizei primcount ),
                             (mode, first, count, primcount) )

GLAPI void APIENTRY glMultiDrawElements( GLenum mode, const GLsizei *count, GLenum type,
                                         const GLvoid **indices, GLsizei primcount )
{
  GLint i;
  CHECK_PROC(glMultiDrawElements);

  Buffer* bufferElement = _get_buffer(GL_ELEMENT_ARRAY_BUFFER_ARB);
  if (bufferElement && primcount > 1)
  {
    if (_check_if_enabled_non_vbo_array())
    {
      log_gl("sorry : unsupported : glMultiDrawElements in EBO with a non VBO array enabled\n");
      return;
    }
    long args[] = { INT_TO_ARG(mode), POINTER_TO_ARG(count), INT_TO_ARG(type),
                    POINTER_TO_ARG(indices), INT_TO_ARG(primcount) };
    int args_size[] = { 0, primcount * sizeof(int), 0, primcount * sizeof(void*), 0 };
    do_opengl_call(_glMultiDrawElements_buffer_func, NULL, args, args_size);
    return;
  }

  for (i = 0; i < primcount; i++) {
    if (count[i] > 0) {
        glDrawElements(mode, count[i], type, indices[i]);
    }
  }
}

DEFINE_EXT(glMultiDrawElements, ( GLenum mode, const GLsizei *count, GLenum type,
                                            const GLvoid **indices, GLsizei primcount ),
                                (mode, count, type, indices, primcount) )


#if 0
GLAPI void APIENTRY glArrayObjectATI (GLenum array, GLint size, GLenum type, GLsizei stride, GLuint buffer, GLuint offset)
{
  CHECK_PROC(glArrayObjectATI);
  //log_gl("glArrayObjectATI(array=%p, size=%d, type=%p, stride=%d, buffer=%d, offset=%d)\n", array, size, type, stride, buffer, offset);
  long args[] = { UNSIGNED_INT_TO_ARG(array), INT_TO_ARG(size), UNSIGNED_INT_TO_ARG(type), INT_TO_ARG(stride), UNSIGNED_INT_TO_ARG(buffer), UNSIGNED_INT_TO_ARG(offset)};
  do_opengl_call(glArrayObjectATI_func, NULL, args, NULL);
}
#endif

GLAPI void APIENTRY glDrawElementArrayATI (GLenum mode, GLsizei count)
{
  CHECK_PROC(glDrawElementArrayATI);
  GET_CURRENT_STATE();

  if (state->client_state.arrays.elementArrayATI.enabled &&
      state->client_state.arrays.elementArrayATI.ptr != NULL)
  {
    int size = count * getMulFactorFromPointerArray(&state->client_state.arrays.elementArrayATI);
    long args[] = { INT_TO_ARG(state->client_state.arrays.elementArrayATI.type),
                    INT_TO_ARG(size),
                    INT_TO_ARG(state->client_state.arrays.elementArrayATI.ptr) };
    int args_size[] = {0, 0, size};
    do_opengl_call(glElementPointerATI_fake_func, NULL, CHECK_ARGS(args, args_size));
  }

  long args[] = { INT_TO_ARG(mode), INT_TO_ARG(count) };
  do_opengl_call(glDrawElementArrayATI_func, NULL, args, NULL);
}

GLAPI void APIENTRY glDrawRangeElementArrayATI (GLenum mode, GLuint start, GLuint end, GLsizei count)
{
  CHECK_PROC(glDrawRangeElementArrayATI);
  GET_CURRENT_STATE();

  if (state->client_state.arrays.elementArrayATI.enabled &&
      state->client_state.arrays.elementArrayATI.ptr != NULL)
  {
    int size = count * getMulFactorFromPointerArray(&state->client_state.arrays.elementArrayATI);
    long args[] = { INT_TO_ARG(state->client_state.arrays.elementArrayATI.type),
                    INT_TO_ARG(size),
                    INT_TO_ARG(state->client_state.arrays.elementArrayATI.ptr) };
    int args_size[] = {0, 0, size};
    do_opengl_call(glElementPointerATI_fake_func, NULL, CHECK_ARGS(args, args_size));
  }

  long args[] = { INT_TO_ARG(mode), INT_TO_ARG(start), INT_TO_ARG(end), INT_TO_ARG(count) };
  do_opengl_call(glDrawRangeElementArrayATI_func, NULL, args, NULL);
}

GLAPI GLuint APIENTRY glGenSymbolsEXT(GLenum datatype, GLenum storagetype, GLenum range, GLuint components)
{
  GLuint id = 0;
  CHECK_PROC_WITH_RET(glGenSymbolsEXT);
  GET_CURRENT_STATE();
  long args[] = { datatype, storagetype, range, components };
  do_opengl_call(glGenSymbolsEXT_func, &id, args, NULL);
  if (id != 0)
  {
    state->symbols.tab = realloc(state->symbols.tab, (state->symbols.count+1) * sizeof(Symbol));
    state->symbols.tab[state->symbols.count].id = id;
    state->symbols.tab[state->symbols.count].datatype = datatype;
    state->symbols.tab[state->symbols.count].components = components;
    state->symbols.count++;
  }
  return id;
}

static int get_vertex_shader_var_nb_composants(GLuint id)
{
  GET_CURRENT_STATE();
  int i;
  for(i=0;i<state->symbols.count;i++)
  {
    if (id >= state->symbols.tab[i].id && id < state->symbols.tab[i].id + state->symbols.tab[i].components)
    {
      int size = 0;

      if (state->symbols.tab[i].datatype == GL_SCALAR_EXT)
        size = 1;
      else if (state->symbols.tab[i].datatype == GL_VECTOR_EXT)
        size = 4;
      else if (state->symbols.tab[i].datatype == GL_MATRIX_EXT)
        size = 16;
      else
      {
        log_gl("unknown datatype %d\n", state->symbols.tab[i].datatype);
        return 0;
      }

      return size;
    }
  }
  log_gl("unknown id %d\n", id);
  return 0;
}

GLAPI void APIENTRY glSetLocalConstantEXT (GLuint id, GLenum type, const GLvoid *addr)
{
  CHECK_PROC(glSetLocalConstantEXT);
  int size = get_vertex_shader_var_nb_composants(id) * getGlTypeByteSize(type);
  if (size)
  {
    long args[] = { id, type, POINTER_TO_ARG(addr) };
    int args_size[] = { 0, 0, size };

    do_opengl_call(glSetLocalConstantEXT_func, NULL, CHECK_ARGS(args, args_size));
  }
}

GLAPI void APIENTRY glSetInvariantEXT (GLuint id, GLenum type, const GLvoid *addr)
{
  CHECK_PROC(glSetInvariantEXT);
  int size = get_vertex_shader_var_nb_composants(id) * getGlTypeByteSize(type);
  if (size)
  {
    long args[] = { id, type, POINTER_TO_ARG(addr) };
    int args_size[] = { 0, 0, size };

    do_opengl_call(glSetInvariantEXT_func, NULL, CHECK_ARGS(args, args_size));
  }
}

#define glVariantGeneric(func_name, gltype) \
GLAPI void APIENTRY func_name (GLuint id, const gltype * addr)\
{\
  CHECK_PROC(func_name); \
  int size = get_vertex_shader_var_nb_composants(id) * sizeof(gltype); \
  if (size) \
  { \
    long args[] = { id, POINTER_TO_ARG(addr) }; \
    int args_size[] = { 0, size }; \
    do_opengl_call(CONCAT(func_name,_func), NULL, CHECK_ARGS(args, args_size)); \
  } \
}

glVariantGeneric(glVariantbvEXT, GLbyte);
glVariantGeneric(glVariantsvEXT, GLshort);
glVariantGeneric(glVariantivEXT, GLint);
glVariantGeneric(glVariantfvEXT, GLfloat);
glVariantGeneric(glVariantdvEXT, GLdouble);
glVariantGeneric(glVariantubvEXT, GLubyte);
glVariantGeneric(glVariantusvEXT, GLushort);
glVariantGeneric(glVariantuivEXT, GLuint);

GLAPI void APIENTRY glEnableVariantClientStateEXT (GLuint id)
{
  CHECK_PROC(glEnableVariantClientStateEXT);
  long args[] = { id };
  do_opengl_call(glEnableVariantClientStateEXT_func, NULL, args, NULL);
  if (id < MY_GL_MAX_VARIANT_POINTER_EXT)
  {
    GET_CURRENT_STATE();
    state->client_state.arrays.variantPointer[id].enabled = 1;
  }
}

GLAPI void APIENTRY glDisableVariantClientStateEXT (GLuint id)
{
  CHECK_PROC(glDisableVariantClientStateEXT);
  long args[] = { id };
  do_opengl_call(glDisableVariantClientStateEXT_func, NULL, args, NULL);
  if (id < MY_GL_MAX_VARIANT_POINTER_EXT)
  {
    GET_CURRENT_STATE();
    state->client_state.arrays.variantPointer[id].enabled = 0;
  }
}

GLAPI void APIENTRY glVariantPointerEXT (GLuint id, GLenum type, GLuint stride, const GLvoid *ptr)
{
  CHECK_PROC(glVariantPointerEXT);

  GET_CURRENT_STATE();
  if (id < MY_GL_MAX_VARIANT_POINTER_EXT)
  {
    state->client_state.arrays.variantPointer[id].vbo_name = state->arrayBuffer;
    if (state->client_state.arrays.variantPointer[id].vbo_name)
    {
      long args[] = { INT_TO_ARG(id), INT_TO_ARG(type), INT_TO_ARG(stride), POINTER_TO_ARG(ptr) };
      do_opengl_call(_glVariantPointerEXT_buffer_func, NULL, args, NULL);
      return;
    }

    if (debug_array_ptr)
      log_gl("glVariantPointerEXT[%d] type=%dstride=%d ptr=%p\n",
              id, type, stride, ptr);
    state->client_state.arrays.variantPointer[id].index = id;
    state->client_state.arrays.variantPointer[id].size = 4;
    state->client_state.arrays.variantPointer[id].type = type;
    state->client_state.arrays.variantPointer[id].stride = stride;
    state->client_state.arrays.variantPointer[id].ptr = ptr;
  }
  else
  {
    log_gl("id >= MY_GL_MAX_VARIANT_POINTER_EXT\n");
  }
}

#define glGetVariantGeneric(func_name, gltype) \
GLAPI void APIENTRY func_name (GLuint id, GLenum name, gltype* addr)\
{\
  CHECK_PROC(func_name); \
  int size = (name == GL_VARIANT_VALUE_EXT) ? get_vertex_shader_var_nb_composants(id) * sizeof(gltype) : sizeof(gltype); \
  if (size) \
  { \
    long args[] = { id, name, POINTER_TO_ARG(addr) }; \
    int args_size[] = { 0, 0, size }; \
    do_opengl_call(CONCAT(func_name,_func), NULL, CHECK_ARGS(args, args_size)); \
  } \
}

glGetVariantGeneric(glGetVariantBooleanvEXT, GLboolean);
glGetVariantGeneric(glGetVariantIntegervEXT, GLint);
glGetVariantGeneric(glGetVariantFloatvEXT, GLfloat);

GLAPI void APIENTRY glGetVariantPointervEXT (GLuint id, GLenum name, GLvoid* *addr)
{
  CHECK_PROC(glGetVariantPointervEXT);

  GET_CURRENT_STATE();
  if (id < MY_GL_MAX_VARIANT_POINTER_EXT)
  {
    if (name == GL_VARIANT_ARRAY_POINTER_EXT)
      *addr = (void*)state->client_state.arrays.variantPointer[id].ptr;
  }
  else
  {
    log_gl("id >= MY_GL_MAX_VARIANT_POINTER_EXT\n");
  }
}

#define glGetInvariantGeneric(func_name, gltype) \
GLAPI void APIENTRY func_name (GLuint id, GLenum name, gltype* addr)\
{\
  CHECK_PROC(func_name); \
  int size = (name == GL_INVARIANT_VALUE_EXT) ? get_vertex_shader_var_nb_composants(id) * sizeof(gltype) : sizeof(gltype); \
  if (size) \
  { \
    long args[] = { id, name, POINTER_TO_ARG(addr) }; \
    int args_size[] = { 0, 0, size }; \
    do_opengl_call(CONCAT(func_name,_func), NULL, CHECK_ARGS(args, args_size)); \
  } \
}

glGetInvariantGeneric(glGetInvariantBooleanvEXT, GLboolean);
glGetInvariantGeneric(glGetInvariantIntegervEXT, GLint);
glGetInvariantGeneric(glGetInvariantFloatvEXT, GLfloat);

#define glGetLocalConstantGeneric(func_name, gltype) \
GLAPI void APIENTRY func_name (GLuint id, GLenum name, gltype* addr)\
{\
  CHECK_PROC(func_name); \
  int size = (name == GL_LOCAL_CONSTANT_VALUE_EXT) ? get_vertex_shader_var_nb_composants(id) * sizeof(gltype) : sizeof(gltype); \
  if (size) \
  { \
    long args[] = { id, name, POINTER_TO_ARG(addr) }; \
    int args_size[] = { 0, 0, size }; \
    do_opengl_call(CONCAT(func_name,_func), NULL, CHECK_ARGS(args, args_size)); \
  } \
}

glGetLocalConstantGeneric(glGetLocalConstantBooleanvEXT, GLboolean);
glGetLocalConstantGeneric(glGetLocalConstantIntegervEXT, GLint);
glGetLocalConstantGeneric(glGetLocalConstantFloatvEXT, GLfloat);

static void _glShaderSource(int func_number, GLhandleARB handle, GLsizei size, const GLcharARB** tab_prog, const GLint* tab_length)
{
  int i;
  int* my_tab_length;
  int total_length = 0;
  int acc_length = 0;
  GLcharARB* all_progs;

  if (size <= 0 || tab_prog == NULL)
  {
    log_gl("size <= 0 || tab_prog == NULL\n");
    return;
  }
  my_tab_length = malloc(sizeof(int) * size);
  for(i=0;i<size;i++)
  {
    if (tab_prog[i] == NULL)
    {
      log_gl("tab_prog[%d] == NULL\n", i);
      free(my_tab_length);
      return ;
    }
    my_tab_length[i] = (tab_length && tab_length[i]) ? tab_length[i] : strlen(tab_prog[i]);
    total_length += my_tab_length[i];
  }
  all_progs = malloc(total_length+1);
  for(i=0;i<size;i++)
  {
    char* str_tmp = all_progs + acc_length;
    memcpy(str_tmp, tab_prog[i], my_tab_length[i]);
    str_tmp[my_tab_length[i]] = 0;
    if (debug_gl) log_gl("glShaderSource[%d] : %s\n", i, str_tmp);
    char* version_ptr = strstr(str_tmp, "#version");
    if (version_ptr && version_ptr != str_tmp)
    {
      /* ATI driver won't be happy if "#version" is not at beginning of program */
      /* Necessary for "Danger from the Deep 0.3.0" */
      int offset = version_ptr - str_tmp;
      char* eol = strchr(version_ptr, '\n');
      if (eol)
      {
        int len = eol - version_ptr + 1;
        memcpy(str_tmp, tab_prog[i] + offset, len);
        memcpy(str_tmp + len, tab_prog[i], offset);
      }
    }
    acc_length += my_tab_length[i];
  }
  long args[] = { INT_TO_ARG(handle), INT_TO_ARG(size), POINTER_TO_ARG(all_progs), POINTER_TO_ARG(my_tab_length) } ;
  int args_size[] = { 0, 0, total_length, sizeof(int) * size };
  do_opengl_call(func_number, NULL, CHECK_ARGS(args, args_size));
  free(my_tab_length);
  free(all_progs);
}


GLAPI void APIENTRY EXT_FUNC(glShaderSourceARB) (GLhandleARB handle, GLsizei size, const GLcharARB** tab_prog, const GLint* tab_length)
{
  CHECK_PROC(glShaderSourceARB);
  _glShaderSource(glShaderSourceARB_fake_func, handle, size, tab_prog, tab_length);
}

GLAPI void APIENTRY EXT_FUNC(glShaderSource) (GLhandleARB handle, GLsizei size, const GLcharARB** tab_prog, const GLint* tab_length)
{
  CHECK_PROC(glShaderSource);
  _glShaderSource(glShaderSource_fake_func, handle, size, tab_prog, tab_length);
}

GLAPI void APIENTRY EXT_FUNC(glGetProgramInfoLog)(GLuint program,
                                                  GLsizei maxLength,
                                                  GLsizei *length,
                                                  GLchar *infoLog)
{
  CHECK_PROC(glGetProgramInfoLog);
  int fake_length;
  if (length == NULL) length = &fake_length;
  long args[] = { INT_TO_ARG(program), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(infoLog) };
  int args_size[] = { 0, 0, sizeof(int), maxLength };
  do_opengl_call(glGetProgramInfoLog_func, NULL, CHECK_ARGS(args, args_size));
  log_gl("glGetProgramInfoLog: %s\n", infoLog);
}


GLAPI void APIENTRY EXT_FUNC(glGetProgramStringARB) (GLenum target, GLenum pname, GLvoid *string)
{
  int size = 0;
  CHECK_PROC(glGetProgramStringARB);
  glGetProgramivARB(target, GL_PROGRAM_LENGTH_ARB, &size);
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(pname), POINTER_TO_ARG(string) };
  int args_size[] = { 0, 0, size };
  do_opengl_call(glGetProgramStringARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetProgramStringNV) (GLenum target, GLenum pname, GLvoid *string)
{
  int size = 0;
  CHECK_PROC(glGetProgramStringNV);
  glGetProgramivNV(target, GL_PROGRAM_LENGTH_NV, &size);
  long args[] = { INT_TO_ARG(target), INT_TO_ARG(pname), POINTER_TO_ARG(string) };
  int args_size[] = { 0, 0, size };
  do_opengl_call(glGetProgramStringNV_func, NULL, CHECK_ARGS(args, args_size));
}



GLAPI void APIENTRY EXT_FUNC(glGetInfoLogARB)(GLhandleARB object,
                               GLsizei maxLength,
                               GLsizei *length,
                               GLcharARB *infoLog)
{
  CHECK_PROC(glGetInfoLogARB);
  int fake_length;
  if (length == NULL) length = &fake_length;
  long args[] = { INT_TO_ARG(object), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(infoLog) };
  /*int size = 0;
  glGetObjectParameterARBiv(object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &size);*/
  int args_size[] = { 0, 0, sizeof(int), maxLength };
  do_opengl_call(glGetInfoLogARB_func, NULL, CHECK_ARGS(args, args_size));
  log_gl("glGetInfoLogARB : %s\n", infoLog);
}

GLAPI void APIENTRY EXT_FUNC(glGetAttachedObjectsARB)(GLhandleARB program,
                                      GLsizei maxCount,
                                      GLsizei *count,
                                      GLhandleARB *objects)
{
  CHECK_PROC(glGetAttachedObjectsARB);
  int fake_count;
  if (count == NULL) count = &fake_count;
  long args[] = { INT_TO_ARG(program), INT_TO_ARG(maxCount), POINTER_TO_ARG(count), POINTER_TO_ARG(objects) };
  /*int size = 0;
  glGetObjectParameterARBiv(object, GL_OBJECT_ATTACHED_OBJECTS_ARB, &size);*/
  int args_size[] = { 0, 0, sizeof(int), maxCount * sizeof(int) };
  do_opengl_call(glGetAttachedObjectsARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetAttachedShaders)(GLuint program,
                                                    GLsizei maxCount,
                                                    GLsizei *count,
                                                    GLuint *shaders)
{
  CHECK_PROC(glGetAttachedShaders);
  int fake_count;
  if (count == NULL) count = &fake_count;
  long args[] = { INT_TO_ARG(program), INT_TO_ARG(maxCount), POINTER_TO_ARG(count), POINTER_TO_ARG(shaders) };
  int args_size[] = { 0, 0, sizeof(int), maxCount * sizeof(int) };
  do_opengl_call(glGetAttachedShaders_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI GLint EXT_FUNC(glGetUniformLocationARB) (GLuint program, const GLcharARB *txt)
{
  int i;
  int ret = -1;
  GET_CURRENT_STATE();
  for(i=0;i<state->countUniformLocations;i++)
  {
    if (state->uniformLocations[i].program == program &&
        strcmp(state->uniformLocations[i].txt, txt) == 0)
    {
      return state->uniformLocations[i].location;
    }
  }

  CHECK_PROC_WITH_RET(glGetUniformLocationARB);
  long args[] = { INT_TO_ARG(program), POINTER_TO_ARG(txt) } ;
  do_opengl_call(glGetUniformLocationARB_func, &ret, args, NULL);

  state->uniformLocations = realloc(state->uniformLocations, sizeof(UniformLocation) * (state->countUniformLocations+1));
  state->uniformLocations[state->countUniformLocations].program = program;
  state->uniformLocations[state->countUniformLocations].txt = strdup(txt);
  state->uniformLocations[state->countUniformLocations].location = ret;
  state->countUniformLocations++;

  return ret;
}

GLAPI GLint EXT_FUNC(glGetUniformLocation) (GLuint program, const GLcharARB *txt)
{
  int i;
  int ret = -1;
  GET_CURRENT_STATE();
  for(i=0;i<state->countUniformLocations;i++)
  {
    if (state->uniformLocations[i].program == program &&
        strcmp(state->uniformLocations[i].txt, txt) == 0)
    {
      return state->uniformLocations[i].location;
    }
  }

  CHECK_PROC_WITH_RET(glGetUniformLocation);
  long args[] = { INT_TO_ARG(program), POINTER_TO_ARG(txt) } ;
  do_opengl_call(glGetUniformLocation_func, &ret, args, NULL);

  state->uniformLocations = realloc(state->uniformLocations, sizeof(UniformLocation) * (state->countUniformLocations+1));
  state->uniformLocations[state->countUniformLocations].program = program;
  state->uniformLocations[state->countUniformLocations].txt = strdup(txt);
  state->uniformLocations[state->countUniformLocations].location = ret;
  state->countUniformLocations++;

  return ret;
}


GLAPI void APIENTRY EXT_FUNC(glGetActiveUniformARB)(GLuint program,
                                    GLuint index,
                                    GLsizei maxLength,
                                    GLsizei *length,
                                    GLint *size,
                                    GLenum *type,
                                    GLcharARB *name)
{
  CHECK_PROC(glGetActiveUniformARB);
  int fake_length;
  if (length == NULL) length = &fake_length;
  long args[] = { INT_TO_ARG(program), INT_TO_ARG(index), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(size), POINTER_TO_ARG(type), POINTER_TO_ARG(name) };
  int args_size[] = { 0, 0, 0, sizeof(int), sizeof(int), sizeof(int), maxLength };
  do_opengl_call(glGetActiveUniformARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetActiveUniform)(GLuint program,
                                    GLuint index,
                                    GLsizei maxLength,
                                    GLsizei *length,
                                    GLint *size,
                                    GLenum *type,
                                    GLcharARB *name)
{
  CHECK_PROC(glGetActiveUniform);
  int fake_length;
  if (length == NULL) length = &fake_length;
  long args[] = { INT_TO_ARG(program), INT_TO_ARG(index), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(size), POINTER_TO_ARG(type), POINTER_TO_ARG(name) };
  int args_size[] = { 0, 0, 0, sizeof(int), sizeof(int), sizeof(int), maxLength };
  do_opengl_call(glGetActiveUniform_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetActiveVaryingNV)(GLuint program,
                             GLuint index,
                             GLsizei bufSize,
                             GLsizei *length,
                             GLsizei *size,
                             GLenum *type,
                             GLchar *name)
{
  CHECK_PROC(glGetActiveVaryingNV);
  int fake_length;
  if (length == NULL) length = &fake_length;
  long args[] = { INT_TO_ARG(program), INT_TO_ARG(index), INT_TO_ARG(bufSize), POINTER_TO_ARG(length), POINTER_TO_ARG(size), POINTER_TO_ARG(type), POINTER_TO_ARG(name) };
  int args_size[] = { 0, 0, 0, sizeof(int), sizeof(int), sizeof(int), bufSize };
  do_opengl_call(glGetActiveVaryingNV_func, NULL, CHECK_ARGS(args, args_size));
}

static int _get_size_of_gl_uniform_variables(GLenum type)
{
  switch(type)
  {
    case GL_FLOAT:       return sizeof(float);
    case GL_FLOAT_VEC2:  return 2*sizeof(float);
    case GL_FLOAT_VEC3:  return 3*sizeof(float);
    case GL_FLOAT_VEC4:  return 4*sizeof(float);
    case GL_INT:         return sizeof(int);
    case GL_INT_VEC2:    return 2*sizeof(int);
    case GL_INT_VEC3:    return 3*sizeof(int);
    case GL_INT_VEC4:    return 4*sizeof(int);
    case GL_BOOL:        return sizeof(int);
    case GL_BOOL_VEC2:   return 2*sizeof(int);
    case GL_BOOL_VEC3:   return 3*sizeof(int);
    case GL_BOOL_VEC4:   return 4*sizeof(int);
    case GL_FLOAT_MAT2:  return 2*2*sizeof(float);
    case GL_FLOAT_MAT3:  return 3*3*sizeof(float);
    case GL_FLOAT_MAT4:  return 4*4*sizeof(float);
    case GL_FLOAT_MAT2x3:return 2*3*sizeof(float);
    case GL_FLOAT_MAT2x4:return 2*4*sizeof(float);
    case GL_FLOAT_MAT3x2:return 3*2*sizeof(float);
    case GL_FLOAT_MAT3x4:return 3*4*sizeof(float);
    case GL_FLOAT_MAT4x2:return 4*2*sizeof(float);
    case GL_FLOAT_MAT4x3:return 4*3*sizeof(float);
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
      return sizeof(int);

    default:
      log_gl("unknown type for a uniform variable : %X\n", type);
      return 0;
  }
}

static void _gl_get_uniform(PFNGLGETPROGRAMIVPROC getProgramiv,
                            PFNGLGETACTIVEUNIFORMPROC getActiveUniform,
                            int func_number,
                            GLuint program,
                            GLint location,
                            void* params)
{
  GET_CURRENT_STATE();
  int nActiveUniforms = 0;
  int nameMaxLength = 0;
  int i;

  getProgramiv(program, GL_ACTIVE_UNIFORMS, &nActiveUniforms);
  getProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &nameMaxLength);

  char* name = malloc(nameMaxLength+1);
  char* uniformName = NULL;
  for(i=0;i<state->countUniformLocations;i++)
  {
    if (state->uniformLocations[i].program == program &&
        state->uniformLocations[i].location == location)
    {
      uniformName = state->uniformLocations[i].txt;
      break;
    }
  }
  if (uniformName == NULL)
  {
    log_gl("unknown uniform location : %d\n", location);
    return;
  }
  char* uniformName2 = NULL;
  if (strchr(uniformName, '[') == 0)
  {
    uniformName2 = malloc(strlen(uniformName) + 3 + 1);
    strcpy(uniformName2, uniformName);
    strcat(uniformName2, "[0]");
  }
  /*log_gl("nActiveUniforms=%d\n", nActiveUniforms);*/
  for(i=0;i<nActiveUniforms;i++)
  {
    int actualLength, size;
    unsigned int type;
    int index = (i == 0 && location < nActiveUniforms) ? location : (i == location) ? 0 : i;
    getActiveUniform(program, index, nameMaxLength, &actualLength, &size, &type, name);
    /*log_gl("[%d] %s\n", i, name);*/
    if (strcmp(name, uniformName) == 0 || (uniformName2 && strcmp(name, uniformName2) == 0))
    {
      long args[] = { INT_TO_ARG(program), INT_TO_ARG(location), POINTER_TO_ARG(params) };
      int args_size[] = { 0, 0, size * _get_size_of_gl_uniform_variables(type)  };
      do_opengl_call(func_number, NULL, CHECK_ARGS(args, args_size));
      break;
    }
  }
  if (i == nActiveUniforms)
  {
    log_gl("sorry : I can't retrieve %s in the list of active uniforms\n", uniformName);
  }
  free(uniformName2);
  free(name);
}

GLAPI void APIENTRY EXT_FUNC(glGetUniformfvARB)(GLuint program,
                                 GLint location,
                                 GLfloat *params)
{
  CHECK_PROC(glGetUniformfvARB);
  _gl_get_uniform(glGetProgramivARB, glGetActiveUniformARB, glGetUniformfvARB_func, program, location, params);
}

GLAPI void APIENTRY EXT_FUNC(glGetUniformfv)(GLuint program,
                                 GLint location,
                                 GLfloat *params)
{
  CHECK_PROC(glGetUniformfv);
  _gl_get_uniform(glGetProgramiv, glGetActiveUniform, glGetUniformfv_func, program, location, params);
}


GLAPI void APIENTRY EXT_FUNC(glGetUniformivARB)(GLuint program,
                                 GLint location,
                                 GLint *params)
{
  CHECK_PROC(glGetUniformivARB);
  _gl_get_uniform(glGetProgramivARB, glGetActiveUniformARB, glGetUniformivARB_func, program, location, params);
}

GLAPI void APIENTRY EXT_FUNC(glGetUniformuivEXT)(GLuint program,
                                 GLint location,
                                 GLuint *params)
{
  CHECK_PROC(glGetUniformuivEXT);
  _gl_get_uniform(glGetProgramivARB, glGetActiveUniformARB, glGetUniformuivEXT_func, program, location, params);
}

GLAPI void APIENTRY EXT_FUNC(glGetUniformiv)(GLuint program,
                                 GLint location,
                                 GLint *params)
{
  CHECK_PROC(glGetUniformiv);
  _gl_get_uniform(glGetProgramiv, glGetActiveUniform, glGetUniformiv_func, program, location, params);
}

GLAPI void APIENTRY EXT_FUNC(glGetShaderSourceARB)(GLuint shader,
                                    GLsizei maxLength,
                                    GLsizei *length,
                                    GLcharARB *source)
{
  CHECK_PROC(glGetShaderSourceARB);
  int fake_length;
  if (length == NULL) length = &fake_length;
  long args[] = { INT_TO_ARG(shader), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(source) };
  int args_size[] = { 0, 0, sizeof(int), maxLength };
  do_opengl_call(glGetShaderSourceARB_func, NULL, CHECK_ARGS(args, args_size));
  log_gl("glGetShaderSourceARB : %s\n", source);
}

GLAPI void APIENTRY EXT_FUNC(glGetShaderSource)(GLuint shader,
                                    GLsizei maxLength,
                                    GLsizei *length,
                                    GLcharARB *source)
{
  CHECK_PROC(glGetShaderSource);
  int fake_length;
  if (length == NULL) length = &fake_length;
  long args[] = { INT_TO_ARG(shader), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(source) };
  int args_size[] = { 0, 0, sizeof(int), maxLength };
  do_opengl_call(glGetShaderSource_func, NULL, CHECK_ARGS(args, args_size));
  log_gl("glGetShaderSource : %s\n", source);
}


GLAPI void APIENTRY EXT_FUNC(glGetShaderInfoLog)(GLuint shader,
                                                    GLsizei maxLength,
                                                    GLsizei *length,
                                                    GLchar *infoLog)
{
  CHECK_PROC(glGetShaderInfoLog);
  int fake_length;
  if (length == NULL) length = &fake_length;
  long args[] = { INT_TO_ARG(shader), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(infoLog) };
  int args_size[] = { 0, 0, sizeof(int), maxLength };
  do_opengl_call(glGetShaderInfoLog_func, NULL, CHECK_ARGS(args, args_size));
  log_gl("glGetShaderInfoLog: %s\n", infoLog);
}


static ObjectBufferATI* _new_object_buffer_ATI()
{
  GET_CURRENT_STATE();
  int i;
  for(i=0;i<32768;i++)
  {
    if (state->objectBuffersATI[i].bufferid == 0)
    {
      memset(&state->objectBuffersATI[i], 0, sizeof(ObjectBufferATI));
      return &state->objectBuffersATI[i];
    }
  }
  return NULL;
}

static ObjectBufferATI* _find_object_buffer_ATI_from_id(GLuint buffer)
{
  GET_CURRENT_STATE();
  int i;
  for(i=0;i<32768;i++)
  {
    if (state->objectBuffersATI[i].bufferid == buffer)
    {
      return &state->objectBuffersATI[i];
    }
  }
  return NULL;
}

static void _free_object_buffer_ATI(ObjectBufferATI* objectBufferATI)
{
  if (objectBufferATI == NULL) return;

  if (objectBufferATI->ptr)
    free(objectBufferATI->ptr);
  objectBufferATI->ptr = NULL;
  if (objectBufferATI->ptrMapped)
    free(objectBufferATI->ptrMapped);
  objectBufferATI->ptrMapped = NULL;
  if (objectBufferATI->ptrUpdatedWhileMapped)
    free(objectBufferATI->ptrUpdatedWhileMapped);
  objectBufferATI->ptrUpdatedWhileMapped = NULL;
  if (objectBufferATI->updatedRangesAfterMapping.ranges)
    free(objectBufferATI->updatedRangesAfterMapping.ranges);
  objectBufferATI->updatedRangesAfterMapping.ranges = NULL;
  objectBufferATI->updatedRangesAfterMapping.nb = 0;
  objectBufferATI->bufferid = 0;
  objectBufferATI->size = 0;
}

GLAPI GLuint APIENTRY EXT_FUNC(glNewObjectBufferATI) (GLsizei size, const GLvoid *pointer, GLenum usage)
{
  int buffer = 0;
  CHECK_PROC_WITH_RET(glNewObjectBufferATI);
  long args[] = { INT_TO_ARG(size), POINTER_TO_ARG(pointer), INT_TO_ARG(usage) };
  int args_size[] = { 0, (pointer) ? size : 0, 0 };
  do_opengl_call(glNewObjectBufferATI_func, &buffer, CHECK_ARGS(args, args_size));
  //log_gl("glNewObjectBufferATI(%d,%p) --> %d\n", size, pointer, buffer);

  if (buffer != 0)
  {
    ObjectBufferATI* objectBufferATI = _new_object_buffer_ATI();
    if (objectBufferATI)
    {
      objectBufferATI->bufferid = buffer;
      objectBufferATI->size = size;
      objectBufferATI->ptr = malloc(size);
      objectBufferATI->ptrMapped = NULL;
      if (pointer)
        memcpy(objectBufferATI->ptr, pointer, size);
    }
  }

  return buffer;
}

GLAPI void APIENTRY EXT_FUNC(glFreeObjectBufferATI) (GLuint buffer)
{
  CHECK_PROC(glFreeObjectBufferATI);
  long args[] = { UNSIGNED_INT_TO_ARG(buffer)};
  do_opengl_call(glFreeObjectBufferATI_func, NULL, args, NULL);
  _free_object_buffer_ATI(_find_object_buffer_ATI_from_id(buffer));
}

static void _add_int_range_to_ranges(IntSetRanges* ranges, int start, int length)
{
  int i,j;
  for(i=0;i<ranges->nb;i++)
  {
    IntRange* range = &ranges->ranges[i];
    if (start <= range->start)
    {
      if (start + length < range->start)
      {
        if (ranges->nb == ranges->maxNb)
          ranges->ranges = realloc(ranges->ranges, sizeof(IntRange) * (ranges->nb+1));
        memmove(&ranges->ranges[i+1], &ranges->ranges[i], sizeof(IntRange) * (ranges->nb - i));
        ranges->nb++;
        if (ranges->nb > ranges->maxNb)
          ranges->maxNb = ranges->nb;
        ranges->ranges[i].start = start;
        ranges->ranges[i].length = length;
        return;
      }
      else if (start + length <= range->start + range->length)
      {
        range->length = range->start + range->length - start;
        range->start = start;
        return;
      }
      else
      {
        j = i + 1;
        range->start = start;
        range->length = start + length - range->start;
        while(j < ranges->nb && start + length >= ranges->ranges[j].start)
        {
          if (start + length <= ranges->ranges[j].start + ranges->ranges[j].length)
          {
            range->length = ranges->ranges[j].start + ranges->ranges[j].length - range->start;
            j++;
            break;
          }
          j++;
        }
        if (i+1<j && j < ranges->nb)
          memmove(&ranges->ranges[i+1], &ranges->ranges[j], sizeof(IntRange) * (j - (i + 1)));
        ranges->nb -= j - (i+1);
        return;
      }
    }
    else
    {
      if (start > range->start + range->length)
      {
        continue;
      }
      else if (start + length <= range->start + range->length)
      {
        return;
      }
      else
      {
        j = i + 1;
        range->length = start + length - range->start;
        while(j < ranges->nb && start + length >= ranges->ranges[j].start)
        {
          if (start + length <= ranges->ranges[j].start + ranges->ranges[j].length)
          {
            range->length = ranges->ranges[j].start + ranges->ranges[j].length - range->start;
            j++;
            break;
          }
          j++;
        }
        if (i+1<j && j < ranges->nb)
          memmove(&ranges->ranges[i+1], &ranges->ranges[j], sizeof(IntRange) * (j - (i + 1)));
        ranges->nb -= j - (i+1);
        return;
      }
    }
  }
  if (ranges->nb == ranges->maxNb)
     ranges->ranges = realloc(ranges->ranges, sizeof(IntRange) * (ranges->nb+1));
  ranges->ranges[ranges->nb].start = start;
  ranges->ranges[ranges->nb].length = length;
  ranges->nb++;
  if (ranges->nb > ranges->maxNb)
    ranges->maxNb = ranges->nb;
  return;
}

static IntSetRanges _get_empty_ranges(IntSetRanges* inRanges, int start, int length)
{
  IntSetRanges outRanges = {0};
  int i;
  int end = start+length;
  int last_end = 0x80000000;
  for(i=0;i<=inRanges->nb;i++)
  {
    int cur_start, cur_end;
    if (i == inRanges->nb)
    {
      cur_start = cur_end = 0x7FFFFFFF;
    }
    else
    {
      IntRange* range = &inRanges->ranges[i];
      cur_start = range->start;
      cur_end = range->start + range->length;
    }

    /* [last_end,cur_start[ inter [start,end[ */
    if ((last_end >= start && last_end < end) || (start >= last_end && start < cur_start))
    {
      outRanges.ranges = realloc(outRanges.ranges, sizeof(IntRange) * (outRanges.nb+1));
      outRanges.ranges[outRanges.nb].start = MAX(start,last_end);
      outRanges.ranges[outRanges.nb].length = MIN(end,cur_start) - outRanges.ranges[outRanges.nb].start;
      outRanges.nb++;
    }

    last_end = cur_end;
  }
  return outRanges;
}

GLAPI void APIENTRY EXT_FUNC(glUpdateObjectBufferATI) (GLuint buffer, GLuint offset, GLsizei size, const GLvoid *pointer, GLenum preserve)
{
  CHECK_PROC(glUpdateObjectBufferATI);
  //log_gl("glUpdateObjectBufferATI(%d, %d, %d)\n", buffer, offset, size);
  long args[] = { INT_TO_ARG(buffer), INT_TO_ARG(offset), INT_TO_ARG(size), POINTER_TO_ARG(pointer), INT_TO_ARG(preserve) };
  int args_size[] = { 0, 0, 0, size, 0 };
  do_opengl_call(glUpdateObjectBufferATI_func, NULL, CHECK_ARGS(args, args_size));
  ObjectBufferATI* objectBufferATI = _find_object_buffer_ATI_from_id(buffer);
  if (objectBufferATI)
  {
    if (offset >= 0 && offset + size <= objectBufferATI->size)
    {
      if (objectBufferATI->ptrMapped)
      {
        log_gl("you shouldn't call glUpdateObjectBufferATI after glMapObjectBufferATI. we're emulating ATI fglrx (strange) behaviour\n");
        objectBufferATI->updatedRangesAfterMapping.nb = 0;
        _add_int_range_to_ranges(&objectBufferATI->updatedRangesAfterMapping, offset, size);
        objectBufferATI->ptrUpdatedWhileMapped = realloc(objectBufferATI->ptrUpdatedWhileMapped, size);
        memcpy(objectBufferATI->ptrUpdatedWhileMapped, pointer, size);
      }
      else
      {
        memcpy(objectBufferATI->ptr + offset, pointer, size);
      }
    }
    else
    {
      log_gl("offset >= 0 && offset + size <= state->objectBuffersATI[i].size failed\n");
    }
  }
}

GLAPI GLvoid* APIENTRY EXT_FUNC(glMapObjectBufferATI) (GLuint buffer)
{
  CHECK_PROC_WITH_RET(glMapObjectBufferATI);
  //log_gl("glMapObjectBufferATI(%d)\n", buffer);
  ObjectBufferATI* objectBufferATI = _find_object_buffer_ATI_from_id(buffer);
  if (objectBufferATI)
  {
    if (objectBufferATI->ptrMapped == NULL)
    {
      objectBufferATI->ptrMapped = malloc(objectBufferATI->size);
      memcpy(objectBufferATI->ptrMapped,
             objectBufferATI->ptr,
             objectBufferATI->size);
      return objectBufferATI->ptrMapped;
    }
    else
      return NULL;
  }
  else
    return NULL;
}

GLAPI void APIENTRY EXT_FUNC(glUnmapObjectBufferATI) (GLuint buffer)
{
  CHECK_PROC(glUnmapObjectBufferATI);
  //log_gl("glUnmapObjectBufferATI(%d)\n", buffer);
  ObjectBufferATI* objectBufferATI = _find_object_buffer_ATI_from_id(buffer);
  if (objectBufferATI)
  {
    if (objectBufferATI->ptrMapped)
    {
      IntSetRanges outRanges = _get_empty_ranges(&objectBufferATI->updatedRangesAfterMapping, 0, objectBufferATI->size);
      int i;
      void* ptrMapped = objectBufferATI->ptrMapped;
      if (objectBufferATI->ptrUpdatedWhileMapped)
      {
        assert(objectBufferATI->updatedRangesAfterMapping.nb == 1);
        memcpy(objectBufferATI->ptr + objectBufferATI->updatedRangesAfterMapping.ranges[0].start,
               objectBufferATI->ptrUpdatedWhileMapped,
               objectBufferATI->updatedRangesAfterMapping.ranges[0].length);
        free(objectBufferATI->ptrUpdatedWhileMapped);
        objectBufferATI->ptrUpdatedWhileMapped = NULL;
      }
      objectBufferATI->updatedRangesAfterMapping.nb = 0;
      objectBufferATI->ptrMapped = NULL;
      for(i=0;i<outRanges.nb;i++)
      {
        glUpdateObjectBufferATI(buffer, outRanges.ranges[i].start, outRanges.ranges[i].length,
                                ptrMapped, GL_DISCARD_ATI);
      }
      free(outRanges.ranges);
      free(ptrMapped);
    }
  }
}

GLAPI void APIENTRY EXT_FUNC(glGetActiveAttribARB)(GLhandleARB program,
                                    GLuint index,
                                    GLsizei maxLength,
                                    GLsizei *length,
                                    GLint *size,
                                    GLenum *type,
                                    GLcharARB *name)
{
  CHECK_PROC(glGetActiveAttribARB);
  int fake_length;
  if (length == NULL) length = &fake_length;
  long args[] = { INT_TO_ARG(program), INT_TO_ARG(index), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(size), POINTER_TO_ARG(type), POINTER_TO_ARG(name) };
  int args_size[] = { 0, 0, 0, sizeof(int), sizeof(int), sizeof(int), maxLength };
  do_opengl_call(glGetActiveAttribARB_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY EXT_FUNC(glGetActiveAttrib)(GLhandleARB program,
                                    GLuint index,
                                    GLsizei maxLength,
                                    GLsizei *length,
                                    GLint *size,
                                    GLenum *type,
                                    GLcharARB *name)
{
  CHECK_PROC(glGetActiveAttrib);
  int fake_length;
  if (length == NULL) length = &fake_length;
  long args[] = { INT_TO_ARG(program), INT_TO_ARG(index), INT_TO_ARG(maxLength), POINTER_TO_ARG(length), POINTER_TO_ARG(size), POINTER_TO_ARG(type), POINTER_TO_ARG(name) };
  int args_size[] = { 0, 0, 0, sizeof(int), sizeof(int), sizeof(int), maxLength };
  do_opengl_call(glGetActiveAttrib_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI GLint APIENTRY EXT_FUNC(glGetAttribLocationARB)(GLhandleARB program,
                                       const GLcharARB *name)
{
  CHECK_PROC_WITH_RET(glGetAttribLocationARB);
  int ret = 0;
  long args[] = { INT_TO_ARG(program), POINTER_TO_ARG(name) };
  do_opengl_call(glGetAttribLocationARB_func, &ret, args, NULL);
  return ret;
}

GLAPI GLint APIENTRY EXT_FUNC(glGetAttribLocation)(GLhandleARB program,
                                       const GLcharARB *name)
{
  CHECK_PROC_WITH_RET(glGetAttribLocation);
  int ret = 0;
  long args[] = { INT_TO_ARG(program), POINTER_TO_ARG(name) };
  do_opengl_call(glGetAttribLocation_func, &ret, args, NULL);
  return ret;
}

GLAPI void APIENTRY glGetDetailTexFuncSGIS (GLenum target, GLfloat *points)
{
  int npoints = 0;
  CHECK_PROC(glGetDetailTexFuncSGIS);
  glGetTexParameteriv(target, GL_DETAIL_TEXTURE_FUNC_POINTS_SGIS, &npoints);
  long args[] = { INT_TO_ARG(target), POINTER_TO_ARG(points) };
  int args_size[] = { 0, 2 * npoints * sizeof(float) };
  do_opengl_call(glGetDetailTexFuncSGIS_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glGetSharpenTexFuncSGIS (GLenum target, GLfloat *points)
{
  int npoints = 0;
  CHECK_PROC(glGetSharpenTexFuncSGIS);
  glGetTexParameteriv(target, GL_SHARPEN_TEXTURE_FUNC_POINTS_SGIS, &npoints);
  long args[] = { INT_TO_ARG(target), POINTER_TO_ARG(points) };
  int args_size[] = { 0, 2 * npoints * sizeof(float) };
  do_opengl_call(glGetSharpenTexFuncSGIS_func, NULL, CHECK_ARGS(args, args_size));
}

GLAPI void APIENTRY glColorTable (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table)
{
  NOT_IMPLEMENTED(glColorTable);
}

GLAPI void APIENTRY glColorTableEXT (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table)
{
  NOT_IMPLEMENTED(glColorTableEXT);
}


GLAPI void APIENTRY glColorSubTable (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data)
{
  NOT_IMPLEMENTED(glColorSubTable);
}

GLAPI void APIENTRY glColorSubTableEXT (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data)
{
  NOT_IMPLEMENTED(glColorSubTableEXT);
}


GLAPI void APIENTRY glGetColorTable (GLenum target, GLenum format, GLenum type, GLvoid *table)
{
  NOT_IMPLEMENTED(glGetColorTable);
}

GLAPI void APIENTRY glGetColorTableEXT (GLenum target, GLenum format, GLenum type, GLvoid *table)
{
  NOT_IMPLEMENTED(glGetColorTableEXT);
}


GLAPI void APIENTRY glConvolutionFilter1D (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *image)
{
  NOT_IMPLEMENTED(glConvolutionFilter1D);
}

GLAPI void APIENTRY glConvolutionFilter1DEXT (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *image)
{
  NOT_IMPLEMENTED(glConvolutionFilter1DEXT);
}

GLAPI void APIENTRY glConvolutionFilter2D (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *image)
{
  NOT_IMPLEMENTED(glConvolutionFilter2D);
}

GLAPI void APIENTRY glConvolutionFilter2DEXT (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *image)
{
  NOT_IMPLEMENTED(glConvolutionFilter2DEXT);
}


GLAPI void APIENTRY glGetConvolutionFilter (GLenum target, GLenum format, GLenum type, GLvoid *image)
{
  NOT_IMPLEMENTED(glGetConvolutionFilter);
}

GLAPI void APIENTRY glGetConvolutionFilterEXT (GLenum target, GLenum format, GLenum type, GLvoid *image)
{
  NOT_IMPLEMENTED(glGetConvolutionFilterEXT);
}

GLAPI void APIENTRY glGetSeparableFilter (GLenum target, GLenum format, GLenum type, GLvoid *row, GLvoid *column, GLvoid *span)
{
  NOT_IMPLEMENTED(glGetSeparableFilter);
}

GLAPI void APIENTRY glGetSeparableFilterEXT (GLenum target, GLenum format, GLenum type, GLvoid *row, GLvoid *column, GLvoid *span)
{
  NOT_IMPLEMENTED(glGetSeparableFilterEXT);
}

GLAPI void APIENTRY glSeparableFilter2D (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *row, const GLvoid *column)
{
  NOT_IMPLEMENTED(glSeparableFilter2D);
}

GLAPI void APIENTRY glSeparableFilter2DEXT (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *row, const GLvoid *column)
{
  NOT_IMPLEMENTED(glSeparableFilter2DEXT);
}


GLAPI void APIENTRY glGetHistogram (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values)
{
  NOT_IMPLEMENTED(glGetHistogram);
}

GLAPI void APIENTRY glGetHistogramEXT (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values)
{
  NOT_IMPLEMENTED(glGetHistogramEXT);
}


GLAPI void APIENTRY glGetMinmax (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values)
{
  NOT_IMPLEMENTED(glGetMinmax);
}

GLAPI void APIENTRY glGetMinmaxEXT (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values)
{
  NOT_IMPLEMENTED(glGetMinmaxEXT);
}

GLAPI void* APIENTRY glXAllocateMemoryNV(GLsizei size, GLfloat readfreq, GLfloat writefreq, GLfloat priority)
{
  return malloc(size);
}

GLAPI void APIENTRY glXFreeMemoryNV(GLvoid *pointer)
{
  free(pointer);
}

GLAPI void APIENTRY glPixelDataRangeNV (GLenum target, GLsizei length, GLvoid *pointer)
{
  CHECK_PROC(glPixelDataRangeNV);
  /* do nothing is a possible implementation... */
}

GLAPI void APIENTRY glFlushPixelDataRangeNV (GLenum target)
{
  CHECK_PROC(glFlushPixelDataRangeNV);
  /* do nothing is a possible implementation... */
}

GLAPI void APIENTRY glVertexArrayRangeNV (GLsizei size, const GLvoid *ptr)
{
  CHECK_PROC(glVertexArrayRangeNV);
  /* do nothing is a possible implementation... */
}

GLAPI void APIENTRY glFlushVertexArrayRangeNV (void)
{
  CHECK_PROC(glFlushVertexArrayRangeNV);
  /* do nothing is a possible implementation... */
}

