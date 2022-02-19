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

#ifndef BECO_H
#define BECO_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum BecoError {
  BECO_ERR_OK = 0,
  BECO_ERR_IO = 1,
  BECO_ERR_OVERFLOW = 2,
  BECO_ERR_NULL = 3,
  BECO_ERR_INVALID_JSON = 4,
  BECO_ERR_NO_IMPL = 5,
  BECO_ERR_GENERIC = 9,
} BecoError;

typedef enum BecoValueType {
  BECO_VALUE_TYPE_NONE,
  BECO_VALUE_TYPE_BOOL,
  BECO_VALUE_TYPE_INTEGER,
  BECO_VALUE_TYPE_POSITIVE_INTEGER,
  BECO_VALUE_TYPE_DOUBLE,
  BECO_VALUE_TYPE_STR,
  BECO_VALUE_TYPE_MAP,
  BECO_VALUE_TYPE_ARRAY,
} BecoValueType;

struct BecoRequest;
struct BecoContext;
struct BecoMap;
struct BecoKV;
struct BecoArray;
struct BecoObject;
struct BecoRequestHandler;

typedef BecoError (*BecoRequestHandlerFunc)(struct BecoContext *, struct BecoRequest *, void *);

struct BecoObject {
  enum BecoValueType type;
  union {
    bool bool_;
    uint64_t u64;
    int64_t i64;
    double f64;
    char *str;
    struct BecoMap *map;
    struct BecoArray *array;
  } via;
};

struct BecoKV {
  enum BecoValueType type;
  char *key;
  struct BecoObject *value;
};

struct BecoArray {
  size_t size;
  struct BecoObject **ptr;
};

struct BecoRequest {
  char *cmd;
  struct BecoObject *data;
};

struct BecoConf {
  void (*sig_handler)(int);
  bool use_stdio;
  FILE *in;
  FILE *out;
  FILE *log_file;
  BecoRequestHandlerFunc default_cmd_handler;
  void *default_user_data;
  BecoRequestHandlerFunc null_cmd_handler;
  void *null_user_data;
  bool *exit_flag;
};

struct BecoContext {
  FILE *in;
  FILE *out;
  FILE *log;
  struct BecoRequestHandler *handler_entries;
  struct BecoRequestHandler *null_cmd_handler;
  struct BecoRequestHandler *default_cmd_handler;
};

/******************************************
 * Core Functions
 *   - BecoContext
 *   - Configuration
 *   - Main Loop
 *   - Command Handlers
 *****************************************/

/**
 * Initialize a context allocated by yourself, especially allocated in stack
 * @param ctx context
 */
void BecoContextInit(struct BecoContext *ctx);

/**
 * Create a new context, it's initialized by default.
 * @return context
 */
struct BecoContext *BecoContextNew();

/**
 * Create a new context, initialized by user defined configuration
 * @param conf configuration
 * @return context
 */
struct BecoContext *BecoContextNewWithConf(struct BecoConf *conf);

/**
 * Set log file handle, log functionality will be disabled if file is NULL
 * @param ctx context
 * @param file writable file handle
 */
void BecoSetLog(struct BecoContext *ctx, FILE *file);

/**
 * Set input channel, stdin by default
 * @param ctx context
 * @param in input channel
 * @return error
 */
BecoError BecoSetIn(struct BecoContext *ctx, FILE *in);

/**
 * Set output channel, stdout by default
 * @param ctx context
 * @param out output channel
 * @return error
 */
BecoError BecoSetOut(struct BecoContext *ctx, FILE *out);

/**
 * Free a context allocated on heap
 * @param ctx context
 */
void BecoContextFree(struct BecoContext *ctx);

/**
 * Destroy a context, won't free context itself
 * @param ctx context
 */
void BecoContextDestroy(struct BecoContext *ctx);

/**
 * Register function handler for NULL-command-request which does not contain a command field
 * @param ctx context
 * @param handler handler
 * @param user_data user data
 */
void BecoSetNullCmdHandler(struct BecoContext *ctx, BecoRequestHandlerFunc handler, void *user_data);

/**
 * Register default function handler, it's a fallback option when no handler founded for a request
 * @param ctx context
 * @param handler handler
 * @param user_data user data
 */
