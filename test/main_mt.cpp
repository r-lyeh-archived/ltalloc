#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ltalloc.cc"

static const size_t INITIAL_MEMORY_SIZE = 2;
static const size_t NUM_ALLOCATIONS = 32;
static const size_t NUM_ITERATIONS = 100;

static void multiple_malloc() {
  size_t previous_size = 0;
  size_t current_size = INITIAL_MEMORY_SIZE;
  char* data[NUM_ALLOCATIONS];

  for (size_t i = 0; i < NUM_ALLOCATIONS; i++) {
    data[i] = (char*)malloc(sizeof(char) * current_size);
    assert(data[i]);
    current_size += previous_size;
    previous_size = current_size;
  }
  for (size_t i = 0; i < NUM_ALLOCATIONS; i++) {
    free(data[i]);
  }
}


static void multiple_realloc() {
  size_t current_size = INITIAL_MEMORY_SIZE;
  char* mem = 0;
  for (size_t i = 0; i < NUM_ALLOCATIONS; i++) {
    mem = (char*)realloc(mem, sizeof(char) * current_size);
    assert(mem);
    current_size += current_size;
  }
  free(mem);
}
///***********************ltalloc*************************
static void multiple_ltmalloc() {
  size_t previous_size = 0;
  size_t current_size = INITIAL_MEMORY_SIZE;
  char* data[NUM_ALLOCATIONS];

  for (size_t i = 0; i < NUM_ALLOCATIONS; i++) {
    data[i] = (char*)ltmalloc(sizeof(char) * current_size);
    assert(data[i]);
    current_size += previous_size;
    previous_size = current_size;
  }
  for (size_t i = 0; i < NUM_ALLOCATIONS; i++) {
    ltfree(data[i]);
  }
}


static void multiple_ltrealloc() {
  size_t current_size = INITIAL_MEMORY_SIZE;
  char* mem = 0;
  for (size_t i = 0; i < NUM_ALLOCATIONS; i++) {
    mem = (char*)ltrealloc(mem, sizeof(char) * current_size);
    assert(mem);
    current_size += current_size;
  }
  ltfree(mem);
}

//**********************************************

static void run_test(
  const char* test_name,
  void allocate()) {
  clock_t start = clock();

  for (size_t i = 0; i < NUM_ITERATIONS; i++) {
    allocate();
  }

  clock_t end = clock();
  printf("%s: ran in %lf seconds.\n",
     test_name, (end - start) / (double)CLOCKS_PER_SEC);
}

int main(int argc, char** argv) {

  run_test("multiple_malloc", multiple_malloc);
  run_test("multiple_realloc", multiple_realloc);

  run_test("multiple_ltmalloc", multiple_ltmalloc);
  run_test("multiple_ltrealloc", multiple_ltrealloc);

  return 0;
}