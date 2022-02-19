/*
 * Copyright (c) 2022 Rieon Ke <i@ry.ke>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "mock.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef _WIN32
#include <Windows.h>
#include <io.h>
typedef HANDLE IO;
#else
#include <unistd.h>
#include <spawn.h>
typedef int IO;
#endif

bool MockCreateProcess(char *exec, IO in, IO out, IO err);
bool MockCloseProcess();
bool MockCreatePipe(struct BecoMockPipe *pipe);
void MockClosePipe(struct BecoMockPipe *pipe);
bool MockConvertFileDescriptor(struct BecoMockContext *ctx);
bool MockCreateDriver(struct BecoMockContext *ctx);
void MockCloseDriver(struct BecoMockContext *ctx);

struct BecoMockContext *BecoMockNew() {
  struct BecoMockContext *mock;
  mock = malloc(sizeof(*mock));

  BecoMockInit(mock);

  return mock;
}

void BecoMockInit(struct BecoMockContext *mock) {
  if (mock == NULL) return;
  memset(mock, 0, sizeof(*mock));
}

BecoError BecoMockStart(struct BecoMockContext *mock) {
  if (mock == NULL || mock->exec_path == NULL) return BECO_ERR_NULL;

  if (!MockCreatePipe(&mock->pipe)) {
    return BECO_ERR_GENERIC;
  }
  if (!MockCreateProcess(mock->exec_path, mock->pipe.in_rd, mock->pipe.out_wr, mock->pipe.out_wr)) {
    return BECO_ERR_GENERIC;
  }
  if (!MockConvertFileDescriptor(mock)) {
    return BECO_ERR_GENERIC;
  }
  if (!MockCreateDriver(mock)) {
    return BECO_ERR_GENERIC;
  }
  return BECO_ERR_OK;
}

BecoError BecoMockFinish(struct BecoMockContext *mock) {
  MockClosePipe(&mock->pipe);
  MockCloseDriver(mock);
  return BECO_ERR_OK;
}

struct BecoContext *BecoMockGetDriver(struct BecoMockContext *mock) {
  if (mock == NULL) return NULL;
  return &mock->driver;
}

bool MockConvertFileDescriptor(struct BecoMockContext *ctx) {

#ifdef _WIN32
  int out_fd = -1, in_fd = -1;
  FILE *in = NULL, *out = NULL;

  out_fd = _open_osfhandle((intptr_t) ctx->pipe.in_wr, _O_APPEND);
  if (out_fd == -1) goto error;

  in_fd = _open_osfhandle((intptr_t) ctx->pipe.out_rd, _O_RDONLY);
  if (in_fd == -1) goto error;

  out = _fdopen(out_fd, "ab");
  if (out == NULL) goto error;

  in = _fdopen(in_fd, "rb");
  if (in == NULL) goto error;

  ctx->in = in;
  ctx->out = out;

  return true;

  error:
  if (out_fd != -1) _close(out_fd);
  if (in_fd != -1) _close(in_fd);
  if (out != NULL) fclose(out);
#else

  FILE *in = NULL, *out = NULL;

  out = fdopen(ctx->pipe.in_wr, "ab");
  if (out == NULL) goto error;

  in = fdopen(ctx->pipe.out_rd, "rb");
  if (in == NULL) goto error;

  ctx->in = in;
  ctx->out = out;

  return true;

  error:
  if (out != NULL) fclose(out);
  return false;
#endif

}

bool MockCreateProcess(char *exec, IO in, IO out, IO err) {

#ifdef _WIN32
  PROCESS_INFORMATION proc_info;
  STARTUPINFO start_info;
  bool ret = false;

  memset(&proc_info, 0, sizeof(PROCESS_INFORMATION));
  memset(&start_info, 0, sizeof(STARTUPINFO));

  start_info.cb = sizeof(STARTUPINFO);
  start_info.hStdError = err;
  start_info.hStdOutput = out;
  start_info.hStdInput = in;
  start_info.dwFlags |= STARTF_USESTDHANDLES;

  ret = CreateProcess(NULL,     // application name
                      exec,       // process executable name
                      NULL,     // security attributes
                      NULL,     // thread attributes
                      TRUE,       // handles inherited
                      0,         // creation flags
                      NULL,       // use parent's
                      NULL,    // work idr
                      &start_info, // startup info
                      &proc_info); // proc info

  if (!ret) {
    return false;
  }

  CloseHandle(proc_info.hProcess);
  CloseHandle(proc_info.hThread);
  CloseHandle(out);
  CloseHandle(in);

  return true;
#else
  pid_t pid;
  int ret = 0;
  posix_spawn_file_actions_t file_actions;

  posix_spawn_file_actions_init(&file_actions);
  posix_spawn_file_actions_adddup2(&file_actions, in, STDIN_FILENO);
  posix_spawn_file_actions_adddup2(&file_actions, out, STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&file_actions, err, STDERR_FILENO);

  ret = posix_spawn(&pid, exec, &file_actions, NULL, NULL, NULL);
  if (ret != 0) {
    goto error;
  }

  if (file_actions != NULL) {
//    ret = posix_spawn_file_actions_destroy(file_actions);
//    if (ret != 0)
//      goto error;
  }

  error:
  if (ret != 0) {
    fprintf(stderr, "Failed to create sub process\n");
    return false;
  }
  return true;
#endif
}

bool MockCreatePipe(struct BecoMockPipe *p) {

#ifdef _WIN32
  SECURITY_ATTRIBUTES sec_attr;
  HANDLE in_rd = NULL;
  HANDLE in_wr = NULL;
  HANDLE out_rd = NULL;
  HANDLE out_wr = NULL;

  sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sec_attr.bInheritHandle = TRUE;
  sec_attr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&out_rd, &out_wr, &sec_attr, 0)) {
    fprintf(stderr, "CREATE PIPE FAILED\n");
    goto error;
  }
  if (!SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0)) {
    fprintf(stderr, "CREATE PIPE FAILED\n");
    goto error;
  }

  if (!CreatePipe(&in_rd, &in_wr, &sec_attr, 0)) {
    fprintf(stderr, "CREATE PIPE FAILED\n");
    goto error;
  }
  if (!SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0)) {
    fprintf(stderr, "CREATE PIPE FAILED\n");
    goto error;
  }

  p->in_rd = in_rd;
  p->in_wr = in_wr;
  p->out_rd = out_rd;
  p->out_wr = out_wr;

  return true;

  error:
  CloseHandle(in_rd);
  CloseHandle(in_wr);
  CloseHandle(out_rd);
  CloseHandle(out_wr);
  return false;

#else
  int in_fd[2];
  int out_fd[2];
  int ret;

  if ((ret = pipe(in_fd)) != 0) {
    goto error;
  }

  if ((ret = pipe(out_fd)) != 0) {
    goto error;
  }

  p->in_rd = in_fd[0];
  p->in_wr = in_fd[1];
  p->out_rd = out_fd[0];
  p->out_wr = out_fd[1];

  error:
  return ret == 0;
#endif
}

void MockClosePipe(struct BecoMockPipe *pipe) {
  if (pipe == NULL) return;

  // CloseHandle(pipe->in_rd);
  // CloseHandle(pipe->in_wr);
  // CloseHandle(pipe->in_rd);
  // CloseHandle(pipe->in_wr);
}

bool MockCreateDriver(struct BecoMockContext *ctx) {
  if (ctx == NULL) return false;

  BecoContextInit(&ctx->driver);
  BecoSetIn(&ctx->driver, ctx->in);
  BecoSetOut(&ctx->driver, ctx->out);

  return true;
}

void MockCloseDriver(struct BecoMockContext *ctx) {
  if (ctx == NULL) return;
  BecoContextDestroy(&ctx->driver);
}
