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

#include "../beco.h"
#include "../mock.h"
#include <string.h>
#include <assert.h>

volatile bool g_con_exit = false;

struct BecoObject *STR(char *str) {
  struct BecoObject *obj = BecoObjectNew();
  obj->type = BECO_VALUE_TYPE_STR;
  obj->via.str = strdup(str);
  return obj;
}

void close_child(struct BecoContext *ctx) {
  struct BecoObject obj;
  struct BecoMap *map = NULL;
  struct BecoRequest req = {0};

  map = BecoMapNew();
  BecoMapPut(map, "command", STR("close"));

  obj.type = BECO_VALUE_TYPE_MAP;
  obj.via.map = map;

  BecoWrite(ctx, &obj);

  BecoRead(ctx, &req);

  BecoObjectDump(req.data);
  BecoMapFree(map);
  BecoRequestDestroy(&req);
}

void test_hello(struct BecoContext *ctx) {
  struct BecoObject obj;
  struct BecoMap *map = NULL;
  struct BecoRequest req = {0};

  map = BecoMapNew();
  BecoMapPut(map, "hello", STR("you"));

  obj.type = BECO_VALUE_TYPE_MAP;
  obj.via.map = map;

  BecoWrite(ctx, &obj);

  BecoRead(ctx, &req);

  BecoObjectDump(req.data);
  BecoMapFree(map);
  BecoRequestDestroy(&req);
}

void test_print(struct BecoContext *ctx) {
  struct BecoObject obj;
  struct BecoMap *map = NULL;
  struct BecoRequest req = {0};

  map = BecoMapNew();
  BecoMapPut(map, "command", STR("print"));
  BecoMapPut(map, "hello", STR("print"));

  obj.type = BECO_VALUE_TYPE_MAP;
  obj.via.map = map;

  BecoWrite(ctx, &obj);

  BecoRead(ctx, &req);

  BecoObjectDump(req.data);
  BecoMapFree(map);
  BecoRequestDestroy(&req);
}

#ifdef _WIN32
#define MOCK_TARGET_EXE "test_beco.exe"
#else
#define MOCK_TARGET_EXE "test_beco"
#endif

int main(int argc, char **argv) {

  struct BecoMockContext mock;
  struct BecoContext *driver;
  BecoError err;

  BecoMockInit(&mock);
  mock.exec_path = MOCK_TARGET_EXE;

  err = BecoMockStart(&mock);
  assert(err == BECO_ERR_OK);
  driver = BecoMockGetDriver(&mock);
  assert(driver != NULL);

  test_hello(driver);
  test_print(driver);
  close_child(driver);

  BecoMockFinish(&mock);

  return 0;
}