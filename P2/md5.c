#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <openssl/evp.h>


#include "options.h"
#include "queue.h"


#define MAX_PATH 1024
#define BLOCK_SIZE (10*1024*1024)
#define MAX_LINE_LENGTH (MAX_PATH * 2)


struct file_md5 {
    char *file;
    unsigned char *hash;
    unsigned int hash_size;
};

struct write{
    pthread_t id;
    struct writePar *wpar;
};
struct writePar{
    queue* out_q;
    char* file;
    char* dir;
};
struct compute{
    pthread_t id;
    struct param *params;
};
struct read{
    pthread_t id;
    struct args* args;
};
struct args{
    char* dir;
    queue* queue;
};
struct param{
    queue* in_q;
    queue* out_q;
};

void get_entries(char *dir, queue q);
void* writeOut(void* args);

void print_hash(struct file_md5 *md5) {
    for(int i = 0; i < md5->hash_size; i++) {
        printf("%02hhx", md5->hash[i]);
    }
}


void read_hash_file(char *file, char *dir, queue q) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char *file_name, *hash;
    int hash_len;

    if((fp = fopen(file, "r")) == NULL) {
        printf("Could not open %s : %s\n", file, strerror(errno));
        exit(0);
    }

    while(fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        struct file_md5 *md5 = malloc(sizeof(struct file_md5));
        file_name = strtok(line, ": ");
        hash      = strtok(NULL, ": ");
        hash_len  = strlen(hash);

        md5->file      = malloc(strlen(file_name) + strlen(dir) + 2);
        sprintf(md5->file, "%s/%s", dir, file_name);
        md5->hash      = malloc(hash_len / 2);
        md5->hash_size = hash_len / 2;


        for(int i = 0; i < hash_len; i+=2)
            sscanf(hash + i, "%02hhx", &md5->hash[i / 2]);

        q_insert(q, md5);
    }

    fclose(fp);
}


void sum_file(struct file_md5 *md5) {
    EVP_MD_CTX *mdctx;
    int nbytes;
    FILE *fp;
    char *buf;

    if((fp = fopen(md5->file, "r")) == NULL) {
        printf("Could not open %s\n", md5->file);
        return;
    }

    buf = malloc(BLOCK_SIZE);
    const EVP_MD *md = EVP_get_digestbyname("md5");

    mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(mdctx, md, NULL);

    while((nbytes = fread(buf, 1,BLOCK_SIZE, fp)) >0)
        EVP_DigestUpdate(mdctx, buf, nbytes);

    md5->hash = malloc(EVP_MAX_MD_SIZE);
    EVP_DigestFinal_ex(mdctx, md5->hash, &md5->hash_size);

    EVP_MD_CTX_destroy(mdctx);
    free(buf);
    fclose(fp);
}


void recurse(char *entry, void *arg) {
    queue q = * (queue *) arg;
    struct stat st;

    stat(entry, &st);

    if(S_ISDIR(st.st_mode))
        get_entries(entry, q);
}


void add_files(char *entry, void *arg) {
    queue q = * (queue *) arg;
    struct stat st;

    stat(entry, &st);

    if(S_ISREG(st.st_mode))
        q_insert(q, strdup(entry));
}


void walk_dir(char *dir, void (*action)(char *entry, void *arg), void *arg) {
    DIR *d;
    struct dirent *ent;
    char full_path[MAX_PATH];

    if((d = opendir(dir)) == NULL) {
        printf("Could not open dir %s\n", dir);
        return;
    }

    while((ent = readdir(d)) != NULL) {
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") ==0)
            continue;

        snprintf(full_path, MAX_PATH, "%s/%s", dir, ent->d_name);

        action(full_path, arg);
    }

    closedir(d);
}


void get_entries(char *dir, queue q) {
    walk_dir(dir, add_files, &q);
    walk_dir(dir, recurse, &q);
}


