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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdarg.h>
#include <signal.h>

#include "3rd/yyjson.h"
#include "3rd/uthash.h"

#define SIZE_1M 0x100000

#ifdef _WIN32
#define strdup(x) _strdup(x)
#define SIZE_FMT "%Iu"
#else
#define SIZE_FMT "%zu"
#endif

struct BecoMapEntry;
struct BecoRequestHandler;

struct BecoRequestHandler {
  char *cmd;
  BecoRequestHandlerFunc handler;
  void *user_data;
  UT_hash_handle hh;
};

struct BecoMap {
  struct BecoMapEntry *entries;
};

struct BecoMapEntry {
  char *key;
  struct BecoKV *kv;
  UT_hash_handle hh;
};

struct BecoRequestHandler *CreateHandler(const char *cmd, BecoRequestHandlerFunc handler, void *user_data);
BecoError InvokeHandler(struct BecoContext *ctx, struct BecoRequestHandler *entry, struct BecoRequest *req);
void FreeHandler(struct BecoRequestHandler *handler);

BecoError JsonToObj(yyjson_val *root, struct BecoObject *out);
BecoError JsonToMap(yyjson_val *root, struct BecoMap *out);
BecoError JsonToArr(yyjson_val *root, struct BecoArray *out);
BecoError ObjToJson(struct BecoObject *obj, yyjson_mut_doc *doc, yyjson_mut_val **out);
BecoError MapToJson(struct BecoMap *obj, yyjson_mut_doc *doc, yyjson_mut_val **out);
BecoError ArrToJson(struct BecoArray *obj, yyjson_mut_doc *doc, yyjson_mut_val **out);

void BecoLog(struct BecoContext *ctx, const char *fmt, ...) {
  if (ctx == NULL || ctx->log == NULL) return;
  //@formatter:off
  va_list args;
  va_start(args, fmt);
  vfprintf(ctx->log, fmt, args);
  va_end(args);
  fprintf(ctx->log,"\n");
  fflush(ctx->log);
  //@formatter:on
}

struct BecoContext *BecoContextNew() {
  struct BecoContext *ctx = NULL;
  ctx = malloc(sizeof(*ctx));
  BecoContextInit(ctx);
  return ctx;
}

struct BecoContext *BecoContextNewWithConf(struct BecoConf *conf) {
  struct BecoContext *ctx;
  struct BecoRequestHandler *null_handler;
  struct BecoRequestHandler *default_handler;

  ctx = BecoContextNew();
  BecoContextInit(ctx);

  if (conf->log_file != NULL) {
    BecoSetLog(ctx, conf->log_file);
  }

  if (!conf->use_stdio) {
    if (conf->in == NULL || conf->out == NULL) {
      BecoLog(ctx, "in and out must not be null!");
      goto error;
    }
    ctx->in = conf->in;
    ctx->out = conf->out;
  }

  if (conf->default_cmd_handler != NULL) {
    default_handler = CreateHandler(NULL, conf->default_cmd_handler, conf->default_user_data);
    ctx->default_cmd_handler = default_handler;
  }

  if (conf->null_cmd_handler != NULL) {
    null_handler = CreateHandler(NULL, conf->null_cmd_handler, conf->null_user_data);
    ctx->null_cmd_handler = null_handler;
  }

  if (conf->sig_handler) {
    signal(SIGABRT, conf->sig_handler);
    signal(SIGTERM, conf->sig_handler);
    signal(SIGINT, conf->sig_handler);
    signal(SIGILL, conf->sig_handler);
  }

  return ctx;

  error:
  BecoContextFree(ctx);
  return NULL;
}

void BecoContextInit(struct BecoContext *ctx) {
  if (ctx == NULL) return;
  memset(ctx, 0, sizeof(*ctx));
  ctx->in = stdin;
  ctx->out = stdout;
  ctx->log = stderr;
}

void BecoSetLog(struct BecoContext *ctx, FILE *file) {
  if (ctx == NULL || file == NULL) return;
  ctx->log = file;
}

