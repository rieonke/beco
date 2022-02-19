# beco

A Browser(Chrome/Firefox/Edge) extension native messaging host library/framework for C.

## Intro

Beco (**B**rowser **E**xtension **C** native messaging h**O**st)
helps you to build and test native messaging host easily.

Beco implemented the native messaging protocol and a command-driven request-response application model. It also provides
a mock tool for you to test your app.

## Features

- Native messaging protocol
- Convert between JSON and C seamlessly
- Command pattern programming model
- Easy and native mock framework

## Usages
check out examples/ to find more usages
### Basic
```c
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

```

### Mock
```c

void test_hello(struct BecoContext *ctx) {
  struct BecoObject obj;
  struct BecoMap *map = NULL;
  struct BecoRequest req = {0};

  map = BecoMapNew();
  BecoMapPut(map, "hello", STR("you"));

  obj.type = BECO_VALUE_TYPE_MAP;
  obj.via.map = map;

  BecoWrite(ctx, &obj); // send request

  BecoRead(ctx, &req);  // receive response

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

  BecoWrite(ctx, &obj); // send request

  BecoRead(ctx, &req); // receive response

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
```

## Build

### Tested platforms
- Windows
- Linux
- macOS

### Requirements
- C90 Compiler (GCC/Clang/MSVC/...)
- CMake >= 3.0

### Build

```shell

mkdir build && cd build
cmake ..
cmake --build .
cmake --install . # optional

```

## TODO
* [ ] Add documentation and Github page.
* [ ] Add GitHub workflow for CI and codecov.
* [ ] Support more json libraries
* [ ] Add more tests: valgrind, sanitizer.
* [ ] Add more utilities