void check(struct options opt) {
    queue in_q;
    struct file_md5 *md5_in, md5_file;

    in_q  = q_create(opt.queue_size, 1);

    read_hash_file(opt.file, opt.dir, in_q);

    has_finished(in_q);

    while((md5_in = q_remove(in_q))) {
        md5_file.file = md5_in->file;

        sum_file(&md5_file);

        if(memcmp(md5_file.hash, md5_in->hash, md5_file.hash_size)!=0) {
            printf("File %s doesn't match.\nFound:    ", md5_file.file);
            print_hash(&md5_file);
            printf("\nExpected: ");
            print_hash(md5_in);
            printf("\n");
        }

        free(md5_file.hash);

        free(md5_in->file);
        free(md5_in->hash);
        free(md5_in);
    }

    q_destroy(in_q);
}

void* readInsert(void* ar){
    struct args* args = ar;
    char *dir = args->dir;
    queue* queue = args->queue;
    get_entries(dir,*queue);
    has_finished(*queue);
    return NULL;
}
void* removeComputeInsert(void* args){
    struct param* p= args;
    char *ent;
    struct file_md5 *md5;

    while(1){
        if((ent = q_remove(*p->in_q)) == NULL){
            waitForThreads(*p->out_q);
            break;
        }
        md5 = malloc(sizeof(struct file_md5));
        md5->file = ent;

        sum_file(md5);

        q_insert(*p->out_q, md5);
    }
    has_finished(*p->out_q);

    return NULL;
}

void* writeOut(void* args){
    struct writePar* wpar =args;
    struct file_md5* md5;
    FILE* out;
    queue* queue = wpar->out_q;
    if((out = fopen(wpar->file, "w")) == NULL) {
        printf("Could not open output file\n");
        exit(0);
    }
    int dirname_len;
    dirname_len = strlen(wpar->dir) + 1; // length of dir + /
    while(1){
        if((md5 = q_remove(*queue)) ==NULL){
            break;
        }
        fprintf(out, "%s: ", md5->file + dirname_len);

        for(int i = 0; i < md5->hash_size; i++)
            fprintf(out, "%02hhx", md5->hash[i]);
        fprintf(out, "\n");

        free(md5->file);
        free(md5->hash);
        free(md5);
    }
    has_finished(*queue);
    fclose(out);
    return NULL;
}
void sum(struct options opt) {

    queue in_queue = q_create(1, opt.num_threads);
    queue out_queue = q_create(1, opt.num_threads);

    //reading files from dirs
    struct read* read;
    read = malloc(sizeof(struct read));
    read[0].args = malloc(sizeof(struct args));
    read[0].args->queue = &in_queue;
    read[0].args->dir = opt.dir;
    pthread_create(&read[0].id, NULL, readInsert, read[0].args);

    //making computations
    struct compute *computing =malloc (sizeof(struct compute) * opt.num_threads);
    for (int i = 0 ; i<opt.num_threads;i++){
        computing[i].params = malloc (sizeof (struct param));
        computing[i].params->in_q=&in_queue;
        computing[i].params->out_q=&out_queue;
        pthread_create(&computing[i].id, NULL, removeComputeInsert, computing[i].params);
    }

    //writing from out_q
    struct write* write = malloc(sizeof(struct write));
    write[0].wpar = malloc(sizeof(struct writePar));
    write[0].wpar->out_q = &out_queue;
    write[0].wpar->dir = opt.dir;
    write[0].wpar->file = opt.file;
    pthread_create(&write[0].id, NULL, writeOut, write[0].wpar);


    pthread_join(read[0].id,NULL);
    for (int i =0; i<opt.num_threads;i++)
        pthread_join(computing[i].id, NULL);
    pthread_join(write[0].id,NULL);

    q_destroy(in_queue);
    q_destroy(out_queue);

}

int main(int argc, char *argv[]) {
    struct options opt;
    opt.num_threads = 5;
    opt.queue_size  = 1000;
    opt.check       = true;
    opt.file        = NULL;
    opt.dir         = NULL;

    read_options (argc, argv, &opt);

    if(opt.check){
        check(opt);
    }
    else{
        sum(opt);
    }
    printf("OK\n");
}