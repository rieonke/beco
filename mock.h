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

#ifndef BECO_MOCK_H
#define BECO_MOCK_H

#include "beco.h"

struct BecoMockContext;
struct BecoMockPipe;

struct BecoMockPipe {
#ifdef _WIN32
  void *in_rd;
  void *in_wr;
  void *out_rd;
  void *out_wr;
#else
  int in_rd;
  int in_wr;
  int out_rd;
  int out_wr;
#endif
};

struct BecoMockContext {
  char *exec_path;
  FILE *in;
  FILE *out;
  struct BecoMockPipe pipe;
  struct BecoContext driver;
};

/**
 * Create mock context
 * @return mock context
 */
struct BecoMockContext *BecoMockNew();

/**
 * Initialize mock context
 * @param mock mock context
 */
void BecoMockInit(struct BecoMockContext *mock);

/**
 * Start mock
 * @param mock mock
 * @return error
 */
BecoError BecoMockStart(struct BecoMockContext *mock);

/**
 * Finish mock
 * @param mock mock
 * @return error
 */
BecoError BecoMockFinish(struct BecoMockContext *mock);

/**
 * Get BecoContext driver
 * @param mock mock
 * @return mock driver
 */
struct BecoContext *BecoMockGetDriver(struct BecoMockContext *mock);

#endif //BECO_MOCK_H
