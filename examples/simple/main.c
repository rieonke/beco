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

#include "beco.h"
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <stdbool.h>

struct BecoObject *STR(char *str) {
  struct BecoObject *obj = BecoObjectNew();
  obj->type = BECO_VALUE_TYPE_STR;
  obj->via.str = strdup(str);
  return obj;
}

struct BecoObject *MAP(struct BecoMap *map) {
  struct BecoObject *obj = BecoObjectNew();
  obj->type = BECO_VALUE_TYPE_MAP;
  obj->via.map = map;
  return obj;
}

void sig_handler(int sig);
BecoError hello_handler(struct BecoContext *ctx, struct BecoRequest *req, void *user_data);

volatile bool g_con_exit; // global exit condition

int main(int argc, char **argv) {
  struct BecoContext ctx;
  BecoError err;

  signal(SIGTERM, sig_handler);
  signal(SIGINT, sig_handler);

  BecoContextInit(&ctx);
  BecoRegisterCommand(&ctx, "hello", hello_handler, NULL);

  err = BecoMainLoop(&ctx, &g_con_exit, false);
  if (err) {
    BecoLog(&ctx, "Exit with ret: %d\n", err);
  }

  BecoContextDestroy(&ctx);
  return 0;
}

void sig_handler(int sig) {
  g_con_exit = true;
}

BecoError hello_handler(struct BecoContext *ctx, struct BecoRequest *req, void *user_data) {
  struct BecoObject *obj;
  struct BecoMap *map = NULL;

  map = BecoMapNew();
  BecoMapPut(map, "hello", STR("you"));

  obj = MAP(map);

  BecoSendResponse(ctx, obj);
  BecoObjectFree(obj);

  return BECO_ERR_OK;
}