BecoError BecoSetIn(struct BecoContext *ctx, FILE *in) {
  if (ctx == NULL || in == NULL) return BECO_ERR_NULL;
  ctx->in = in;
  return BECO_ERR_OK;
}

BecoError BecoSetOut(struct BecoContext *ctx, FILE *out) {
  if (ctx == NULL || out == NULL) return BECO_ERR_NULL;
  ctx->out = out;
  return BECO_ERR_OK;
}

void BecoContextFree(struct BecoContext *ctx) {
  BecoContextDestroy(ctx);
  free(ctx);
}

void BecoContextDestroy(struct BecoContext *ctx) {
  if (ctx == NULL) return;
  if (ctx->in != NULL)
    fclose(ctx->in);
  if (ctx->out != NULL)
    fclose(ctx->out);
  if (ctx->handler_entries != NULL) {
    struct BecoRequestHandler *entry, *temp;
    HASH_ITER(hh, ctx->handler_entries, entry, temp) {
      HASH_DEL(ctx->handler_entries, entry);
      FreeHandler(entry);
    }
    free(ctx->handler_entries);
  }
  FreeHandler(ctx->null_cmd_handler);
  FreeHandler(ctx->default_cmd_handler);
}

void BecoSetNullCmdHandler(struct BecoContext *ctx, BecoRequestHandlerFunc handler, void *user_data) {
  if (ctx == NULL) return;
  struct BecoRequestHandler *entry = NULL;

  entry = CreateHandler(NULL, handler, user_data);
  ctx->null_cmd_handler = entry;
}

void BecoSetDefaultCmdHandler(struct BecoContext *ctx, BecoRequestHandlerFunc handler, void *user_data) {
  if (ctx == NULL) return;
  struct BecoRequestHandler *entry = NULL;

  entry = CreateHandler(NULL, handler, user_data);
  ctx->default_cmd_handler = entry;
}

BecoError BecoMainLoop(struct BecoContext *ctx, const volatile bool *exit, bool exit_on_fail) {
  if (ctx == NULL) return BECO_ERR_NULL;

  BecoError err = BECO_ERR_OK;
  while (!*exit) {
    err = BecoNext(ctx);
    if (err != BECO_ERR_OK && exit_on_fail) break;
  }
  return BECO_ERR_OK;
}

BecoError BecoNext(struct BecoContext *ctx) {
  if (ctx == NULL) return BECO_ERR_NULL;

  struct BecoRequest req;
  BecoError err = BECO_ERR_OK;
  struct BecoRequestHandler *handler;

  BecoRequestInit(&req);

  err = BecoRead(ctx, &req);
  if (err != BECO_ERR_OK) {
    return err;
  }

  if (req.cmd == NULL && ctx->null_cmd_handler) {
    InvokeHandler(ctx, ctx->null_cmd_handler, &req);
    goto exit;
  }

  handler = BecoFindRequestHandler(ctx, req.cmd);
  if (handler == NULL && ctx->default_cmd_handler == NULL) {
    err = BECO_ERR_NO_IMPL;
    goto exit;
  }

  handler = handler == NULL ? ctx->default_cmd_handler : handler;
  InvokeHandler(ctx, handler, &req);

  exit:
  BecoRequestDestroy(&req);
  return err;
}

BecoError BecoSendResponse(struct BecoContext *ctx, struct BecoObject *res) {
  return BecoWrite(ctx, res);
}

