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
    unsigned char lenght;
    int counter;
    int code;
} pair;

typedef struct Works{
    int id;
    int size_in;
    int size_out;
    int lastSize;
    unsigned char *buffer_in;
    unsigned char *buffer_out;
    unsigned char *binaryBuffer;
    pair freqs[256];
    pthread_t tid;
    struct Works *next;
} work;

work *head = NULL;
work *tail = NULL;

FILE* fd;

pthread_mutex_t thr;

int turn = 1;

bool worthRLE = false;
bool firstEndedRLE = false;
bool debug = false;

bool faseA = false;
bool faseB = false;
bool faseC = true;

char *inputFile;

clock_t tm, initB, initC;
double tmRLE;
double tmSF;
double tmC;


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

void orderFreqs(work* w){
    unsigned char byte=0, val=0;
    pair p = {0};

    for(int i = 0; i < 256; i++){ // Initialize field 'byte' of Pair freq
        w->freqs[i].byte = i;
    }

    for(int i = 0; i < 255; i++){
        byte = i;
        for(int j = i+1; j < 256; j++){
            if(w->freqs[j].counter > w->freqs[byte].counter){
                byte = j;
            }
        }
        p.byte = w->freqs[byte].byte;
        p.counter = w->freqs[byte].counter;

        w->freqs[byte] = w->freqs[i];
        w->freqs[i] = p;
    }
}

void freqsIn(work* w){  // Count frequency of symbols of buffer_in
    memset(w->freqs, 0, 256*sizeof(pair)); // Clear array of frequencys

    for(int i = 0; i < w->size_in; i++){
        w->freqs[w->buffer_in[i]].counter++;
    }
    orderFreqs(w);
}

void printFreqs(work* w){
    printf("\n\n -----------------------\n");
    printf("| Byte\t|    Times\t|\n");
    printf("|-----------------------|\n");
    for(int i = 0; i < 256; i++){
        if(w->freqs[i].counter != 0)
        printf("| %d\t| %d\t\t|\n", w->freqs[i].byte, w->freqs[i].counter);
    }
    printf(" -----------------------\n");
}

void printBin(int code, unsigned char lenght){
    for(int i=0; i<lenght; i++){
        printf("%d", (code >> (lenght-i-1)) & 1);
    }
}

void printSFTableSync(work* w){
    while(w->id != turn){}; // Block thread until it reaches it's turn

    printf("\n -------------------------------\n");
    printf("|           Block %d             |", w->id);
    printf("\n|-------------------------------|\n");
    if(w->size_out < 256){
        printf("|    No codes (size < 256)      |\n");
    }else{
        printf("| Byte\t| Frequency\t| Code\t|\n");
        printf("|-------------------------------|\n");
        for(int i = 0; i < 256; i++){
            if(w->freqs[i].counter != 0){
                printf("| %d\t| %d\t\t| ", w->freqs[i].byte, w->freqs[i].counter);
                printBin(w->freqs[i].code, w->freqs[i].lenght);
                printf("\t|\n");
            }
        }
    }
    printf(" -------------------------------\n");
    turn++;
}

void printSingleSFTable(work* w){
    printf("\n -------------------------------\n");
    printf("|           Block %d             |", w->id);
    printf("\n|-------------------------------|\n");
    if(w->size_out < 256){
        printf("|    No codes (size < 256)      |\n");
    }else{
        printf("| Byte\t| Frequency\t| Code\t|\n");
        printf("|-------------------------------|\n");
        for(int i = 0; i < 256; i++){
            if(w->freqs[i].counter != 0){
                printf("| %d\t| %d\t\t| ", w->freqs[i].byte, w->freqs[i].counter);
                for(int j=0; j<w->freqs[i].lenght; j++){
                    printf("%d", (w->freqs[i].code >> (w->freqs[i].lenght-j-1)) & 1);
                }
                printf("\t|\n");
            }
        }
    }
    printf(" -------------------------------\n");
}

