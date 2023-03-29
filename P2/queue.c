#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

// circular array
typedef struct _queue {
    int count;
    int size;
    int used;
    int first;
    void **data;
    pthread_mutex_t lock;
    pthread_cond_t empty;
    pthread_cond_t full;
    pthread_cond_t stop;

    bool finished; // check if getfiles already finished putting things in the queue before removing or doing anything

} _queue;

#include "queue.h"
#include "options.h"

queue q_create(int size, int num) {
    queue q = malloc(sizeof(_queue));
    q->count = num;
    q->size  = size;
    q->used  = 0;
    q->first = 0;
    q->finished = false;
    q->data  = malloc(size * sizeof(void *));
    pthread_mutex_init (&(q->lock),NULL);
    pthread_cond_init (&(q->full),NULL);
    pthread_cond_init (&(q->empty),NULL);
    pthread_cond_init (&(q->stop),NULL);

    return q;
}

int q_insert(queue q, void *elem) {

    pthread_mutex_lock(&(q->lock));

    while(q_full(q)){
        pthread_cond_wait(&q->full,&q->lock);
    }
    q->data[(q->first + q->used) % q->size] = elem;
    q->used++;
    pthread_cond_broadcast(&q->empty);
    pthread_mutex_unlock(&(q->lock));
    return 0;
}

void *q_remove(queue q) {
    void *res;
    pthread_mutex_lock(&(q->lock));
    while(q_empty(q)){
        if (q->finished ){
            pthread_mutex_unlock(&(q->lock));
            return NULL;
        }
        pthread_cond_wait(&q->empty,&q->lock);
    }
    res = q->data[q->first];
    q->first = (q->first + 1) % q->size;
    q->used--;
    pthread_cond_broadcast(&q->full);
    pthread_mutex_unlock(&(q->lock));
    return res;
}


void q_destroy(queue q) {
    pthread_mutex_destroy(&(q->lock));
    free(q->data);
    free(q);
}
bool q_full(queue q){
    if(q->used == q->size) return true;
    else return false;
}
bool q_empty (queue q){
    if(q->used == 0) return true;
    else return false;
}

void waitForThreads(queue q){
    pthread_mutex_lock(&q->lock);
    q->count--;
    while(q->count > 0){
        pthread_cond_wait(&q->stop,&q->lock);
    }
    pthread_cond_broadcast(&q->stop);
    pthread_mutex_unlock(&q->lock);
}

void has_finished(queue q){
    pthread_mutex_lock(&(q->lock));
    q->finished = true;
    pthread_cond_broadcast(&q->empty);
    pthread_mutex_unlock(&(q->lock));
}