void BecoSetDefaultCmdHandler(struct BecoContext *ctx, BecoRequestHandlerFunc handler, void *user_data);

/**
 * Send a response, synonym for BecoWrite()
 * @param ctx context
 * @param res response
 * @return error
 */
BecoError BecoSendResponse(struct BecoContext *ctx, struct BecoObject *res);

/**
 * Beco main loop, it will handle incoming requests continuously unless `exit` state changed
 * @param ctx context
 * @param exit loop exit when exit turns to be true
 * @param exit_on_fail if true, loop will exit when error occurs
 * @return last error
 */
BecoError BecoMainLoop(struct BecoContext *ctx, const volatile bool *exit, bool exit_on_fail);

/**
 * Handle an incoming request only once
 * @param ctx context
 * @return error
 */
BecoError BecoNext(struct BecoContext *ctx);

/**
 * Read request from input channel
 * @param ctx context
 * @param req output request
 * @return error
 */
BecoError BecoRead(struct BecoContext *ctx, struct BecoRequest *req);

/**
 * Write response to output channel
 * @param ctx context
 * @param res response
 * @return error
 */
BecoError BecoWrite(struct BecoContext *ctx, struct BecoObject *res);

/**
 * Read raw request content from channel
 * @param in input channel
 * @param out raw request content
 * @param olen raw request content length
 * @return error
 */
BecoError BecoReadRaw(FILE *in, char **out, size_t *olen);

/**
 * Write raw response content to channel
 * @param out output channel
 * @param data raw response content
 * @param dlen raw response content
 * @return error
 */
BecoError BecoWriteRaw(FILE *out, const char *data, size_t dlen);

/**
 * Register command handler function to context
 * @param ctx context
 * @param cmd command name
 * @param handler handler function
 * @param user_data user data
 * @return error
 */
BecoError BecoRegisterCommand(struct BecoContext *ctx,
                              const char *cmd,
                              BecoRequestHandlerFunc handler,
                              void *user_data);

/**
 * Remove command handler function
 * @param ctx context
 * @param cmd command name
 * @return error
 */
BecoError BecoRemoveCommand(struct BecoContext *ctx, const char *cmd);

/**
 * Find command handler for specific command
 * @param ctx context
 * @param cmd command name
 * @return error
 */
struct BecoRequestHandler *BecoFindRequestHandler(struct BecoContext *ctx, const char *cmd);

/******************************************
 * Common Data Structures
 *   - BecoMap          HashMap
 *   - BecoArray        Fixed-size Array
 *   - BecoObject       Generic Object
 *   - BecoKV           Key-Value
 *****************************************/

/**
 * Create an object
 * @return object
 */
struct BecoObject *BecoObjectNew();

/**
 * Get value type.
 * @param obj object
 * @return  value type
 */
enum BecoValueType BecoObjectGetType(struct BecoObject *obj);

/**
 * Get 64-bit signed int value
 * @param obj object
 * @return 64-bit signed int value
 */
int64_t BecoObjectGetInt64(struct BecoObject *obj);

/**
 * Get 64-bit unsigned int value
 * @param obj object
 * @return 64-bit unsigned int value
 */
uint64_t BecoObjectGetUInt64(struct BecoObject *obj);

/**
 * Get bool value
 * @param obj object
 * @return bool
 */
bool BecoObjectGetBool(struct BecoObject *obj);

/**
 * Get 64-bit float point number
 * @param obj object
 * @return 64-bit float point number
 */
double BecoObjectGetFloat64(struct BecoObject *obj);

/**
 * Get null-terminated string value
 * @param obj  object
 * @return  string
 */
const char *BecoObjectGetStr(struct BecoObject *obj);

/**
 * Get hash map value
 * @param obj  object
 * @return  hash map
 */
struct BecoMap *BecoObjectGetMap(struct BecoObject *obj);

/**
 * Get fixed-size array value
 * @param obj  object
 * @return array
 */
struct BecoArray *BecoObjectGetArray(struct BecoObject *obj);

/**
 * Dump object content to stdout.
 * @param obj object
 */
void BecoObjectDump(struct BecoObject *obj);

