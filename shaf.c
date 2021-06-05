#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

// Há problema em esperas ativas para ter as threads a escrever de forma sequencial ?

// Threads em windows e linux, tenho de ter algo a verificar o sistema operativo, e criar as threads conforme o sistema operativo atual ?



typedef struct Works{
    int id;
    int size;
    unsigned char *buffer_in;
    unsigned char *buffer_out;
    pthread_t tid;
    struct Works *next;
} work;

work *head = NULL;
work *tail = NULL;

pthread_mutex_t thr;

int turn = 1;

work* newBlock(char* buff_in, int size, int id){
    work *temp = malloc(sizeof(work));
    temp->id = id;
    temp->size = size;
    temp->buffer_in = malloc(size * sizeof(char));
    memcpy(temp->buffer_in, buff_in, size);

    if(head == NULL){       // If list is empty
        head = temp;            // This node is the head
        tail = temp;            // This node is the tail aswell
    }else{                  // If list is not empty
        tail->next = temp;      // Add to the end of the list
        tail = temp;            // Keep a pointer to the tail to faster access
    }

    return temp;
}

void writeProcessedBlock(work* w){
    // printf("id: %d\n", w->id);
}

void rle(work* w){
    int pos = 0;
    int counter = 0;
    char byte = w->buffer_in[0];
    char processed[50] = {0};
    int n;

    w->buffer_out = malloc(w->size * sizeof(char)); // Allocate space on output buffer

    for(int i = 0; i < w->size; i++){
        if(w->buffer_in[i] == byte){ // If is equals than the previous
            counter++;
        }else{  // If it is different from the previous
            if(counter > 3 || byte == '0'){   // Worth compress if it repeated at least 4 times
                if(counter < 256){
                    n = sprintf(processed, "0%c%d", byte, counter);
                }else{  // If there are at least 256 equal bytes
                    n = sprintf(processed, "0%c", byte);
                    while(counter > 255){
                        n += sprintf(processed+n, "0");
                        counter -= 255;
                    }
                    n += sprintf(processed+n, "%d", counter);
                }
                memcpy(w->buffer_out + pos, processed, n);
                pos += n;
            }else{
                while(counter != 0){
                    memcpy(w->buffer_out + pos, &byte, 1);
                    pos += 1;
                    counter--;
                }
            }
            counter = 1;
            byte = w->buffer_in[i];
        }
    }

    for(int x=0; x<pos; x++){
        printf("%c ", w->buffer_out[x]);
    }
    printf("\nend\n");
}

void* processBlock(work* w){ // sq mudar o nome para workThreadHandler
    w->tid = pthread_self();    // Store thread id on struct to join thread later
    rle(w);
    while(1){
        if(w->id == turn){
            pthread_mutex_lock(&thr);
            writeProcessedBlock(w); // Por agora escrevo o bloco não processado
            turn++;
            pthread_mutex_unlock(&thr);
            pthread_exit(NULL);
        }
    }
}

int main(int argc, char *argv[]){
    int i, n;
    int id = 1;
    int bsize = 64000;
    FILE* fd;
    pthread_t tid;

    if(argc == 1){
        return -1;
    }else{
        for(i=1; i<argc; i++){
            if(argv[i][0] == '-'){
                printf("-%c %s\n", argv[i][1], argv[i+1]);
                switch(argv[i][1]){
                    case 'S':
                        if(argv[i+1]){
                            bsize = atoi(argv[i+1]) * 1000;
                            if(bsize<64000){ // If less than 64Kbytes, make it 64Kbytes 
                                bsize=64000;
                                printf("Buffer Size is now 64Kbytes (minimum value)\n");
                            }
                            if(bsize>64000000){ // If more than 64Mbytes, make it 64Mbytes
                                bsize=64000000;
                                printf("Buffer Size is now 64Mbytes (maximum value)\n");
                            }
                        }else{
                            printf("Bad usage, aborting ...");
                            return -1;
                        }
                }
            }
        }
    }

    fd = fopen(argv[1], "r");
    if(fd < 0){
        perror("Error opening file");
        return -1;
    }

    // printf("Total number of cores: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));

    pthread_mutex_init(&thr, 0);
    char *buffer = malloc(bsize * sizeof(char));
    while(n = fread(buffer, sizeof(char), bsize, fd)){
        // printf("N = %d\n", n);
        // printf("[%d]: %c%c%c \n", n, buffer[0], buffer[1], buffer[2]);
        work *w = newBlock(buffer, n, id);
        pthread_create(&tid, NULL, (void *)&processBlock, w);
        id++;
    }

    work *aux = head;

    // SQ NAO ESTA A DAR JOIN!!!!!!!!!! VERIFICAR
    while(aux!=NULL){
        pthread_join(aux->tid, NULL);
        aux=aux->next;
        sleep(1);
    }

    return 0;
}


// THREADS WINDOWS E UNIX
// https://www.embeddedcomputing.com/technology/software-and-os/thread-synchronization-in-linux-and-windows-systems-part-1