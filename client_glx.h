
extern Bool glXMakeCurrent_no_lock( Display *dpy, GLXDrawable drawable, GLXContext ctx);
extern void glXSwapBuffers_no_lock( Display *dpy, GLXDrawable drawable );
extern __GLXextFuncPtr glXGetProcAddress_no_lock(const GLubyte * name);

