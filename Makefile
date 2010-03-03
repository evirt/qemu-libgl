BUILD_CC := gcc
GL_CFLAGS := -Wall -g -O2 -fno-strict-aliasing

all: libGL.so.1.2

libGL.so.1.2: client_stub.c opengl_client.c glgetv_cst.h opengl_func.h opengl_utils.h opengl_client_xfonts.c mesa_gl.h mesa_glext.h mesa_glx.h mesa_glxext.h
	$(CC) -fPIC $(GL_CFLAGS) opengl_client.c -shared -o libGL.so -lX11 -lXfixes -lm -L$(D)/usr/X11R6/lib -lpthread -I.

opengl_func.h: gl_func.h

client_stub.c gl_func.h: parse_gl_h mesa_gl.h mesa_glext.h gl_func_perso.h
	./parse_gl_h
glgetv_cst.h: parse_mesa_get_c mesa_get.c mesa_gl.h mesa_glext.h
	./parse_mesa_get_c
parse_gl_h: parse_gl_h.c
	$(BUILD_CC) -g -o $@ $<
parse_mesa_get_c: parse_mesa_get_c.c mesa_gl.h mesa_glext.h
	$(BUILD_CC) -g -o $@ parse_mesa_get_c.c

clean:
	rm client_stub.c gl_func.h glgetv_cst.h parse_gl_h parse_mesa_get_c