BecoError BecoRead(struct BecoContext *ctx, struct BecoRequest *req) {
  if (ctx == NULL || req == NULL) return BECO_ERR_NULL;

  BecoError err;
  char *data = NULL;
  size_t len = 0;

  struct BecoObject *obj = NULL;
  const char *cmd_name = NULL;
  char *fmt_json = NULL;
  size_t fmt_json_len = 0;
  yyjson_doc *doc = NULL;
  yyjson_val *root = NULL;
  yyjson_val *cmd_obj = NULL;

  if ((err = BecoReadRaw(ctx->in, &data, &len)) != BECO_ERR_OK) {
    goto error;
  }

  // parse content
  doc = yyjson_read(data, len, YYJSON_READ_NOFLAG);
  if (doc == NULL) {
    err = BECO_ERR_INVALID_JSON;
    goto error;
  }

  root = yyjson_doc_get_root(doc);
  if (root == NULL) {
    err = BECO_ERR_INVALID_JSON;
    goto error;
  }

  fmt_json = yyjson_write(doc, YYJSON_WRITE_PRETTY, &fmt_json_len);
  BecoLog(ctx, "Received input: %s\n", fmt_json);

  // optional command tag
  cmd_obj = yyjson_obj_get(root, "command");
  if (cmd_obj != NULL) {
    cmd_name = yyjson_get_str(cmd_obj);
  }

  obj = BecoObjectNew();
  // read json to key-value obj
  if (JsonToObj(root, obj) != BECO_ERR_OK) {
    goto error;
  }

  if (ctx->log)
    BecoObjectDumpF(obj, 2, ctx->log);

  req->data = obj;
  if (cmd_name != NULL)
    req->cmd = strdup(cmd_name);

  error:
  if (doc != NULL) yyjson_doc_free(doc);
  if (fmt_json != NULL) free(fmt_json);
  return err;
}

BecoError BecoWrite(struct BecoContext *ctx, struct BecoObject *res) {
  if (ctx == NULL || res == NULL || ctx->out == NULL) return BECO_ERR_NULL;

  char *out = NULL;
  size_t len = 0;
  BecoError err = BECO_ERR_OK;

  if ((err = BecoObjectDumpJson(res, &out, &len)) != BECO_ERR_OK) {
    goto error;
  }

  BecoLog(ctx, "Write Response: (%d) %s\n", len, out);

  if ((err = BecoWriteRaw(ctx->out, out, len)) != BECO_ERR_OK) {
    goto error;
  }

  error:
  return err;
}

BecoError BecoReadRaw(FILE *in, char **out, size_t *olen) {
  if (in == NULL || out == NULL || olen == NULL) return BECO_ERR_NULL;

  BecoError err = BECO_ERR_OK;
  size_t read = 0;
  uint32_t size = 0;
  char *buf = NULL;

  read = fread(&size, sizeof(size), 1, in);
  if (read != 1) {
    err = BECO_ERR_IO;
    goto error;
  }

  buf = malloc(sizeof(char) * size);
  read = fread(buf, sizeof(*buf), size, in);
  if (read != size) {
    err = BECO_ERR_IO;
    goto error;
  }

  error:
  if (err != BECO_ERR_OK) {
    free(buf);
  } else {
    *out = buf;
    *olen = size;
  }
  return err;
}

BecoError BecoWriteRaw(FILE *out, const char *data, size_t dlen) {
  if (out == NULL || data == NULL) return BECO_ERR_NULL;

  BecoError err = BECO_ERR_OK;
  size_t write = 0;
  uint32_t len = 0;

  if (dlen > SIZE_1M) {
    return BECO_ERR_OVERFLOW;
  }

  len = (uint32_t) dlen;

  write = fwrite(&len, sizeof(len), 1, out);
  if (write != 1) {
    err = BECO_ERR_IO;
    goto error;
  }

  write = fwrite(data, sizeof(*data), dlen, out);
  if (write != dlen) {
    err = BECO_ERR_IO;
    goto error;
  }

  if (fflush(out) != 0) {
    err = BECO_ERR_IO;
    goto error;
  }

  error:
  return err;
}

BecoError BecoRegisterCommand(struct BecoContext *ctx,
                              const char *cmd,
                              BecoRequestHandlerFunc handler,
                              void *user_data) {
  if (ctx == NULL || handler == NULL) return BECO_ERR_NULL;

  struct BecoRequestHandler *entry = NULL;

  entry = CreateHandler(cmd, handler, user_data);
  HASH_ADD_STR(ctx->handler_entries, cmd, entry);

  return BECO_ERR_OK;
}

BecoError BecoRemoveCommand(struct BecoContext *ctx, const char *cmd) {
  if (ctx == NULL) return BECO_ERR_NULL;

  struct BecoRequestHandler *out = NULL;
  HASH_FIND_STR(ctx->handler_entries, cmd, out);
  if (out != NULL) {
    HASH_DEL(ctx->handler_entries, out);
    free(out->cmd);
    free(out);
  }
  return BECO_ERR_OK;
}

