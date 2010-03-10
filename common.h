#ifndef __INCLUDE_COMMON_H

#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include "mesa_glx.h"

#define ENABLE_THREAD_SAFETY

extern int debug_gl;
extern int debug_array_ptr;
extern int disable_optim;
extern int limit_fps;

#define IS_GLX_CALL(x) (x >= glXChooseVisual_func && x <= glXReleaseTexImageARB_func)

#define RET_STRING_SIZE 32768
#define SIZE_BUFFER_COMMAND 65536*16

#define POINTER_TO_ARG(x)            (long)(void*)(x)
#define CHAR_TO_ARG(x)               (long)(x)
#define SHORT_TO_ARG(x)              (long)(x)
#define INT_TO_ARG(x)                (long)(x)
#define UNSIGNED_CHAR_TO_ARG(x)      (long)(x)
#define UNSIGNED_SHORT_TO_ARG(x)     (long)(x)
#define UNSIGNED_INT_TO_ARG(x)       (long)(x)
#define FLOAT_TO_ARG(x)              (long)*((int*)(&x))
#define DOUBLE_TO_ARG(x)             (long)(&x)

#define NOT_IMPLEMENTED(x) log_gl(#x " not implemented !\n")
//#define PROVIDE_STUB_IMPLEMENTATION

#define CHECK_ARGS(x, y) (1 / ((sizeof(x)/sizeof(x[0])) == (sizeof(y)/sizeof(y[0])) ? 1 : 0)) ? x : x, y

#define EXT_FUNC(x) x

#define CONCAT(a, b) a##b
#define DEFINE_EXT(glFunc, paramsDecl, paramsCall)  GLAPI void APIENTRY CONCAT(glFunc,EXT) paramsDecl { glFunc paramsCall; }

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif


#define NB_MAX_TEXTURES 16
#define MY_GL_MAX_VERTEX_ATTRIBS_ARB 16
#define MY_GL_MAX_VERTEX_ATTRIBS_NV 16
#define MY_GL_MAX_VARIANT_POINTER_EXT 16

#define CHECK_PROC(x)
#define CHECK_PROC_WITH_RET(x)

//static void log_gl(const char* format, ...);

#include "opengl_utils.h"

typedef struct
{
  int size;
  int type;
  int stride;
  const void* ptr;

  int index;
  int normalized;

  int enabled;
  int vbo_name;

  unsigned int last_crc;
} ClientArray;

typedef struct {
    GLboolean swapEndian;
    GLboolean lsbFirst;
    GLuint rowLength;
    GLuint imageHeight;
    GLuint skipRows;
    GLuint skipPixels;
    GLuint skipImages;
    GLuint alignment;
} PixelStoreMode;

typedef struct
{
  int format;
  int stride;
  const void* ptr;
} InterleavedArrays;

typedef struct {
  ClientArray vertexArray;
  ClientArray normalArray;
  ClientArray colorArray;
  ClientArray secondaryColorArray;
  ClientArray indexArray;
  ClientArray edgeFlagArray;
  ClientArray weightArray;
  ClientArray matrixIndexArray;
  ClientArray fogCoordArray;
  ClientArray texCoordArray[NB_MAX_TEXTURES];
  ClientArray vertexAttribArray[MY_GL_MAX_VERTEX_ATTRIBS_ARB];
  ClientArray vertexAttribArrayNV[MY_GL_MAX_VERTEX_ATTRIBS_NV];
  ClientArray variantPointer[MY_GL_MAX_VARIANT_POINTER_EXT];
  ClientArray elementArrayATI;
  InterleavedArrays interleavedArrays;
} ClientArrays;


typedef struct
{
  GLbitfield     mask;
  PixelStoreMode pack;
  PixelStoreMode unpack;
  ClientArrays   arrays;
  int clientActiveTexture;
  int selectBufferSize;
  void* selectBufferPtr;
  int feedbackBufferSize;
  void* feedbackBufferPtr;
} ClientState;

#define MAX_MATRIX_STACK_SIZE  64
#define NB_GL_MATRIX    (3+NB_MAX_TEXTURES+31)
typedef struct
{
  double val[16];
} Matrixd;

typedef struct
{
  Matrixd stack[MAX_MATRIX_STACK_SIZE];
  Matrixd current;
  int sp;
} GLMatrix;

