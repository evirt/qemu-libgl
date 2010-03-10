#include <pthread.h>
#include "common.h"
#include "log.h"

#ifdef ENABLE_THREAD_SAFETY

extern GLState* new_gl_state();

extern pthread_mutex_t global_mutex;
extern pthread_key_t   key_current_gl_state;

extern pthread_t last_current_thread;
extern GLState* _mono_threaded_current_gl_state;
extern GLState* default_gl_state;


/* Posix threading */
/* The concepts used here are coming directly from http://www.mesa3d.org/dispatch.html */

#ifdef TEST_IF_LOCK_USE_IS_CORRECT_INTO_MONO_THREADED_CASE
  static int lock_count = 0;
  #define LOCK(func_number)  int __lock__ = 0; assert(lock_count == 0); lock_count++; pthread_mutex_lock( &global_mutex );
  #define UNLOCK(func_number) assert(lock_count + __lock__ == 1); lock_count--; pthread_mutex_unlock( &global_mutex );
  #define IS_MT() 2
  static int _is_mt = 2;
#else
  static int _is_mt = 0;
  static inline int is_mt()
  {
    if (_is_mt) return 1;
    pthread_t current_thread = pthread_self();
    if (last_current_thread == 0)
      last_current_thread = current_thread;
    if (current_thread != last_current_thread)
    {
      _is_mt = 1;
      log_gl("-------- Two threads at least are doing OpenGL ---------\n");
      pthread_key_create(&key_current_gl_state, NULL);
    }
    return _is_mt;
  }
  #define IS_MT() is_mt()

  /* The idea here is that the first GL/GLX call made in each thread is necessary a GLX call */
  /* So in the case where it's a GLX call we always take the lock and check if we're in MT case */
  /* otherwise (regular GL call), we have to take the lock only in the MT case */
  #define LOCK(func_number) do { if (IS_GLX_CALL(func_number)) { pthread_mutex_lock( &global_mutex ); IS_MT(); } else if (_is_mt) {  pthread_mutex_lock( &global_mutex ); } } while(0)
  #define UNLOCK(func_number) do { if (IS_GLX_CALL(func_number) || _is_mt) pthread_mutex_unlock( &global_mutex ); } while(0)
#endif

static void set_current_state(GLState* current_gl_state)
{
  if (_is_mt)
    pthread_setspecific(key_current_gl_state, current_gl_state);
  else
    _mono_threaded_current_gl_state = current_gl_state;
}

static inline GLState* get_current_state()
{
  GLState* current_gl_state;
  if (_is_mt == 1 &&
      last_current_thread == pthread_self())
  {
    _is_mt = 2;
    set_current_state(_mono_threaded_current_gl_state);
    _mono_threaded_current_gl_state = NULL;
  }
  current_gl_state = (_is_mt) ? pthread_getspecific(key_current_gl_state) : _mono_threaded_current_gl_state;
  if (current_gl_state == NULL)
  {
    if (default_gl_state == NULL)
    {
      default_gl_state = new_gl_state();
    }
    current_gl_state = default_gl_state;
    set_current_state(current_gl_state);
  }
  return current_gl_state;
}
#define SET_CURRENT_STATE(_x) set_current_state(_x)

/* No support for threading */
#else
#define LOCK(func_number)
#define UNLOCK(func_number)
#define GET_CURRENT_THREAD() 0
#define IS_MT() 0
static GLState* current_gl_state = NULL;
static inline GLState* get_current_state()
{
  if (current_gl_state == NULL)
  {
    if (default_gl_state == NULL)
    {
      default_gl_state = new_gl_state();
    }
    return default_gl_state;
  }
  return current_gl_state;
}
#define SET_CURRENT_STATE(_x) current_gl_state = _x
#endif

#define GET_CURRENT_STATE()   GLState* state = get_current_state()