struct BecoRequestHandler *BecoFindRequestHandler(struct BecoContext *ctx, const char *cmd) {
  if (ctx == NULL || cmd == NULL) return NULL;

  struct BecoRequestHandler *out = NULL;
  HASH_FIND_STR(ctx->handler_entries, cmd, out);
  if (out != NULL) return out;
  return NULL;
}

struct BecoRequest *BecoRequestNew() {
  struct BecoRequest *request = NULL;
  request = malloc(sizeof(*request));
  memset(request, 0, sizeof(*request));
  return request;
}

const char *BecoRequestGetCommand(struct BecoRequest *request) {
  if (request == NULL) return NULL;
  return request->cmd;
}

void BecoRequestSetData(struct BecoRequest *request, struct BecoObject *obj) {
  if (request == NULL) return;
  request->data = obj;
}

void BecoRequestSetCommand(struct BecoRequest *request, const char *command) {
  if (request == NULL) return;
  request->cmd = strdup(command);
}

void BecoRequestInit(struct BecoRequest *req) {
  if (req == NULL) return;
  memset(req, 0, sizeof(*req));
}

void BecoRequestDestroy(struct BecoRequest *req) {
  if (req == NULL) return;
  free(req->cmd);
  BecoObjectFree(req->data);
}

void BecoRequestFree(struct BecoRequest *req) {
  BecoRequestDestroy(req);
  free(req);
}

struct BecoObject *BecoObjectNew() {
  struct BecoObject *obj = NULL;
  obj = malloc(sizeof(*obj));
  memset(obj, 0, sizeof(*obj));
  return obj;
}

enum BecoValueType BecoObjectGetType(struct BecoObject *obj) {
  if (obj == NULL) return BECO_VALUE_TYPE_NONE;
  return obj->type;
}

int64_t BecoObjectGetInt64(struct BecoObject *obj) {
  if (obj == NULL) return 0;
  return obj->via.i64;
}

uint64_t BecoObjectGetUInt64(struct BecoObject *obj) {
  if (obj == NULL) return 0;
  return obj->via.u64;
}

bool BecoObjectGetBool(struct BecoObject *obj) {
  if (obj == NULL) return false;
  return obj->via.bool_;
}

double BecoObjectGetFloat64(struct BecoObject *obj) {
  if (obj == NULL) return NAN;
  return obj->via.f64;
}

const char *BecoObjectGetStr(struct BecoObject *obj) {
  if (obj == NULL) return NULL;
  return obj->via.str;
}

struct BecoMap *BecoObjectGetMap(struct BecoObject *obj) {
  if (obj == NULL) return NULL;
  return obj->via.map;
}

struct BecoArray *BecoObjectGetArray(struct BecoObject *obj) {
  if (obj == NULL) return NULL;
  return obj->via.array;
}

void BecoObjectDumpF(struct BecoObject *obj, int indent, FILE *out) {
  if (obj == NULL || out == NULL) return;
  switch (obj->type) {
    case BECO_VALUE_TYPE_NONE: {
      fprintf(out, "(none)\n");
      break;
    }
    case BECO_VALUE_TYPE_BOOL: {
      fprintf(out, "%d (none)\n", obj->via.bool_);
      break;
    }
    case BECO_VALUE_TYPE_INTEGER: {
      fprintf(out, "%lld (integer)\n", obj->via.i64);
      break;
    }
    case BECO_VALUE_TYPE_POSITIVE_INTEGER: {
      fprintf(out, "%lld (+integer)\n", obj->via.u64);
      break;
    }
    case BECO_VALUE_TYPE_DOUBLE: {
      fprintf(out, "%f (double)\n", obj->via.f64);
      break;
    }
    case BECO_VALUE_TYPE_STR: {
      fprintf(out, "\"%s\" (string)\n", obj->via.str);
      break;
    }
    case BECO_VALUE_TYPE_MAP: {
      fprintf(out, "(map) {\n");
      struct BecoMapEntry *s, *tmp;
      HASH_ITER(hh, obj->via.map->entries, s, tmp) {
        fprintf(out, "%*s\"%s\": ", indent + 2, " ", s->key);
        BecoObjectDumpF(s->kv->value, indent + 2, out);
      }
      fprintf(out, "%*s}\n", indent, " ");
      break;
    }
    case BECO_VALUE_TYPE_ARRAY: {
      fprintf(out, "(array["SIZE_FMT"]) {\n", obj->via.array->size);
      size_t i;
      for (i = 0; i < obj->via.array->size; ++i) {
        fprintf(out, "%*s["SIZE_FMT"]: ", indent + 2, " ", i);
        BecoObjectDumpF(obj->via.array->ptr[i], indent + 2, out);
      }
      fprintf(out, "%*s}\n", indent, " ");
      break;
    }
  }
}