typedef struct
{
  int x;
  int y;
  int width;
  int height;
} ViewportStruct;

typedef struct
{
  int x;
  int y;
  int width;
  int height;
  int map_state;
} WindowPosStruct;


typedef struct
{
  int id;
  int datatype;
  int components;
} Symbol;

typedef struct
{
  Symbol* tab;
  int count;
} Symbols;

typedef struct
{
  int texture;
  int level;

  int width;
  int height;
} Texture2DDim;

typedef struct
{
  float mode;
  float density;
  float start;
  float end;
  float index;
  float color[4];
} Fog;

typedef struct
{
  int size;
  void* ptr;
  int mapped;
  int usage;
  int access;
} Buffer;

typedef struct
{
  int start;
  int length;
} IntRange;

typedef struct
{
  IntRange* ranges;
  int nb;
  int maxNb;
} IntSetRanges;

typedef struct
{
  int bufferid;
  int size;
  void* ptr;
  void* ptrMapped;
  IntSetRanges updatedRangesAfterMapping;
  void* ptrUpdatedWhileMapped;
} ObjectBufferATI;

typedef struct
{
  GLuint program;
  char* txt;
  int location;
} UniformLocation;


#define MAX_CLIENT_STATE_STACK_SIZE 16
#define MAX_SERVER_STATE_STACK_SIZE 16

#define N_CLIP_PLANES   8
typedef struct
{
  GLbitfield     mask;
  int            matrixMode;
  int            bindTexture2D; /* optimization for openquartz  */
  int            bindTextureRectangle;
  int            linesmoothEnabled; /* optimization for googleearth */
  int            lightingEnabled;
  int            texture2DEnabled;
  int            blendEnabled;
  int            scissorTestEnabled;
  int            vertexProgramEnabled;
  int            fogEnabled;
  int            depthFunc;
  Fog            fog;

  Texture2DDim*  texture2DCache;
  int            texture2DCacheDim;
  Texture2DDim*  textureProxy2DCache;
  int            textureProxy2DCacheDim;
  Texture2DDim*  textureRectangleCache;
  int            textureRectangleCacheDim;
  Texture2DDim*  textureProxyRectangleCache;
  int            textureProxyRectangleCacheDim;

  double         clipPlanes[N_CLIP_PLANES][4];
} ServerState;




typedef struct
{
  int ref;
  Display* display;
  GLXContext context;
  GLXDrawable current_drawable;
  GLXDrawable current_read_drawable;
  struct timeval last_swap_buffer_time;
  GLXContext shareList;
  XFixesCursorImage last_cursor;
  GLXPbuffer pbuffer;

  int isAssociatedToFBConfigVisual;

  ClientState client_state_stack[MAX_CLIENT_STATE_STACK_SIZE];
  ClientState client_state;
  int client_state_sp;

  ServerState server_stack[MAX_SERVER_STATE_STACK_SIZE];
  ServerState current_server_state;
  int server_sp;

  int            arrayBuffer; /* optimization for ww2d */
  int            elementArrayBuffer;
  int            pixelUnpackBuffer;
  int            pixelPackBuffer;

  Buffer arrayBuffers[32768];
  Buffer elementArrayBuffers[32768];
  Buffer pixelUnpackBuffers[32768];
  Buffer pixelPackBuffers[32768];

  RangeAllocator ownTextureAllocator;
  RangeAllocator* textureAllocator;
  RangeAllocator ownBufferAllocator;
  RangeAllocator* bufferAllocator;
  RangeAllocator ownListAllocator;
  RangeAllocator* listAllocator;

  Symbols symbols;

  ObjectBufferATI objectBuffersATI[32768];

  UniformLocation* uniformLocations;
  int countUniformLocations;

  WindowPosStruct oldPos;
  ViewportStruct viewport;
  ViewportStruct scissorbox;
  int drawable_width;
  int drawable_height;

  int currentRasterPosKnown;
  float currentRasterPos[4];

  char* tuxRacerBuffer;

  int activeTexture;

  int isBetweenBegin;

  int lastGlError;

  int isBetweenLockArrays;
  int locked_first;
  int locked_count;

  GLMatrix matrix[NB_GL_MATRIX];
} GLState;

extern int nbGLStates;
extern GLState** glstates;
extern GLState* default_gl_state;

#define __INCLUDE_COMMON_H
#endif /* __INCLUDE_COMMON_H */

