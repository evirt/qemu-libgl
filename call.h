extern void do_opengl_call_no_lock(int func_number, void* ret_ptr, long* args, int* args_size_opt);

static inline void do_opengl_call(int func_number, void* ret_ptr, long* args, int* args_size_opt)
{
  LOCK(func_number);
  do_opengl_call_no_lock(func_number, ret_ptr, args, args_size_opt);
  UNLOCK(func_number);
}
