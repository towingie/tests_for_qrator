
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define THREADS 4
#define PRIO_LOW 0
#define PRIO_NORM 1
#define PRIO_HIGH 2
#define MAX_THREADS 32

typedef struct {
    void (*function)(void *);
    void *argument;
} threadpool_task_t;

struct threadpool_t {
  pthread_mutex_t lock;
  pthread_cond_t notify;
  pthread_t *threads;
  threadpool_task_t *queue0;
  threadpool_task_t *queue1;
  threadpool_task_t *queue2;
  int ql0;
  int ql1;
  int ql2;
  int thread_count;
  int queue_size0;
  int queue_size1;
  int queue_size2;
  int head0;
  int head1;
  int head2;
  int tail0;
  int tail1;
  int tail2;
  int count;
  int shutdown;
  int started;
};

long tasks = 0;
int done = 0;
pthread_mutex_t lock;

static void *threadpool_thread(void *threadpool);
typedef struct threadpool_t threadpool_t;
threadpool_t *threadpool_create(int thread_count);

int threadpool_free(threadpool_t *pool);
int max_high_prio = 3;

threadpool_t *threadpool_create(int thread_count)
{
    threadpool_t *pool;
    int i;
    int flags = 0;
    int queue_size = 256;

    if(thread_count <= 0 || thread_count > MAX_THREADS) {
        printf("ERR: thread_count should be > 0 and < %d", MAX_THREADS);
        return NULL;
    }

    pool = (threadpool_t *)malloc(sizeof(threadpool_t));

    pool->thread_count = 0;
    pool->queue_size0 = queue_size;
    pool->queue_size1 = queue_size;
    pool->queue_size2 = queue_size;
    pool->head0 = 0;
    pool->head1 = 0;
    pool->head2 = 0;
    pool->tail0 = 0;
    pool->tail1 = 0;
    pool->tail2 = 0;
    pool->count = 0;
    pool->shutdown = 0;
    pool->started = 0;

    /* Allocate thread and task queue */
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    pool->queue0 = (threadpool_task_t *)malloc (sizeof(threadpool_task_t) * queue_size);
    pool->queue1 = (threadpool_task_t *)malloc (sizeof(threadpool_task_t) * queue_size);
    pool->queue2 = (threadpool_task_t *)malloc (sizeof(threadpool_task_t) * queue_size);
    pool->ql0 = 0;
    pool->ql1 = 0;
    pool->ql2 = 0;

    pthread_mutex_init(&(pool->lock), NULL);
    pthread_cond_init(&(pool->notify), NULL);

    for(i = 0; i < thread_count; i++) {
        pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void*)pool);
        pool->thread_count++;
        pool->started++;
    }

    return pool;
}

int threadpool_add(threadpool_t *pool, void (*function)(void *), void *argument, int flags, int prio)
{
    int ret = 0;
    int next0, next1, next2;
    (void) flags;

    pthread_mutex_lock(&(pool->lock));

    next0 = (pool->tail0 + 1) % pool->queue_size0;
    next1 = (pool->tail1 + 1) % pool->queue_size1;
    next2 = (pool->tail2 + 1) % pool->queue_size2;

    do {
      if(pool->shutdown) {
          ret = -1;
          break;
      }

      if (prio == 0){
        pool->queue0[pool->tail0].function = function;
        pool->queue0[pool->tail0].argument = argument;
        pool->ql0++;
        pool->tail0 = next0;
      } else if (prio == 1) {
        pool->queue1[pool->tail1].function = function;
        pool->queue1[pool->tail1].argument = argument;
        pool->ql1++;
        pool->tail1 = next1;
      } else if (prio == 2) {
        pool->queue2[pool->tail2].function = function;
        pool->queue2[pool->tail2].argument = argument;
        pool->ql2++;
        pool->tail2 = next2;
      }
      pool->count += 1;

      pthread_cond_signal(&(pool->notify));
    } while(0);

    pthread_mutex_unlock(&pool->lock);

    return ret;
}

