extern void glGetIntegerv_no_lock( GLenum pname, GLint *params );
extern void glPixelStorei_no_lock( GLenum pname, GLint param );
extern void glBindBufferARB_no_lock (GLenum target, GLuint buffer);
extern void glReadPixels_no_lock  ( GLint x, GLint y,
                                    GLsizei width, GLsizei height,
                                    GLenum format, GLenum type,
                                    GLvoid *pixels );

