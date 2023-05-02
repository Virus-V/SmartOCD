/*
 *  Copyright 2014 The Luvit Authors. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include <lua.h>
#if (LUA_VERSION_NUM < 503)
#include "compat-5.3.h"
#endif
#include "luv.h"

#include "async.c"
#include "check.c"
#include "constants.c"
#include "dns.c"
#include "fs.c"
#include "fs_event.c"
#include "fs_poll.c"
#include "handle.c"
#include "idle.c"
#include "lhandle.c"
#include "loop.c"
#include "lreq.c"
#include "metrics.c"
#include "misc.c"
#include "pipe.c"
#include "poll.c"
#include "prepare.c"
#include "process.c"
#include "req.c"
#include "signal.c"
#include "stream.c"
#include "tcp.c"
#include "thread.c"
#include "timer.c"
#include "tty.c"
#include "udp.c"
#include "util.c"
#include "work.c"

static const luaL_Reg luv_functions[] = {
  // loop.c
  {"LoopClose", luv_loop_close},
  {"Run", luv_run},
  {"LoopMode", luv_loop_mode},
  {"LoopAlive", luv_loop_alive},
  {"Stop", luv_stop},
  {"BackendFd", luv_backend_fd},
  {"BackendTimeout", luv_backend_timeout},
  {"Now", luv_now},
  {"UpdateTime", luv_update_time},
  {"Walk", luv_walk},
#if LUV_UV_VERSION_GEQ(1, 0, 2)
  {"LoopConfigure", luv_loop_configure},
#endif

  // req.c
  {"Cancel", luv_cancel},
#if LUV_UV_VERSION_GEQ(1, 19, 0)
  {"ReqGetType", luv_req_get_type},
#endif

  // handle.c
  {"IsActive", luv_is_active},
  {"IsClosing", luv_is_closing},
  {"Close", luv_close},
  {"Ref", luv_ref},
  {"Unref", luv_unref},
  {"HasRef", luv_has_ref},
  {"SendBufferSize", luv_send_buffer_size},
  {"RecvBufferSize", luv_recv_buffer_size},
  {"FileNo", luv_fileno},
#if LUV_UV_VERSION_GEQ(1, 19, 0)
  {"HandleGetType", luv_handle_get_type},
#endif


  // timer.c
  {"NewTimer", luv_new_timer},
  {"TimerStart", luv_timer_start},
  {"TimerStop", luv_timer_stop},
  {"TimerAgain", luv_timer_again},
  {"TimerSetRepeat", luv_timer_set_repeat},
  {"TimerGetRepeat", luv_timer_get_repeat},
#if LUV_UV_VERSION_GEQ(1, 40, 0)
  {"TimerGetDueIn", luv_timer_get_due_in},
#endif

  // prepare.c
  {"NewPrepare", luv_new_prepare},
  {"PrepareStart", luv_prepare_start},
  {"PrepareStop", luv_prepare_stop},

  // check.c
  {"NewCheck", luv_new_check},
  {"CheckStart", luv_check_start},
  {"CheckStop", luv_check_stop},

  // idle.c
  {"NewIdle", luv_new_idle},
  {"IdleStart", luv_idle_start},
  {"IdleStop", luv_idle_stop},

  // async.c
  {"NewAsync", luv_new_async},
  {"AsyncSend", luv_async_send},

  // poll.c
  {"NewPoll", luv_new_poll},
  {"NewSocketPoll", luv_new_socket_poll},
  {"PollStart", luv_poll_start},
  {"PollStop", luv_poll_stop},

  // signal.c
  {"NewSignal", luv_new_signal},
  {"SignalStart", luv_signal_start},
#if LUV_UV_VERSION_GEQ(1, 12, 0)
  {"SignalStartOneshot", luv_signal_start_oneshot},
#endif
  {"SignalStop", luv_signal_stop},

  // process.c
  {"DisableStdioInheritance", luv_disable_stdio_inheritance},
  {"Spawn", luv_spawn},
  {"ProcessKill", luv_process_kill},
#if LUV_UV_VERSION_GEQ(1, 19, 0)
  {"ProcessGetPid", luv_process_get_pid},
#endif
  {"Kill", luv_kill},

  // stream.c
  {"Shutdown", luv_shutdown},
  {"Listen", luv_listen},
  {"Accept", luv_accept},
  {"ReadStart", luv_read_start},
  {"ReadStop", luv_read_stop},
  {"Write", luv_write},
  {"Write2", luv_write2},
  {"TryWrite", luv_try_write},
#if LUV_UV_VERSION_GEQ(1, 42, 0)
  {"TryWrite2", luv_try_write2},
#endif
  {"IsReadable", luv_is_readable},
  {"IsWritable", luv_is_writable},
  {"StreamSetBlocking", luv_stream_set_blocking},
#if LUV_UV_VERSION_GEQ(1, 19, 0)
  {"StreamGetWriteQueueSize", luv_stream_get_write_queue_size},
#endif

  // tcp.c
  {"NewTCP", luv_new_tcp},
#if 0
  {"TCPOpen", luv_tcp_open},
  {"TCPNodelay", luv_tcp_nodelay},
  {"TCPKeepalive", luv_tcp_keepalive},
  {"TCPSimultaneousAccepts", luv_tcp_simultaneous_accepts},
  {"TCPBind", luv_tcp_bind},
  {"TCPGetpeername", luv_tcp_getpeername},
  {"TCPGetsockname", luv_tcp_getsockname},
  {"TCPConnect", luv_tcp_connect},
  {"TCPWriteQueueSize", luv_write_queue_size},
#if LUV_UV_VERSION_GEQ(1, 32, 0)
  {"TCPCloseReset", luv_tcp_close_reset},
#endif
#endif

#if LUV_UV_VERSION_GEQ(1, 41, 0)
  {"SocketPair", luv_socketpair},
#endif

  // pipe.c
  {"NewPipe", luv_new_pipe},
  {"PipeOpen", luv_pipe_open},
  {"PipeBind", luv_pipe_bind},
#if LUV_UV_VERSION_GEQ(1, 16, 0)
  {"PipeChmod", luv_pipe_chmod},
#endif
  {"PipeConnect", luv_pipe_connect},
  {"PipeGetSockName", luv_pipe_getsockname},
#if LUV_UV_VERSION_GEQ(1, 3, 0)
  {"PipeGetPeerName", luv_pipe_getpeername},
#endif
  {"PipePendingInstances", luv_pipe_pending_instances},
  {"PipePendingCount", luv_pipe_pending_count},
  {"PipePendingType", luv_pipe_pending_type},
#if LUV_UV_VERSION_GEQ(1, 41, 0)
  {"Pipe", luv_pipe},
#endif

  // tty.c
  {"NewTTY", luv_new_tty},
  {"TTYSetMode", luv_tty_set_mode},
  {"TTYResetMode", luv_tty_reset_mode},
  {"TTYGetWinSize", luv_tty_get_winsize},
#if LUV_UV_VERSION_GEQ(1, 33, 0)
  {"TTYSetVtermState", luv_tty_set_vterm_state},
  {"TTYGetVtermState", luv_tty_get_vterm_state},
#endif

  // udp.c
  {"NewUdp", luv_new_udp},
  {"UDPGetSendQueueSize", luv_udp_get_send_queue_size},
  {"UDPGetSendQueueCount", luv_udp_get_send_queue_count},
  {"UDPOpen", luv_udp_open},
  {"UDPBind", luv_udp_bind},
  {"UDPGetSockName", luv_udp_getsockname},
  {"UDPSetMembership", luv_udp_set_membership},
#if LUV_UV_VERSION_GEQ(1, 32, 0)
  {"UDPSetSourceMembership", luv_udp_set_source_membership},
#endif
  {"UDPSetMulticastLoop", luv_udp_set_multicast_loop},
  {"UDPSetMulticastTTL", luv_udp_set_multicast_ttl},
  {"UDPSetMulticastInterface", luv_udp_set_multicast_interface},
  {"UDPSetBroadcast", luv_udp_set_broadcast},
  {"UDPSetTTL", luv_udp_set_ttl},
  {"UDPSend", luv_udp_send},
  {"UDPTrySend", luv_udp_try_send},
  {"UDPRecvStart", luv_udp_recv_start},
  {"UDPRecvStop", luv_udp_recv_stop},
#if LUV_UV_VERSION_GEQ(1, 27, 0)
  {"UDPConnect", luv_udp_connect},
  {"UDPGetPeerName", luv_udp_getpeername},
#endif

#if 0
  // fs_event.c
  {"NewFSEvent", luv_new_fs_event},
  {"FSEventStart", luv_fs_event_start},
  {"FSEventStop", luv_fs_event_stop},
  {"FSEventGetPath", luv_fs_event_getpath},

  // fs_poll.c
  {"NewFSPoll", luv_new_fs_poll},
  {"FSPollStart", luv_fs_poll_start},
  {"FSPollStop", luv_fs_poll_stop},
  {"FSPollGetPath", luv_fs_poll_getpath},

  // fs.c
  {"FSClose", luv_fs_close},
  {"FSOpen", luv_fs_open},
  {"FSRead", luv_fs_read},
  {"FSUnlink", luv_fs_unlink},
  {"FSWrite", luv_fs_write},
  {"FSMkdir", luv_fs_mkdir},
  {"FSMkdtemp", luv_fs_mkdtemp},
#if LUV_UV_VERSION_GEQ(1, 34, 0)
  {"FSMkstemp", luv_fs_mkstemp},
#endif
  {"FSRmDir", luv_fs_rmdir},
  {"FSScanDir", luv_fs_scandir},
  {"FSScanDirNext", luv_fs_scandir_next},
  {"FSStat", luv_fs_stat},
  {"FSFstat", luv_fs_fstat},
  {"FSLstat", luv_fs_lstat},
  {"DSReName", luv_fs_rename},
  {"FSFsync", luv_fs_fsync},
  {"FSFdataSync", luv_fs_fdatasync},
  {"FSFtruncate", luv_fs_ftruncate},
  {"FSSendFile", luv_fs_sendfile},
  {"FSAccess", luv_fs_access},
  {"FSChMod", luv_fs_chmod},
  {"FSFchMod", luv_fs_fchmod},
  {"FSUtime", luv_fs_utime},
  {"FSFutime", luv_fs_futime},
#if LUV_UV_VERSION_GEQ(1, 36, 0)
  {"fs_lutime", luv_fs_lutime},
#endif
  {"FSLink", luv_fs_link},
  {"FSSymLink", luv_fs_symlink},
  {"FSReadLink", luv_fs_readlink},
#if LUV_UV_VERSION_GEQ(1, 8, 0)
  {"FSRealPath", luv_fs_realpath},
#endif
  {"FSChown", luv_fs_chown},
  {"FSFchown", luv_fs_fchown},
#if LUV_UV_VERSION_GEQ(1, 21, 0)
  {"FSLchown", luv_fs_lchown},
#endif
#if LUV_UV_VERSION_GEQ(1, 14, 0)
  {"FSCopyFile", luv_fs_copyfile },
#endif
#if LUV_UV_VERSION_GEQ(1, 28, 0)
  {"FSOpenDir", luv_fs_opendir},
  {"FSReadDir", luv_fs_readdir},
  {"FSCloseDir", luv_fs_closedir},
#endif
#if LUV_UV_VERSION_GEQ(1, 31, 0)
  {"FSStatFS", luv_fs_statfs},
#endif
#endif

  // dns.c
  {"GetAddrInfo", luv_getaddrinfo},
  {"GetNameInfo", luv_getnameinfo},

  // misc.c
  {"ChDir", luv_chdir},
#if LUV_UV_VERSION_GEQ(1, 9, 0)
  {"OSHomeDir", luv_os_homedir},
  {"os_tmpdir", luv_os_tmpdir},
  {"os_get_passwd", luv_os_get_passwd},
#endif
#if LUV_UV_VERSION_GEQ(1, 44, 0)
  {"available_parallelism", luv_available_parallelism},
#endif
  {"cpu_info", luv_cpu_info},
  {"cwd", luv_cwd},
  {"exepath", luv_exepath},
  {"get_process_title", luv_get_process_title},
#if LUV_UV_VERSION_GEQ(1, 29, 0)
  {"get_constrained_memory", luv_get_constrained_memory},
#endif
  {"get_total_memory", luv_get_total_memory},
  {"get_free_memory", luv_get_free_memory},
  {"getpid", luv_getpid},
#ifndef _WIN32
  {"getuid", luv_getuid},
  {"setuid", luv_setuid},
  {"getgid", luv_getgid},
  {"setgid", luv_setgid},
#endif
  {"getrusage", luv_getrusage},
  {"guess_handle", luv_guess_handle},
  {"hrtime", luv_hrtime},
  {"interface_addresses", luv_interface_addresses},
  {"loadavg", luv_loadavg},
  {"resident_set_memory", luv_resident_set_memory},
  {"set_process_title", luv_set_process_title},
  {"uptime", luv_uptime},
  {"version", luv_version},
  {"version_string", luv_version_string},
#ifndef _WIN32
#if LUV_UV_VERSION_GEQ(1, 8, 0)
  {"print_all_handles", luv_print_all_handles},
  {"print_active_handles", luv_print_active_handles},
#endif
#endif
#if LUV_UV_VERSION_GEQ(1, 12, 0)
  {"os_getenv", luv_os_getenv},
  {"os_setenv", luv_os_setenv},
  {"os_unsetenv", luv_os_unsetenv},
  {"os_gethostname", luv_os_gethostname},
#endif
#if LUV_UV_VERSION_GEQ(1, 16, 0)
  {"if_indextoname", luv_if_indextoname},
  {"if_indextoiid", luv_if_indextoiid},
  {"os_getppid", luv_os_getppid },
#endif
#if LUV_UV_VERSION_GEQ(1, 18, 0)
  {"os_getpid", luv_os_getpid},
#endif
#if LUV_UV_VERSION_GEQ(1, 23, 0)
  {"os_getpriority", luv_os_getpriority},
  {"os_setpriority", luv_os_setpriority},
#endif
#if LUV_UV_VERSION_GEQ(1, 25, 0)
  {"os_uname", luv_os_uname},
#endif
#if LUV_UV_VERSION_GEQ(1, 28, 0)
  {"gettimeofday", luv_gettimeofday},
#endif
#if LUV_UV_VERSION_GEQ(1, 31, 0)
  {"os_environ", luv_os_environ},
#endif
#if LUV_UV_VERSION_GEQ(1, 33, 0)
  {"Random", luv_random},
#endif
  {"Sleep", luv_sleep},

  // thread.c
  {"NewThread", luv_new_thread},
  {"ThreadEqual", luv_thread_equal},
  {"ThreadSelf", luv_thread_self},
  {"ThreadJoin", luv_thread_join},

  // work.c
  {"NewWork", luv_new_work},
  {"QueueWork", luv_queue_work},

  // util.c
#if LUV_UV_VERSION_GEQ(1, 10, 0)
  {"translate_sys_error", luv_translate_sys_error},
#endif

  // metrics.c
#if LUV_UV_VERSION_GEQ(1, 39, 0)
  {"metrics_idle_time", luv_metrics_idle_time},
#endif

  {NULL, NULL}
};

static const luaL_Reg luv_handle_methods[] = {
  // handle.c
  {"is_active", luv_is_active},
  {"is_closing", luv_is_closing},
  {"close", luv_close},
  {"ref", luv_ref},
  {"unref", luv_unref},
  {"has_ref", luv_has_ref},
  {"send_buffer_size", luv_send_buffer_size},
  {"recv_buffer_size", luv_recv_buffer_size},
  {"fileno", luv_fileno},
#if LUV_UV_VERSION_GEQ(1, 19, 0)
  {"get_type", luv_handle_get_type},
#endif
  {NULL, NULL}
};

static const luaL_Reg luv_async_methods[] = {
  {"send", luv_async_send},
  {NULL, NULL}
};

static const luaL_Reg luv_check_methods[] = {
  {"start", luv_check_start},
  {"stop", luv_check_stop},
  {NULL, NULL}
};

static const luaL_Reg luv_fs_event_methods[] = {
  {"start", luv_fs_event_start},
  {"stop", luv_fs_event_stop},
  {"getpath", luv_fs_event_getpath},
  {NULL, NULL}
};

static const luaL_Reg luv_fs_poll_methods[] = {
  {"start", luv_fs_poll_start},
  {"stop", luv_fs_poll_stop},
  {"getpath", luv_fs_poll_getpath},
  {NULL, NULL}
};

static const luaL_Reg luv_idle_methods[] = {
  {"start", luv_idle_start},
  {"stop", luv_idle_stop},
  {NULL, NULL}
};

static const luaL_Reg luv_stream_methods[] = {
  {"shutdown", luv_shutdown},
  {"listen", luv_listen},
  {"accept", luv_accept},
  {"read_start", luv_read_start},
  {"read_stop", luv_read_stop},
  {"write", luv_write},
  {"write2", luv_write2},
  {"try_write", luv_try_write},
#if LUV_UV_VERSION_GEQ(1, 42, 0)
  {"try_write2", luv_try_write2},
#endif
  {"is_readable", luv_is_readable},
  {"is_writable", luv_is_writable},
  {"set_blocking", luv_stream_set_blocking},
#if LUV_UV_VERSION_GEQ(1, 19, 0)
  {"get_write_queue_size", luv_stream_get_write_queue_size},
#endif
  {NULL, NULL}
};

static const luaL_Reg luv_pipe_methods[] = {
  {"open", luv_pipe_open},
  {"bind", luv_pipe_bind},
#if LUV_UV_VERSION_GEQ(1, 16, 0)
  {"chmod", luv_pipe_chmod},
#endif
  {"connect", luv_pipe_connect},
  {"getsockname", luv_pipe_getsockname},
#if LUV_UV_VERSION_GEQ(1, 3, 0)
  {"getpeername", luv_pipe_getpeername},
#endif
  {"pending_instances", luv_pipe_pending_instances},
  {"pending_count", luv_pipe_pending_count},
  {"pending_type", luv_pipe_pending_type},
  {NULL, NULL}
};

static const luaL_Reg luv_poll_methods[] = {
  {"start", luv_poll_start},
  {"stop", luv_poll_stop},
  {NULL, NULL}
};

static const luaL_Reg luv_prepare_methods[] = {
  {"start", luv_prepare_start},
  {"stop", luv_prepare_stop},
  {NULL, NULL}
};

static const luaL_Reg luv_process_methods[] = {
  {"kill", luv_process_kill},
#if LUV_UV_VERSION_GEQ(1, 19, 0)
  {"get_pid", luv_process_get_pid},
#endif
  {NULL, NULL}
};

static const luaL_Reg luv_tcp_methods[] = {
  {"Open", luv_tcp_open},
  {"Nodelay", luv_tcp_nodelay},
  {"KeepAlive", luv_tcp_keepalive},
  {"SimultaneousAccepts", luv_tcp_simultaneous_accepts},
  {"Bind", luv_tcp_bind},
  {"GetPeerName", luv_tcp_getpeername},
  {"GetSockName", luv_tcp_getsockname},
  {"Connect", luv_tcp_connect},
  {"WriteQueueSize", luv_write_queue_size},
#if LUV_UV_VERSION_GEQ(1, 32, 0)
  {"CloseReset", luv_tcp_close_reset},
#endif
  {NULL, NULL}
};

static const luaL_Reg luv_timer_methods[] = {
  {"start", luv_timer_start},
  {"stop", luv_timer_stop},
  {"again", luv_timer_again},
  {"set_repeat", luv_timer_set_repeat},
  {"get_repeat", luv_timer_get_repeat},
#if LUV_UV_VERSION_GEQ(1, 40, 0)
  {"get_due_in", luv_timer_get_due_in},
#endif
  {NULL, NULL}
};

static const luaL_Reg luv_tty_methods[] = {
  {"set_mode", luv_tty_set_mode},
  {"get_winsize", luv_tty_get_winsize},
  {NULL, NULL}
};

static const luaL_Reg luv_udp_methods[] = {
  {"get_send_queue_size", luv_udp_get_send_queue_size},
  {"get_send_queue_count", luv_udp_get_send_queue_count},
  {"open", luv_udp_open},
  {"bind", luv_udp_bind},
  {"getsockname", luv_udp_getsockname},
  {"set_membership", luv_udp_set_membership},
#if LUV_UV_VERSION_GEQ(1, 32, 0)
  {"set_source_membership", luv_udp_set_source_membership},
#endif
  {"set_multicast_loop", luv_udp_set_multicast_loop},
  {"set_multicast_ttl", luv_udp_set_multicast_ttl},
  {"set_multicast_interface", luv_udp_set_multicast_interface},
  {"set_broadcast", luv_udp_set_broadcast},
  {"set_ttl", luv_udp_set_ttl},
  {"send", luv_udp_send},
  {"try_send", luv_udp_try_send},
  {"recv_start", luv_udp_recv_start},
  {"recv_stop", luv_udp_recv_stop},
#if LUV_UV_VERSION_GEQ(1, 27, 0)
  {"connect", luv_udp_connect},
  {"getpeername", luv_udp_getpeername},
#endif
  {NULL, NULL}
};

static const luaL_Reg luv_signal_methods[] = {
  {"start", luv_signal_start},
  {"stop", luv_signal_stop},
  {NULL, NULL}
};

#if LUV_UV_VERSION_GEQ(1, 28, 0)
static const luaL_Reg luv_dir_methods[] = {
  {"readdir", luv_fs_readdir},
  {"closedir", luv_fs_closedir},
  {NULL, NULL}
};

static void luv_dir_init(lua_State* L) {
  luaL_newmetatable(L, "uv_dir");
  lua_pushcfunction(L, luv_fs_dir_tostring);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, luv_fs_dir_gc);
  lua_setfield(L, -2, "__gc");
  luaL_newlib(L, luv_dir_methods);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}
#endif

static void luv_handle_init(lua_State* L) {

  lua_newtable(L);
#define XX(uc, lc)                             \
    luaL_newmetatable (L, "uv_"#lc);           \
    lua_pushcfunction(L, luv_handle_tostring); \
    lua_setfield(L, -2, "__tostring");         \
    lua_pushcfunction(L, luv_handle_gc);       \
    lua_setfield(L, -2, "__gc");               \
    luaL_newlib(L, luv_##lc##_methods);        \
    luaL_setfuncs(L, luv_handle_methods, 0);   \
    lua_setfield(L, -2, "__index");            \
    lua_pushboolean(L, 1);                     \
    lua_rawset(L, -3);

  UV_HANDLE_TYPE_MAP(XX)
#undef XX
  lua_setfield(L, LUA_REGISTRYINDEX, "uv_handle");

  lua_newtable(L);

  luaL_getmetatable(L, "uv_pipe");
  lua_getfield(L, -1, "__index");
  luaL_setfuncs(L, luv_stream_methods, 0);
  lua_pop(L, 1);
  lua_pushboolean(L, 1);
  lua_rawset(L, -3);

  luaL_getmetatable(L, "uv_tcp");
  lua_getfield(L, -1, "__index");
  luaL_setfuncs(L, luv_stream_methods, 0);
  lua_pop(L, 1);
  lua_pushboolean(L, 1);
  lua_rawset(L, -3);

  luaL_getmetatable(L, "uv_tty");
  lua_getfield(L, -1, "__index");
  luaL_setfuncs(L, luv_stream_methods, 0);
  lua_pop(L, 1);
  lua_pushboolean(L, 1);
  lua_rawset(L, -3);

  lua_setfield(L, LUA_REGISTRYINDEX, "uv_stream");
}

static const luaL_Reg luv_req_methods[] = {
  // req.c
  {"cancel", luv_cancel},
#if LUV_UV_VERSION_GEQ(1, 19, 0)
  {"get_type", luv_req_get_type},
#endif
  {NULL, NULL}
};

static void luv_req_init(lua_State* L) {
  luaL_newmetatable(L, "uv_req");
  lua_pushcfunction(L, luv_req_tostring);
  lua_setfield(L, -2, "__tostring");
  luaL_newlib(L, luv_req_methods);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);

  // Only used for things that need to be garbage collected
  // (e.g. the req when using uv_fs_scandir)
  luaL_newmetatable(L, "uv_fs");
  lua_pushcfunction(L, luv_req_tostring);
  lua_setfield(L, -2, "__tostring");
  luaL_newlib(L, luv_req_methods);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, luv_fs_gc);
  lua_setfield(L, -2, "__gc");
  lua_pop(L, 1);
}

// Call lua function, will pop nargs values from top of vm stack and push some
// values according to nresults. When error occurs, it will print error message
// to stderr, and memory allocation error will cause exit.
LUALIB_API int luv_cfpcall(lua_State* L, int nargs, int nresult, int flags) {
  int ret, top, errfunc;

  top  = lua_gettop(L);
  // Get the traceback function in case of error
  if ((flags & (LUVF_CALLBACK_NOTRACEBACK|LUVF_CALLBACK_NOERRMSG) ) == 0)
  {
    lua_pushcfunction(L, luv_traceback);
    errfunc = lua_gettop(L);
    // And insert it before the function and args
    lua_insert(L, -2 - nargs);
    errfunc -= (nargs+1);
  }else
    errfunc = 0;

  ret = lua_pcall(L, nargs, nresult, errfunc);
  switch (ret) {
  case LUA_OK:
    break;
  case LUA_ERRMEM:
    if ((flags & LUVF_CALLBACK_NOERRMSG) == 0)
      fprintf(stderr, "System Error: %s\n", lua_tostring(L, -1));
    if ((flags & LUVF_CALLBACK_NOEXIT) == 0)
      exit(-1);
    lua_pop(L, 1);
    ret = -ret;
    break;
  case LUA_ERRRUN:
  case LUA_ERRERR:
  default:
    if ((flags & LUVF_CALLBACK_NOERRMSG) == 0)
      fprintf(stderr, "Uncaught Error: %s\n", lua_tostring(L, -1));
    if ((flags & LUVF_CALLBACK_NOEXIT) == 0)
      exit(-1);
    lua_pop(L, 1);
    ret = -ret;
    break;
  }
  if ((flags & (LUVF_CALLBACK_NOTRACEBACK|LUVF_CALLBACK_NOERRMSG) ) == 0)
  {
    lua_remove(L, errfunc);
  }
  if (ret == LUA_OK) {
    if(nresult == LUA_MULTRET)
      nresult = lua_gettop(L) - top + nargs + 1;
    return nresult;
  }
  return ret;
}

// Call lua c function in protected mode. When error occurs, it will print
// error message to stderr, and memory allocation error will cause exit.
LUALIB_API int luv_cfcpcall(lua_State* L, lua_CFunction func, void * ud, int flags) {
  lua_pushcfunction(L, func);
  lua_pushlightuserdata(L, ud);
  int ret = luv_cfpcall(L, 1, 0, flags);
  return ret;
}

// TODO: see if we can avoid using a string key for this to increase performance
static const char* luv_ctx_key = "luv_context";

// Please look at luv_ctx_t in luv.h
LUALIB_API luv_ctx_t* luv_context(lua_State* L) {
  luv_ctx_t* ctx;
  lua_pushstring(L, luv_ctx_key);
  lua_rawget(L, LUA_REGISTRYINDEX);
  if (lua_isnil(L, -1)) {
    // create it if not exist in registry
    lua_pushstring(L, luv_ctx_key);
    ctx = (luv_ctx_t*)lua_newuserdata(L, sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));
    lua_rawset(L, LUA_REGISTRYINDEX);
  } else {
    ctx = (luv_ctx_t*)lua_touserdata(L, -1);
  }
  lua_pop(L, 1);
  return ctx;
}

LUALIB_API lua_State* luv_state(lua_State* L) {
  return luv_context(L)->L;
}

LUALIB_API uv_loop_t* luv_loop(lua_State* L) {
  return luv_context(L)->loop;
}

// Set an external loop, before luaopen_luv
LUALIB_API void luv_set_loop(lua_State* L, uv_loop_t* loop) {
  luv_ctx_t* ctx = luv_context(L);

  ctx->loop = loop;
  ctx->L = L;
  ctx->mode = -1;
}

// Set an external event callback routine, before luaopen_luv
LUALIB_API void luv_set_callback(lua_State* L, luv_CFpcall pcall) {
  luv_ctx_t* ctx = luv_context(L);
  ctx->cb_pcall = pcall;
}

// Set an external thread routine, before luaopen_luv
LUALIB_API void luv_set_thread(lua_State* L, luv_CFpcall pcall) {
  luv_ctx_t* ctx = luv_context(L);
  ctx->thrd_pcall = pcall;
}

// Set an external c thread routine, before luaopen_luv
LUALIB_API void luv_set_cthread(lua_State* L, luv_CFcpcall cpcall) {
  luv_ctx_t* ctx = luv_context(L);
  ctx->thrd_cpcall = cpcall;
}

static void walk_cb(uv_handle_t *handle, void *arg)
{
  (void)arg;
  if (!uv_is_closing(handle)) {
    uv_close(handle, luv_close_cb);
  }
}

static int loop_gc(lua_State *L) {
  luv_ctx_t *ctx = luv_context(L);
  uv_loop_t* loop = ctx->loop;
  if (loop==NULL)
    return 0;
  // Call uv_close on every active handle
  uv_walk(loop, walk_cb, NULL);
  // Run the event loop until all handles are successfully closed
  while (uv_loop_close(loop)) {
    uv_run(loop, UV_RUN_DEFAULT);
  }
  return 0;
}

LUALIB_API int luaopen_luv (lua_State* L) {
  luv_ctx_t* ctx = luv_context(L);

  luaL_newlib(L, luv_functions);

  // loop is NULL, luv need to create an inner loop
  if (ctx->loop==NULL) {
    int ret;
    uv_loop_t* loop;

    // Setup the uv_loop meta table for a proper __gc
    luaL_newmetatable(L, "uv_loop.meta");
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, loop_gc);
    lua_settable(L, -3);
    lua_pop(L, 1);

    lua_pushstring(L, "_loop");
    loop = (uv_loop_t*)lua_newuserdata(L, sizeof(*loop));
    // setup the userdata's metatable for __gc
    luaL_getmetatable(L, "uv_loop.meta");
    lua_setmetatable(L, -2);
    // create a ref to loop, avoid __gc early
    // this puts the loop userdata into the _loop key
    // in the returned luv table
    lua_rawset(L, -3);

    ctx->loop = loop;
    ctx->L = L;
    ctx->mode = -1;

    ret = uv_loop_init(loop);
    if (ret < 0) {
      return luaL_error(L, "%s: %s\n", uv_err_name(ret), uv_strerror(ret));
    }

    /* do cleanup in main thread */
    lua_getglobal(L, "_THREAD");
    if (lua_isnil(L, -1))
      atexit(luv_work_cleanup);
    lua_pop(L, 1);
  }
  // pcall is NULL, luv use default callback routine
  if (ctx->cb_pcall==NULL) {
    ctx->cb_pcall = luv_cfpcall;
  }

  // pcall is NULL, luv use default thread routine
  if (ctx->thrd_pcall==NULL) {
    ctx->thrd_pcall = luv_cfpcall;
  }

  if (ctx->thrd_cpcall==NULL) {
    ctx->thrd_cpcall = luv_cfcpcall;
  }

  luv_req_init(L);
  luv_handle_init(L);
#if LUV_UV_VERSION_GEQ(1, 28, 0)
  luv_dir_init(L);
#endif
  luv_thread_init(L);
  luv_work_init(L);

  luv_constants(L);
  lua_setfield(L, -2, "constants");

#define XX(code, _)                 \
  lua_pushliteral(L, #code);        \
  lua_pushinteger(L, UV_ ## code);  \
  lua_rawset(L, -3);

  lua_newtable(L);
  UV_ERRNO_MAP(XX)
  lua_setfield(L, -2, "errno");

#undef XX

  return 1;
}
