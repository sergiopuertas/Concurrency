#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "options.h"
#include <pthread.h>

#define DELAY_SCALE 1000

enum action{
    INCREMENT,
    MOVE
};
char* enumtochar(enum action action){
    if(action == INCREMENT){
        return "increasing";
    }
    else{
        return "moving";
    }
}

struct slot{
    int value;
    pthread_mutex_t lock;
};

struct array {
    int size;
    struct slot *arr;

    pthread_mutex_t counterinclock;
    pthread_mutex_t countermovelock;

    int COUNTERINC;
    int COUNTERMOV;
};
struct param {
    int id;
    int it;
    int delay;
    struct array *array;
};
struct thread{
    pthread_t id;
    struct param *params;

};
void apply_delay(int delay) {
    for(int i = 0; i < delay * DELAY_SCALE; i++); // waste time
}

void destroyMutexes(struct array nums){
    int i = 0;
    while(i<nums.size){
        pthread_mutex_destroy(&nums.arr[i].lock);
        ++i;
        
    }
    pthread_mutex_destroy(&nums.counterinclock);
    pthread_mutex_destroy(&nums.countermovelock);
}

void initMutexes(struct array nums){
    int i = 0;
    pthread_mutex_init(&nums.counterinclock,NULL);
    pthread_mutex_init(&nums.countermovelock,NULL);
    while(i<nums.size){
        pthread_mutex_init(&nums.arr[i].lock,NULL );
        ++i;
    }
    

}
void* increment(void* r)
{
    struct param * p=r;
    int pos, val;

    pthread_mutex_lock(&p->array->counterinclock);

    while(1){

        if(p->array->COUNTERINC >= p->it) {
             pthread_mutex_unlock(&p->array->counterinclock);
             break;
        }
        p->array->COUNTERINC++;
        pthread_mutex_unlock(&p->array->counterinclock);

        pos = rand() % p->array->size;

        printf("Thread %d increasing position %d\n", p->id, pos);
       
        pthread_mutex_lock(&(p->array->arr[pos].lock));

        val = p->array->arr[pos].value;
        apply_delay(p->delay);
    
        val ++;
        apply_delay(p->delay);

        p->array->arr[pos].value = val;

        apply_delay(p->delay);
          
        pthread_mutex_unlock(&(p->array->arr[pos].lock));
        pthread_mutex_lock(&p->array->counterinclock);

    }

    return NULL;
}

void* move(void* r)

{
    struct param * p=r;
    int pos1, pos2, val1, val2;

    pthread_mutex_lock(&p->array->countermovelock);

    while(1){

        if(p->array->COUNTERMOV >= p->it) {
             pthread_mutex_unlock(&p->array->countermovelock);
             break;
        }
        p->array->COUNTERMOV++;
        pthread_mutex_unlock(&p->array->countermovelock);

        pos1 = rand() % p->array->size;
        pos2 = rand() % p->array->size;

        printf("Thread %d moving values in positions %d and %d\n", p->id,pos1, pos2);

        pthread_mutex_lock(&(p->array->arr[pos1].lock));
    
        val1 = p->array->arr[pos1].value;
        apply_delay(p->delay);
        
        val1 --;
        apply_delay(p->delay);

        p->array->arr[pos1].value = val1;

        pthread_mutex_unlock(&(p->array->arr[pos1].lock));  

        pthread_mutex_lock(&(p->array->arr[pos2].lock));

        val2 = p->array->arr[pos2].value;
        apply_delay(p->delay);

        val2 ++;
        apply_delay(p->delay);
  
        p->array->arr[pos2].value = val2;

        pthread_mutex_unlock(&(p->array->arr[pos2].lock));  
        pthread_mutex_lock(&p->array->countermovelock);
    }
    return NULL;
}

void print_array(struct array arr) {
    int total = 0;

    for(int i = 0; i < arr.size; i++) {
        total += arr.arr[i].value;
        printf("%d ", arr.arr[i].value);
    }

    printf("\nTotal: %d\n", total);
}

struct thread *run_threads(struct options opt, struct array *arr, enum action action, int num_threads){
    int i;
    struct thread *threads;

    printf("creating %d threads that are %s\n", opt.num_threads,  enumtochar(action));
	threads = malloc(sizeof(struct thread) * opt.num_threads);

	if (threads == NULL) {
		printf("Not enough memory\n");
		exit(1);
	}

	for (i = 0; i < opt.num_threads; i++) {
        threads[i].params = malloc(sizeof(struct param));
        threads[i].params->array = arr;
        threads[i].params->it =opt.iterations;
        threads[i].params->delay =opt.delay;
        threads[i].params->id=i+1+num_threads;
        int rt = 0;
        switch (action){
            case INCREMENT: rt = pthread_create(&threads[i].id, NULL, increment, threads[i].params);
                            break;
            case MOVE: rt = pthread_create(&threads[i].id, NULL, move, threads[i].params);
                            break;
        }
        if (rt != 0) {
            printf("Impossible to create thread number %d", i);
            exit(1);
        }
    }
    return threads;
}

void wait_thread(struct options opt, struct array *arr, struct thread *threads) {
    
    int i = 0;
    while(i<opt.num_threads){
        pthread_join(threads[i].id, NULL);
        i++;
    }
   
    for (int i = 0; i < opt.num_threads; i++)
        free(threads[i].params);

    free(threads);
}
int main (int argc, char **argv)
{
    struct options       opt;
    struct array         nums;
    struct thread       *threads, *threads2;

    srand(time(NULL));

    opt.num_threads  = 2;
    opt.size         = 10;
    opt.iterations   = 200;
    opt.delay        = 1000;

    read_options(argc, argv, &opt);

    nums.size = opt.size;
    nums.arr  = malloc(nums.size * sizeof(struct slot));
    memset(nums.arr, 0, nums.size * sizeof(struct slot));
    pthread_mutex_init(&nums.counterinclock,NULL);
    pthread_mutex_init(&nums.countermovelock,NULL);

    initMutexes(nums);
    nums.COUNTERINC = 0;
    nums.COUNTERMOV = 0;
    
    threads = run_threads(opt, &nums, INCREMENT, 0);
    threads2 = run_threads(opt, &nums, MOVE, opt.num_threads); 
    
    wait_thread(opt, &nums, threads);
    wait_thread(opt, &nums, threads2);
    print_array(nums);
   
    destroyMutexes(nums);
  
    free(nums.arr);
    return 0;
}