BecoError BecoObjectDumpJson(struct BecoObject *obj, char **out, size_t *olen) {
  if (obj == NULL || out == NULL || olen == NULL) return BECO_ERR_NULL;

  yyjson_mut_doc *doc = NULL;
  yyjson_mut_val *root = NULL;
  char *json_str = NULL;
  BecoError err = BECO_ERR_OK;

  doc = yyjson_mut_doc_new(NULL);

  if ((err = ObjToJson(obj, doc, &root)) != BECO_ERR_OK) {
    goto error;
  }
  yyjson_mut_doc_set_root(doc, root);

  json_str = yyjson_mut_write(doc, YYJSON_WRITE_NOFLAG, olen);
  *out = json_str;

  error:
  yyjson_mut_doc_free(doc);
  return err;
}

void BecoObjectDump(struct BecoObject *obj) {
  if (obj == NULL) return;
  BecoObjectDumpF(obj, 0, stdout);
}

struct BecoObject *BecoObjectDup(struct BecoObject *src, bool recursive) {
  if (src == NULL) return NULL;
  struct BecoObject *dst = NULL;

  dst = BecoObjectNew();
  dst->type = src->type;
  switch (src->type) {
    case BECO_VALUE_TYPE_NONE: {
      break;
    }
    case BECO_VALUE_TYPE_BOOL: {
      dst->via.bool_ = src->via.bool_;
      break;
    }
    case BECO_VALUE_TYPE_INTEGER: {
      dst->via.i64 = src->via.i64;
      break;
    }
    case BECO_VALUE_TYPE_POSITIVE_INTEGER: {
      dst->via.u64 = src->via.u64;
      break;
    }
    case BECO_VALUE_TYPE_DOUBLE: {
      dst->via.f64 = src->via.f64;
      break;
    }
    case BECO_VALUE_TYPE_STR: {
      dst->via.str = strdup(src->via.str);
      break;
    }
    case BECO_VALUE_TYPE_MAP: {
      dst->via.map = src->via.map; // todo impl recursive
      break;
    }
    case BECO_VALUE_TYPE_ARRAY: {
      dst->via.array = src->via.array; // todo impl recursive
      break;
    }
  }
  return dst;
}

void BecoObjectFree(struct BecoObject *obj) {
  if (obj == NULL) return;
  switch (obj->type) {
    case BECO_VALUE_TYPE_NONE:
    case BECO_VALUE_TYPE_BOOL:
    case BECO_VALUE_TYPE_INTEGER:
    case BECO_VALUE_TYPE_POSITIVE_INTEGER:
    case BECO_VALUE_TYPE_DOUBLE:break;
    case BECO_VALUE_TYPE_STR: {
      free(obj->via.str);
      obj->via.str = NULL;
      break;
    }
    case BECO_VALUE_TYPE_MAP: {
      BecoMapFree(obj->via.map);
      obj->via.map = NULL;
      break;
    }
    case BECO_VALUE_TYPE_ARRAY: {
      BecoArrayFree(obj->via.array);
      obj->via.array = NULL;
      break;
    }
  }
  free(obj);
}

