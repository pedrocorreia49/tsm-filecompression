#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Threads em windows e linux, tenho de ter algo a verificar o sistema operativo, e criar as threads conforme o sistema operativo atual ?

typedef struct Pair{
    unsigned char byte;
    unsigned char counter;
} pair;

typedef struct Works{
    int id;
    int size_in;
    int size_out;
    unsigned char *buffer_in;
    unsigned char *buffer_out;
    pair freqs[255];
    pthread_t tid;
    struct Works *next;
} work;

work *head = NULL;
work *tail = NULL;

FILE* fd;

pthread_mutex_t thr;

int turn = 1;
bool worthRLE = false;
bool debug = false;

char *inputFile;

clock_t tm;
double tmRLE;

work* newBlock(char* buff_in, int size, int id){
    work *temp = malloc(sizeof(work));
    temp->id = id;
    temp->size_in = size;
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

void printOut(work* w){
    for(int x=0; x<w->size_out; x++){
        printf("(%x)", w->buffer_out[x]);
    }
}

void printFreqs(work* w){
    unsigned char byte=0, val=0;
    pair p;


    for(unsigned char i = 0; i < 255; i++){ // Initialize field 'byte' of Pair freq
        w->freqs[i].byte = i;
    }

    printf("\n\n -----------------\n");
    printf("| Byte\t|  Times  |\n");
    printf("|-----------------|\n");
    for(unsigned char i = 0; i < 254; i++){
        byte = i;
        for(unsigned char j = i+1; j < 255; j++){
            if(w->freqs[j].counter > w->freqs[byte].counter){
                byte = j;
            }
        }
        p.byte = w->freqs[byte].byte;
        p.counter = w->freqs[byte].counter;

        w->freqs[byte] = w->freqs[i];
        w->freqs[i] = p;

        if(w->freqs[i].counter != 0)
            printf("| %x\t| %d times |\n", w->freqs[i].byte, w->freqs[i].counter);
    }
    if(w->freqs[254].counter != 0)
        printf("| %x\t| %d times |\n", w->freqs[254].byte, w->freqs[254].counter);

    printf(" -----------------\n");
}

void debugRLE(){
    work* aux = head;
    int nBLocks = 0, initialSize=0, endSize=0;
    int lastSize, secondLastSize;

    while(aux != NULL){
        nBLocks++;

        secondLastSize = lastSize;
        lastSize = aux->size_out;

        initialSize += aux->size_in;
        endSize += aux->size_out;

        aux = aux->next;
    }

    printf("-------------- DEBUG A --------------\n");
    printf("Coded by Pedro Correia, Universidade do Minho on 10/06/2021\n\n");
    printf("Input file: %s\n", inputFile);
    printf("Output file: %s.rle\n", inputFile);
    printf("Processed with RLE? ");
    if(worthRLE){
        printf("Yes\n");
    }else{
        printf("No\n");
    }

    if(nBLocks>1){
        printf("Processed blocks size (bytes): %d|%d\n", secondLastSize, lastSize);
    }else{
        printf("Processed block size (bytes): %d\n", lastSize);
    }

    printf("Number of processed blocks: %d\n", nBLocks);

    if(debug){
        if(worthRLE){
            if(head->size_out >= 160){
                printf("\nFirst 160 bytes of block 1:\n[");
                for(int i = 0; i < 160; i++){
                    printf(" %x", head->buffer_out[i]);
                }
                printf(" ]\n");
            }else{  // If size of less than 160, print only size
                printf("\nFirst %d bytes of block 1:\n[", head->size_out);
                for(int i = 0; i < head->size_out; i++){
                    printf(" %x", head->buffer_out[i]);
                }
                printf(" ]\n");
            }

            if(head->size_out >= 80){
                printf("\nLast 80 bytes of block 1:\n[");
                for(int i = (head->size_out)-80; i < head->size_out; i++){
                    printf(" %x", head->buffer_out[i]);
                }
                printf(" ]");
            }
        }
        
        printFreqs(head);
    }

    // IF RLE WAS NOT USED, END SIZE IS INITIALSIZE, OTHERWISE
    printf("\nInitial file size (bytes): %d\nFinal file size (bytes): %d\n", initialSize, endSize);

    float compression = (initialSize-endSize) / (float)initialSize;
    printf("Compression: %0.2f%%\n", compression*100);
    printf("A fase execution time: %.3fms", tmRLE);

    printf("\n------------ END DEBUG A ------------\n");
}

void writeProcessedBlock(work* w){
    if(w->id == 1){
        char f[strlen(inputFile)+4];
        strcpy(f, inputFile);
        strcat(f, ".rle");
        fd = fopen(f, "w");
    }

    if(worthRLE){
        fwrite(w->buffer_out, 1, w->size_out, fd);
    }else{
        fwrite(w->buffer_in, 1, w->size_in, fd);
    }

    if(w->next == NULL){ // Last block written, show log info
        fclose(fd);
        debugRLE();
    }
}

int writeRleSeq(char byte, int counter, unsigned char *buffer, int pos, pair *freqs){
    unsigned char *processed = malloc(head->size_in);
    int n;

    if(counter > 3 || byte == 0){       // Worth compress if it repeated at least 4 times
        processed[0] = 0;               // RLE indication symbol
        processed[1] = byte;            // Symbol of message
        n = 2;

        while(counter > 255){
            processed[n] = 0;           // For each '0', symbol appears 255 times
            n++;
            counter -= 255;
        }

        processed[n] = counter & 0xFF;      // Number of repetitions
        memcpy(buffer + pos, processed, n+1);

        freqs[0].counter += n-1;            // Number of times that RLE Seq symbol appears
        freqs[byte].counter++;              // Increment number of repetitions of this byte (symbol)
        freqs[counter & 0xFF].counter++;    // Increment number of repetitions of this byte (counter)

        free(processed);
        return n+1;                         // Return bytes written on output buffer
    }else{
        n = counter;
        while(counter != 0){
            memcpy(buffer + pos, &byte, 1);
            pos += 1;
            counter--;
        }

        freqs[byte].counter += n;           // Increment number of repetitions of this byte

        free(processed);
        return n;                           // Return bytes written on output buffer
    }
}

void rle(work* w){
    int pos = 0;
    int counter = 0;
    unsigned char byte = w->buffer_in[0];

    w->buffer_out = malloc(w->size_in * sizeof(char)); // Allocate space on output buffer

    if(w->size_in < 256){
        memcpy(w->buffer_out, w->buffer_in, w->size_in);
        for(int i = 0; i < w->size_in; i++){
            w->freqs[w->buffer_in[i]].counter++;
        }
        w->size_out = w->size_in;
        return;
    }

    for(int i = 0; i < w->size_in; i++){
        if(w->buffer_in[i] == byte){ // If is equals than the previous
            counter++;
        }else{
            pos += writeRleSeq(byte, counter, w->buffer_out, pos, w->freqs);
            byte = w->buffer_in[i];
            counter = 1;
        }

        if(i+1 == w->size_in){  // If this is the last byte
            pos += writeRleSeq(byte, counter, w->buffer_out, pos, w->freqs);  // Write last byte/sequence
        }
    }

    w->size_out = pos;  // pos has the output buffer size

    if(w->id == 1){     // Check if RLE was worth on first block
        float compression =  (w->size_in - w->size_out) / (float)w->size_in;
        if(compression >= 0.05){ // Compression higher than 5%, it's worth
            worthRLE = true;
        }
    }
}

void* processBlock(work* w){
    if(w->id == 1)
        tm = clock();   // Get time before starting RLE on first
    
    rle(w);
    if(w->next == NULL){
        tmRLE = ((double)(clock()-tm)/CLOCKS_PER_SEC)*1000;       
    }

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

    if(argc == 1){
        return -1;
    }else{
        for(i=1; i<argc; i++){
            if(argv[i][0] == '-'){
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
                            i++;
                        }else{
                            printf("Bad usage, aborting ...");
                            return -1;
                        }
                    case 'd':
                        debug=true;
                }
            }
        }
    }

    fd = fopen(argv[1], "r");
    if(fd < 0){
        perror("Error opening file");
        return -1;
    }

    inputFile = argv[1];

    // printf("Total number of cores: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));

    pthread_mutex_init(&thr, 0);
    char *buffer = malloc(bsize * sizeof(char));
    while(n = fread(buffer, sizeof(char), bsize, fd)){
        work *w = newBlock(buffer, n, id);
        pthread_create(&(w->tid), NULL, (void *)&processBlock, w);
        id++;
    }
    fclose(fd);

    work *aux = head;
    while(aux!=NULL){
        pthread_join(aux->tid, NULL);
        aux=aux->next;
    }

    return 0;
}


// THREADS WINDOWS E UNIX
// https://www.embeddedcomputing.com/technology/software-and-os/thread-synchronization-in-linux-and-windows-systems-part-1