void debugFun(){
    work* aux = head;
    int nBLocks = 0, initialSize=0, endSizeB=0, endSizeC=0;
    int lastSize, secondLastSize;
    float compression;

    while(aux != NULL){
        nBLocks++;

        secondLastSize = lastSize;
        lastSize = aux->size_in;

        initialSize += aux->size_in;
        endSizeB += aux->size_out;
        endSizeC += aux->lastSize;

        aux = aux->next;
    }

    printf("\n-------------------------- DEBUG --------------------------\n");
    printf("* Coded by Pedro Correia, Universidade do Minho on 10/06/2021\n\n");
    printf("* Input file: %s\n", inputFile);

    if(faseA){
        printf("* Output file: %s.rle\n", inputFile);
    }else if(faseB){
        printf("* Output file: No Output\n");
    }else if(faseC){
        printf("* Output file: %s.shaf\n", inputFile);
    }

    printf("* Processed with RLE? ");
    if(worthRLE){
        printf("Yes\n");
    }else{
        printf("No\n");
    }

    if(nBLocks>1){
        printf("* Processed blocks size (ALL|LAST) (bytes): %d|%d\n", secondLastSize, lastSize);
    }else{
        printf("* Processed block size (bytes): %d\n", lastSize);
    }

    printf("* Number of processed blocks: %d\n", nBLocks);

    if(debug){
        if(worthRLE){
            if(head->size_out >= 160){
                printf("\n* First 160 bytes of block 1\t|\n");
                for(int i = 0; i < 160; i++){
                    printf("%d ", head->buffer_out[i]);
                    if((i+1) % 8 == 0 && i != 0)
                        printf("\t|\n");
                }
            }else{  // If size of less than 160, print only size
                printf("\n* First %d bytes of block 1\t|\n", head->size_out);
                for(int i = 0; i < head->size_out; i++){
                    printf("%d ", head->buffer_out[i]);
                    if((i+1) % 8 == 0 && i != 0)
                        printf("\t|\n");
                }
            }
            printf("--------------------------------\n");

            if(head->size_out >= 80){
                printf("\n* Last 80 bytes of block 1\t|\n");
                for(int i = (head->size_out)-80, x=0; i < head->size_out; i++, x++){
                    printf("%d ", head->buffer_out[i]);
                    if((x+1) % 8 == 0 && x != 0)
                        printf("\t|\n");
                }
                printf("--------------------------------\n");
            }
        }

        if(faseA){
            printFreqs(head);
        }else if(faseB | faseC){
            printSingleSFTable(head); 
        }

        if(faseC){
            pair freqs[256];
            for(int i = 0; i < 256; i++){
                freqs[head->freqs[i].byte].code = head->freqs[i].code;
                freqs[head->freqs[i].byte].lenght = head->freqs[i].lenght;
            }

            if(head->size_out >= 160){
                printf("\n* First 160 bytes of block 1 (Binary Sequence):\n");
                for(int i = 0; i < 160; i++){
                    printBin(freqs[head->buffer_out[i]].code, freqs[head->buffer_out[i]].lenght);
                    printf(".");
                    if((i+1) % 8 == 0 && i != 0)
                        printf("\t|\n");
                }
            }else{  // If size of less than 160, print only size
                printf("\n* First %d bytes of block 1 (Binary Sequence):\n", head->size_out);
                for(int i = 0; i < head->size_out; i++){
                    printf("%d ", head->buffer_out[i]);
                    if((i+1) % 8 == 0 && i != 0)
                        printf("\t|\n");
                }
            }
            printf("--------------------------------\n");
        }
    }

    printf("* Initial file size (bytes): %d\n", initialSize);
    if(faseA){
        printf("* Final file size (bytes): %d\n", endSizeB);
    }else if(faseC){
        printf("* Final file size (bytes): %d\n", endSizeC);
    }
    
    if(faseA){
        compression = (initialSize-endSizeB) / (float)initialSize;
        printf("* Compression: %0.2f%%\n", compression*100);
    }else{
        compression = (initialSize-endSizeC) / (float)initialSize;
        printf("* Compression: %0.2f%%\n", compression*100);
    }
    printf("* A fase execution time: %.3fms\n", tmRLE);
    if(faseB || faseC){
        printf("* B fase execution time: %.3fms\n", tmSF);
    }
    if(faseC){
        printf("* C fase execution time: %.3fms\n", tmC);
    }

    printf("---------------------- END DEBUG --------------------------\n");
}