struct BecoMap *BecoMapNew() {
  struct BecoMap *map = NULL;
  map = malloc(sizeof(*map));
  memset(map, 0, sizeof(*map));
  return map;
}

void BecoMapPut(struct BecoMap *map, const char *key, struct BecoObject *val) {
  struct BecoMapEntry *entry = NULL;
  struct BecoKV *kv = NULL;

  kv = BecoKVNew();
  kv->key = strdup(key);
  kv->value = val;

  entry = malloc(sizeof(*entry));
  entry->key = kv->key;
  entry->kv = kv;

  HASH_ADD_STR(map->entries, key, entry);
}

struct BecoObject *BecoMapGet(struct BecoMap *map, const char *key) {
  if (map == NULL) return NULL;

  struct BecoMapEntry *out = NULL;
  HASH_FIND_STR(map->entries, key, out);
  if (out == NULL || out->key == NULL) return NULL;
  return out->kv->value;
}

bool BecoMapContainsKey(struct BecoMap *map, const char *key) {
  if (map == NULL) return false;

  struct BecoMapEntry *out = NULL;
  HASH_FIND_STR(map->entries, key, out);
  if (out == NULL || out->key == NULL) return false;
  return true;
}

void BecoMapFree(struct BecoMap *map) {
  if (map == NULL) return;
  struct BecoMapEntry *entry, *temp;
  HASH_ITER(hh, map->entries, entry, temp) {
    BecoKVFree(entry->kv);
    free(entry);
  }
  free(map);
}

struct BecoArray *BecoArrayNew(size_t size) {
  struct BecoArray *arr = NULL;
  arr = malloc(sizeof(*arr));
  memset(arr, 0, sizeof(*arr));
  arr->ptr = malloc(sizeof(*arr->ptr) * size);
  arr->size = size;
  return arr;
}

size_t BecoArrayLen(struct BecoArray *array) {
  if (array == NULL) return 0;
  return array->size;
}

void BecoArrayAdd(struct BecoArray *array, size_t pos, struct BecoObject *obj) {
  if (array == NULL || array->ptr == NULL || pos >= array->size) return;
  *(array->ptr + pos) = obj;
}

struct BecoObject *BecoArrayGet(struct BecoArray *array, size_t pos) {
  if (array == NULL || array->ptr == NULL || pos >= array->size) return NULL;
  return array->ptr[pos];
}

void BecoArrayFree(struct BecoArray *array) {
  if (array == NULL) return;
  size_t i;

  if (array->ptr != NULL) {
    for (i = 0; i < array->size; ++i) {
      BecoObjectFree(array->ptr[i]);
      array->ptr[i] = NULL;
    }
  }
  free(array);
}

struct BecoKV *BecoKVNew() {
  struct BecoKV *obj = NULL;
  obj = malloc(sizeof(*obj));
  memset(obj, 0, sizeof(*obj));
  return obj;
}

void BecoKVFree(struct BecoKV *kv) {
  if (kv == NULL) return;
  free(kv->key);
  BecoObjectFree(kv->value);
}

void BecoKVSetKey(struct BecoKV *kv, const char *key) {
  if (kv == NULL) return;
  kv->key = strdup(key);
}

void BecoKVSetValue(struct BecoKV *kv, struct BecoObject *obj) {
  if (kv == NULL) return;
  kv->value = obj;
}

struct BecoRequestHandler *CreateHandler(const char *cmd, BecoRequestHandlerFunc handler, void *user_data) {
  struct BecoRequestHandler *entry = NULL;
  entry = malloc(sizeof(*entry));
  memset(entry, 0, sizeof(*entry));

  if (cmd != NULL)
    entry->cmd = strdup(cmd);

  entry->handler = handler;
  entry->user_data = user_data;
  return entry;
}

BecoError InvokeHandler(struct BecoContext *ctx, struct BecoRequestHandler *entry, struct BecoRequest *req) {
  if (ctx == NULL || entry == NULL || req == NULL) return BECO_ERR_NULL;
  if (entry->handler == NULL) return BECO_ERR_NULL;
  return entry->handler(ctx, req, entry->user_data);
}

