#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"
#include "mmu.h"  // PGSIZE
#include "defs.h"

char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

void*
memset(void *dst, int c, uint n)
{
  stosb(dst, c, n);
  return dst;
}

char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

void*
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  while(n-- > 0)
    *dst++ = *src++;
  return vdst;
}

// int thread_create(void (*start_routine)(void *, void *), void *arg1, void *arg2) {
//   void *stack = malloc(PGSIZE);
//   if (stack == 0)
//     return -1;
//   int r = clone(start_routine, arg1, arg2, stack);
//   if (r == -1)
//     free(stack);
//   return r;
// }

#define MAX_THREADS 64

struct thread {
  void (*func)(void *, void *);
  void *arg1;
  void *arg2;
  void *stack;
};

struct thread threads[MAX_THREADS];
char *stack_pool[MAX_THREADS];
int stack_count = 0;

void init_threading() {
  int i;
  for (i = 0; i < MAX_THREADS; i++) {
    stack_pool[i] = kalloc();
    if (stack_pool[i] == 0) {
      panic("Out of memory\n");
    }
  }
}

void *get_stack() {
  if (stack_count == MAX_THREADS) {
    return 0;
  }
  return stack_pool[stack_count++];
}

void release_stack(char *stack) {
  if (stack_count == 0) {
    panic("Stack pool underflow\n");
  }
  stack_pool[--stack_count] = stack;
}

int thread_create(void (*func)(void *, void *), void *arg1, void *arg2) {
  struct thread *t;
  char *stack;

  if (stack_count == 0) {
    return -1;  // No more stack memory
  }

  t = &threads[0];
  while (t->stack != 0 && t < &threads[MAX_THREADS]) {
    t++;
  }
  if (t == &threads[MAX_THREADS]) {
    return -1;  // No more thread slots
  }

  stack = get_stack();
  if (stack == 0) {
    return -1;  // Out of memory
  }

  t->func = func;
  t->arg1 = arg1;
  t->arg2 = arg2;
  t->stack = stack;

  return 0;
}

void thread_exit() {
  struct thread *t = &threads[0];
  while (t->stack != 0 && t < &threads[MAX_THREADS]) {
    t++;
  }
  if (t == &threads[MAX_THREADS]) {
    panic("No thread to exit\n");
  }
  release_stack(t->stack);
  t->stack = 0;
}

void scheduler() {
  int i;

  while (1) {
    for (i = 0; i < MAX_THREADS; i++) {
      if (threads[i].stack != 0) {
        threads[i].func(threads[i].arg1, threads[i].arg2);
        thread_exit();
      }
    }
  }
}

int thread_join() {
  void *stack;
  int r = join(&stack);
  free(stack);
  return r;
}

// ticket lock
void lock_init(lock_t *lock) {
  lock->ticket = 0;
  lock->turn = 0;
}

void lock_acquire(lock_t *lock) {  
  int myturn = __atomic_fetch_add(&lock->ticket, 1, __ATOMIC_SEQ_CST);
  while (lock->turn != myturn)
    ; // spin
}

void lock_release(lock_t *lock) {
  lock->turn += 1;
}