/**
 * Dump object content to specific file
 * @param obj object
 * @param indent indent, leading space count
 * @param out output file
 */
void BecoObjectDumpF(struct BecoObject *obj, int indent, FILE *out);

/**
 * Dump object content as json
 * @param obj object
 * @param out output buf
 * @param olen output buf length
 * @return error code
 */
BecoError BecoObjectDumpJson(struct BecoObject *obj, char **out, size_t *olen);

/**
 * Duplicate an object
 * @param src source object
 * @param recursive deep copy
 * @return new object
 */
struct BecoObject *BecoObjectDup(struct BecoObject *src, bool recursive);

/**
 * Free an object, it'll free the object recursively
 * @param obj
 */
void BecoObjectFree(struct BecoObject *obj);

/**
 * Create a hash map.
 *
 * call BecoMapFree() to deallocate memory
 * @return hash map
 */
struct BecoMap *BecoMapNew();

/**
 * Add a kv entry into the hash map
 * @param map map
 * @param key key
 * @param val value
 */
void BecoMapPut(struct BecoMap *map, const char *key, struct BecoObject *val);

/**
 * Get value by key
 * @param map map
 * @param key key
 * @return value
 */
struct BecoObject *BecoMapGet(struct BecoMap *map, const char *key);

/**
 * test if map contains key
 * @param map map
 * @param key key
 * @return contains key or not
 */
bool BecoMapContainsKey(struct BecoMap *map, const char *key);

/**
 * Release a map
 * @param map map
 */
void BecoMapFree(struct BecoMap *map);

/**
 * Create a fixed-size array with given size
 * @param size size
 * @return array
 */
struct BecoArray *BecoArrayNew(size_t size);

/**
 * Get array's length
 * @param array array
 * @return length
 */
size_t BecoArrayLen(struct BecoArray *array);

/**
 * Insert an element
 * @param array array
 * @param pos index
 * @param obj value
 */
void BecoArrayAdd(struct BecoArray *array, size_t pos, struct BecoObject *obj);

/**
 * Get an element by index
 * @param array array
 * @param pos index
 * @return value
 */
struct BecoObject *BecoArrayGet(struct BecoArray *array, size_t pos);

/**
 * Free an array
 * @param array array
 */
void BecoArrayFree(struct BecoArray *array);

/**
 * Create a key-value pair
 * @return key-value pair
 */
struct BecoKV *BecoKVNew();

/**
 * Free key-value pair
 * @param kv key-value pair
 */
void BecoKVFree(struct BecoKV *kv);

/**
 * Key-value pair set key
 * @param kv kv
 * @param key key
 */
void BecoKVSetKey(struct BecoKV *kv, const char *key);

/**
 * Key-value pair set key
 * @param kv kv
 * @param obj value
 */
void BecoKVSetValue(struct BecoKV *kv, struct BecoObject *obj);

/**
 * Create request
 * @return request
 */
struct BecoRequest *BecoRequestNew();

/**
 * Initialize a request allocated by yourself
 * @param req request
 */
void BecoRequestInit(struct BecoRequest *req);

/**
 * Destroy a request, it won't free request itself
 * @param req request
 */
void BecoRequestDestroy(struct BecoRequest *req);

/**
 * Get request payload
 * @param request request
 * @return request payload
 */
struct BecoObject *BecoRequestGetData(struct BecoRequest *request);

/**
 * Get request command
 * @param request request
 * @return command, maybe NULL
 */
const char *BecoRequestGetCommand(struct BecoRequest *request);

/**
 * Set request data
 * @param request request
 * @param obj object
 */
void BecoRequestSetData(struct BecoRequest *request, struct BecoObject *obj);

/**
 * Set request command
 * @param request request
 * @param command command
 */
void BecoRequestSetCommand(struct BecoRequest *request, const char *command);

/**
 * Free request
 * @param request request
 */
void BecoRequestFree(struct BecoRequest *request);

/******************************************
 * Utilities
 *   - Log
 *****************************************/

/**
 * Log utility
 * @param ctx context
 * @param fmt format
 * @param ... args
 */
void BecoLog(struct BecoContext *ctx, const char *fmt, ...);

#endif //BECO_H