BecoError JsonToArr(yyjson_val *root, struct BecoArray *out) {
  if (root == NULL || out == NULL) return BECO_ERR_NULL;

  if (!yyjson_is_arr(root)) {
    return BECO_ERR_INVALID_JSON;
  }

  struct BecoObject *obj = NULL;
  yyjson_val *val;
  yyjson_arr_iter iter;
  size_t len = 0;
  size_t pos = 0;

  len = yyjson_arr_size(root);
  if (len != out->size) {
    return BECO_ERR_OVERFLOW;
  }

  yyjson_arr_iter_init(root, &iter);
  while ((val = yyjson_arr_iter_next(&iter))) {
    obj = malloc(sizeof(*obj));
    if (JsonToObj(val, obj) != BECO_ERR_OK) {
      goto error;
    }
    BecoArrayAdd(out, pos++, obj);
  }
  error:
  return BECO_ERR_OK;
}

BecoError JsonToObj(yyjson_val *root, struct BecoObject *out) {
  if (root == NULL || out == NULL) return BECO_ERR_NULL;

  bool val_bool = false;
  const char *val_str = NULL;
  uint64_t val_u64 = 0;
  int64_t val_i64 = 0;
  double val_f64 = 0;
  struct BecoMap *val_map = NULL;
  struct BecoArray *val_array = NULL;
  enum BecoValueType val_type = BECO_VALUE_TYPE_NONE;
  size_t arr_len = 0;

  yyjson_type type;
  yyjson_type main_type;
  yyjson_type sub_type;

  type = yyjson_get_type(root);
  main_type = type & YYJSON_TYPE_MASK;
  sub_type = type & YYJSON_SUBTYPE_MASK;
  switch (main_type) {
    case YYJSON_TYPE_BOOL: {
      val_bool = yyjson_get_bool(root);
      val_type = BECO_VALUE_TYPE_BOOL;
      break;
    }
    case YYJSON_TYPE_NUM : {
      switch (sub_type) {
        case YYJSON_SUBTYPE_UINT: {
          val_u64 = yyjson_get_uint(root);
          val_type = BECO_VALUE_TYPE_POSITIVE_INTEGER;
          break;
        }
        case YYJSON_SUBTYPE_SINT: {
          val_i64 = yyjson_get_sint(root);
          val_type = BECO_VALUE_TYPE_INTEGER;
          break;
        }
        case YYJSON_SUBTYPE_REAL: {
          val_f64 = yyjson_get_real(root);
          val_type = BECO_VALUE_TYPE_DOUBLE;
          break;
        }
        default: {
          // unknown sub type
        }
      }
      break;
    }
    case YYJSON_TYPE_STR : {
      val_str = yyjson_get_str(root);
      val_type = BECO_VALUE_TYPE_STR;
      break;
    }
    case YYJSON_TYPE_ARR : {
      arr_len = yyjson_arr_size(root);
      val_array = BecoArrayNew(arr_len);
      JsonToArr(root, val_array);
      val_type = BECO_VALUE_TYPE_ARRAY;
      break;
    }
    case YYJSON_TYPE_OBJ : {
      val_map = BecoMapNew();
      JsonToMap(root, val_map);
      val_type = BECO_VALUE_TYPE_MAP;
      break;
    }
    case YYJSON_TYPE_NULL:
    case YYJSON_TYPE_NONE:
    default: {
      val_type = BECO_VALUE_TYPE_NONE;
      // unknown type
    }
  }

  out->type = val_type;
  switch (val_type) {
    case BECO_VALUE_TYPE_NONE:break;
    case BECO_VALUE_TYPE_BOOL: {
      out->via.bool_ = val_bool;
      break;
    }
    case BECO_VALUE_TYPE_INTEGER: {
      out->via.i64 = val_i64;
      break;
    }
    case BECO_VALUE_TYPE_POSITIVE_INTEGER: {
      out->via.u64 = val_u64;
      break;
    }
    case BECO_VALUE_TYPE_DOUBLE: {
      out->via.f64 = val_f64;
      break;
    }
    case BECO_VALUE_TYPE_STR: {
      out->via.str = strdup(val_str);
      break;
    }
    case BECO_VALUE_TYPE_MAP: {
      out->via.map = val_map;
      break;
    }
    case BECO_VALUE_TYPE_ARRAY: {
      out->via.array = val_array;
      break;
    }
  }

  return BECO_ERR_OK;
}