void writeRLEBlock(work* w){
    while(w->id != turn){}; // Block thread until it reaches it's turn

    if(w->id == 1){
        char f[strlen(inputFile)+4];
        strcpy(f, inputFile);
        strcat(f, ".rle");
        fd = fopen(f, "w");
    }

    fwrite(w->buffer_out, 1, w->size_out, fd);
    turn++; // Make the next block leave while cycle

    if(w->next == NULL){ // Last block written, show log info
        tmRLE = ((double)(clock()-tm)/CLOCKS_PER_SEC)*1000; // Get end of A phase execution time
        fclose(fd);
        debugFun();
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
        orderFreqs(w);
        if(w->id == 1){
            firstEndedRLE = true;
        }
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
    orderFreqs(w);

    if(w->id == 1){     // Check if RLE was worth on first block
        float compression =  (w->size_in - w->size_out) / (float)w->size_in;
        if(compression >= 0.05){ // Compression higher than 5%, it's worth
            worthRLE = true;
        }
        firstEndedRLE = true;
    }
}

int bestDiv(int start, int end, work* w){
    int up=start, down=end;
    int sumUp = w->freqs[up].counter;
    int sumDown = w->freqs[down].counter;

    while((down-up) != 1){
        if(sumUp < sumDown){
            up++;
            sumUp += w->freqs[up].counter;
        }else{
            down--;
            sumDown += w->freqs[down].counter;
        }
    }
    return up; 
}

void addBit(bool bit, work* w, int start, int end){
    int byte = start;
    while(byte <= end){
        if(w->freqs[byte].lenght < 32){
            if(bit){
                w->freqs[byte].code = (w->freqs[byte].code << 1) | 1;
            }else{
                w->freqs[byte].code = (w->freqs[byte].code << 1);
            }
            w->freqs[byte].lenght++;
        }else{
            printf("NAO PODE\n");
            // TEM DE TERMINAR, CÓDIGO ULTRAPASSA 32 bits, o inteiro nao suporta
        }
        byte++;
    }
}

void shannonCode(int start, int end, work* w){
    int mid;

    while(w->freqs[end].counter == 0){
        end--;
    }
    if(start != end){
        mid = bestDiv(start, end, w);
        addBit(false, w, start, mid);
        addBit(true, w, mid+1, end);
        shannonCode(start, mid, w);
        shannonCode(mid+1, end, w);
    }
}

void shannonFano(work* w){
    if(w->size_out >= 256){
        shannonCode(0, 254, w);
    }
}

int mask(int num){
    switch (num){
        case 1:
            return 0x1;
        case 2:
            return 0x3;
        case 3:
            return 0x7;
        case 4:
            return 0xF;
        case 5:
            return 0x1F;
        case 6:
            return 0x3F;
        case 7:
            return 0x7F;
        case 8:
            return 0xFF;
    }
    return 0;
}

void print_binary(unsigned char c)
{
 unsigned char i1 = (1 << (sizeof(c)*8-1));
 for(; i1; i1 >>= 1)
      printf("%d",(c&i1)!=0);
}

int processBinaryBuffer(work* w){
    pair freqs[256];
    int pos=0;
    int open=8;
    int len, aux;
    unsigned char cod;

    w->binaryBuffer = malloc(w->size_out * sizeof(char)); // Allocate space on binary buffer
    memset(w->binaryBuffer, 0, w->size_out);
    
    for(int i = 0; i < 256; i++){
        freqs[w->freqs[i].byte].code = w->freqs[i].code;
        freqs[w->freqs[i].byte].lenght = w->freqs[i].lenght;
    }

    for(int i = 0; i < w->size_out; i++){
        if(freqs[w->buffer_out[i]].lenght <= open){
            w->binaryBuffer[pos] =  (w->binaryBuffer[pos] | (freqs[w->buffer_out[i]].code << (open-freqs[w->buffer_out[i]].lenght)));
            open -= freqs[w->buffer_out[i]].lenght;
        }else{
            cod = freqs[w->buffer_out[i]].code;
            len = freqs[w->buffer_out[i]].lenght;
            while(len >= open){
                aux = (cod >> (len-open)) & mask(open);
                w->binaryBuffer[pos] = (w->binaryBuffer[pos] | aux);
                len = len-open;
                open = 8;
                pos++;
            }
            if(len > 0){ // If there's code to write (it's less than open)
                aux = ((cod & mask(len)) << (open-len));
                w->binaryBuffer[pos] = (w->binaryBuffer[pos] | aux);
                open = open-len;
            }
        }
        
        if(open == 0){
            open = 8;
            pos++;
        }
    }

    return pos;
}

void writeBinaryBuffer(work* w){
    unsigned char rle;
    int freqs[256];
    unsigned char header[1029]; // 261 is the max size of header (rle(1) + size(4) + sfCodes(256*4))
    int headerSize = 0;

    while(w->id != turn){}; // Block thread until it reaches it's turn

    if(worthRLE && w->size_in >= 256){
        rle = 0xFF;
    }else{
        rle = 0;
    }

    if(w->size_out >= 256) // If this is not true, we don't have SF codes
    for(int i = 0; i < 256; i++){ // Order freqs by symbol
        freqs[w->freqs[i].byte] = w->freqs[i].counter;
    }

    header[0] = rle;
    header[1] = (w->size_out >> 24) & 0xFF;
    header[2] = (w->size_out >> 16) & 0xFF;
    header[3] = (w->size_out >> 8) & 0xFF;
    header[4] = (w->size_out) & 0xFF;
    if(w->size_out >= 256){
        memcpy(header+5, freqs, sizeof(int)*256);
        headerSize = 1029;
    }else{
        headerSize = 5;
    }

    if(w->id == 1){
        char f[strlen(inputFile)+5];
        strcpy(f, inputFile);
        strcat(f, ".shaf");
        fd = fopen(f, "w");
    }

    fwrite(header, 1, headerSize, fd);              // Write header

    // printf("[");
    // for(int i = 0; i<headerSize; i++){
    //     printf("%d ", header[i]);
    // }
    // printf("]\n");
    
    fwrite(w->binaryBuffer, 1, w->lastSize, fd);    // Write Payload

    turn++; // Make the next block leave while cycle

    if(w->next == NULL){ // Last block written, show log info
        tmC = ((double)(clock()-initC)/CLOCKS_PER_SEC)*1000; // Get finish time of SF phase
        fclose(fd);
        debugFun();
    }
}

void* processBlock(work* w){
    if(w->id == 1)
        tm = clock();       // Get time before starting RLE on first block
    
    rle(w);

    while(!firstEndedRLE){} // Wait until first block end RLE to check if we can keep going
    if(!worthRLE){          // First already ended, and is not worth the rle, we need to recompute frequencys to buffer in
        freqsIn(w);
        memcpy(w->buffer_out, w->buffer_in, w->size_in);    // RLE not worth on block 1, so every buffer_out will be buffer_in
        w->size_out = w->size_in;                           // And size_out will be the size_in
    }

    if(faseA){
        writeRLEBlock(w);
        pthread_exit(NULL);
    }else{
        if(w->next == NULL){
            tmRLE = ((double)(clock()-tm)/CLOCKS_PER_SEC)*1000; // Get end of A phase execution time
        }
    }

    if(w->id == 1){
        initB = clock();       // Get time before starting SF on first block
    }
    shannonFano(w);
    if(faseB){
        printSFTableSync(w);
        if(w->next == NULL){ // Last block codes printed, show log info
            tmSF = ((double)(clock()-initB)/CLOCKS_PER_SEC)*1000; // Get finish time of SF phase
            debugFun();
        }
        pthread_exit(NULL);
    }else{
        if(w->next == NULL){
            tmSF = ((double)(clock()-initB)/CLOCKS_PER_SEC)*1000; // Get finish time of SF phase
        }
    }

    if(w->id == 1){
        initC = clock();       // Get time before starting SF on first block
    }
    w->lastSize = processBinaryBuffer(w);
    writeBinaryBuffer(w);

    if(w->next == NULL){
        pthread_exit(NULL);
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
                        break;
                    case 'A':
                        faseA = true;
                        faseB = false;
                        faseC = false;
                        break;
                    case 'B':
                        faseA = false;
                        faseB = true;
                        faseC = false;
                        break;
                    case 'd':
                        debug=true;
                        break;
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