int threadpool_destroy(threadpool_t *pool, int flags)
{
    pthread_mutex_lock(&(pool->lock));

    do {
        pool->shutdown = 2;

        pthread_cond_broadcast(&(pool->notify));
        pthread_mutex_unlock(&(pool->lock));

        for(int i = 0; i < pool->thread_count; i++)
            pthread_join(pool->threads[i], NULL);

    } while(0);

    threadpool_free(pool);
    return 0;
}

int threadpool_free(threadpool_t *pool)
{
    if(pool == NULL || pool->started > 0) {
        printf("ERR: pool null? or someone still alive\n");
        return -1;
    }

    if(pool->threads) {
        free(pool->threads);
        free(pool->queue0);
        free(pool->queue1);
        free(pool->queue2);

        pthread_mutex_lock(&(pool->lock));
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->notify));
    }
    free(pool);
    return 0;
}


static void *threadpool_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;

    for(;;) {
        pthread_mutex_lock(&(pool->lock));
        while((pool->count == 0) && (!pool->shutdown))
            pthread_cond_wait(&(pool->notify), &(pool->lock));

        if(pool->shutdown == 2 && pool->count == 0)
            break;

        if (pool->ql2 > 0 && max_high_prio < 3) {
          printf("DO ql2=%d (HIGH)\n",pool->ql2);
          task.function = pool->queue2[pool->head2].function;
          task.argument = pool->queue2[pool->head2].argument;
          pool->ql2--;
          pool->head2 = (pool->head2 + 1) % pool->queue_size1;
          max_high_prio++;
        } else if (pool->ql1 > 0) {
          printf("DO ql1=%d (NORM)\n",pool->ql1);
          task.function = pool->queue1[pool->head1].function;
          task.argument = pool->queue1[pool->head1].argument;
          pool->ql1--;
          pool->head1 = (pool->head1 + 1) % pool->queue_size1;
          max_high_prio = 0;
        } else if (pool->ql0 > 0) {
          printf("DO ql0=%d (LOW)\n",pool->ql0);
          task.function = pool->queue0[pool->head0].function;
          task.argument = pool->queue0[pool->head0].argument;
          pool->ql0--;
          pool->head0 = (pool->head0 + 1) % pool->queue_size0;
        }
        pool->count -= 1;

        pthread_mutex_unlock(&(pool->lock));

        (*(task.function))(task.argument);
    }

    pool->started--;

    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return NULL;
}

int threadpool_wait(threadpool_t *pool)
{
  int wait = 1;
  while(wait) {
    usleep(10000);
    pthread_mutex_lock(&(pool->lock));

    if(pool->count == 0)
      wait = 0;

    pthread_mutex_unlock(&(pool->lock));
  }
  return 0;
}

void dummy_task(void *task) {
    usleep(10000);
    pthread_mutex_lock(&lock);
    printf("Thread working task job #%ld for dummy_task\n",(long) task);
    done++;
    pthread_mutex_unlock(&lock);
}

int main(int argc, char **argv)
{
    threadpool_t *pool;

    pthread_mutex_init(&lock, NULL);

    pool = threadpool_create(THREADS);
    printf("Pool created with %d threads\n", THREADS);

    while(tasks != 10){
        threadpool_add(pool, &dummy_task, (void *)tasks, 0, PRIO_LOW);
        threadpool_add(pool, &dummy_task, (void *)tasks+1000, 0, PRIO_NORM);
        threadpool_add(pool, &dummy_task, (void *)tasks+2000, 0, PRIO_HIGH);
        pthread_mutex_lock(&lock);
        tasks++;
        pthread_mutex_unlock(&lock);
    }

    fprintf(stderr, "Task adding iters %ld done, should be added %ld tasks\n", tasks, tasks * 3);

    threadpool_wait(pool);
    threadpool_destroy(pool, 0);
    fprintf(stderr, "done %d tasks\n", done);

    return 0;
}