BecoError JsonToMap(yyjson_val *root, struct BecoMap *out) {
  if (root == NULL || out == NULL) return BECO_ERR_NULL;

  struct BecoObject *obj = NULL;

  const char *key_str = NULL;

  yyjson_val *key, *val;
  yyjson_obj_iter iter;
  yyjson_obj_iter_init(root, &iter);
  while ((key = yyjson_obj_iter_next(&iter))) {

    val = yyjson_obj_iter_get_val(key);
    key_str = yyjson_get_str(key);

    obj = BecoObjectNew();
    if (JsonToObj(val, obj) != BECO_ERR_OK) {
      goto error;
    }

    BecoMapPut(out, key_str, obj);
  }

  error:
  return BECO_ERR_OK;
}

BecoError ObjToJson(struct BecoObject *obj, yyjson_mut_doc *doc, yyjson_mut_val **out) {
  if (obj == NULL || out == NULL) return BECO_ERR_NULL;

  yyjson_mut_val *val = NULL;
  switch (obj->type) {
    case BECO_VALUE_TYPE_NONE: {
      val = yyjson_mut_null(doc);
      break;
    }
    case BECO_VALUE_TYPE_BOOL: {
      val = yyjson_mut_bool(doc, obj->via.bool_);
      break;
    }
    case BECO_VALUE_TYPE_INTEGER: {
      val = yyjson_mut_int(doc, obj->via.i64);
      break;
    }
    case BECO_VALUE_TYPE_POSITIVE_INTEGER: {
      val = yyjson_mut_uint(doc, obj->via.u64);
      break;
    }
    case BECO_VALUE_TYPE_DOUBLE: {
      val = yyjson_mut_real(doc, obj->via.f64);
      break;
    }
    case BECO_VALUE_TYPE_STR: {
      val = yyjson_mut_str(doc, obj->via.str);
      break;
    }
    case BECO_VALUE_TYPE_MAP: {
      MapToJson(obj->via.map, doc, &val);
      break;
    }
    case BECO_VALUE_TYPE_ARRAY: {
      ArrToJson(obj->via.array, doc, &val);
      break;
    }
  }
  *out = val;
  return BECO_ERR_OK;
}

BecoError MapToJson(struct BecoMap *obj, yyjson_mut_doc *doc, yyjson_mut_val **out) {
  if (obj == NULL) return BECO_ERR_NULL;

  struct BecoMapEntry *entry, *temp;
  struct BecoKV *kv = NULL;
  yyjson_mut_val *val;
  yyjson_mut_val *vk;
  yyjson_mut_val *vv;

  val = yyjson_mut_obj(doc);
  HASH_ITER(hh, obj->entries, entry, temp) {
    kv = entry->kv;
    if (kv != NULL) {
      vk = yyjson_mut_str(doc, kv->key);
      ObjToJson(kv->value, doc, &vv);
      yyjson_mut_obj_add(val, vk, vv);
    }
  }
  *out = val;
  return BECO_ERR_OK;
}

BecoError ArrToJson(struct BecoArray *obj, yyjson_mut_doc *doc, yyjson_mut_val **out) {
  if (obj == NULL) return BECO_ERR_NULL;

  struct BecoObject *element = NULL;
  yyjson_mut_val *val = NULL;
  yyjson_mut_val *ve = NULL;
  int i;

  val = yyjson_mut_arr(doc);
  if (obj->ptr != NULL) {
    for (i = 0; i < obj->size; ++i) {
      element = obj->ptr[i];
      ObjToJson(element, doc, &ve);
      yyjson_mut_arr_add_val(val, ve);
    }
  }

  *out = val;
  return BECO_ERR_OK;
}

void FreeHandler(struct BecoRequestHandler *handler) {
  if (handler == NULL) return;
  free(handler->cmd);
  free(handler);
}
