// 刘昕垚 2200012866
// 对cache的模拟程序，具体实现见各个函数注释+函数内部注释

#include "cachelab.h"
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

unsigned long hit_count = 0, miss_count = 0, eviction_count = 0;
unsigned long S, B, s, E, b, v;

// h对应的帮助信息
void print_help() { 
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("    -h         Print this help message.\n");
    printf("    -v         Optional verbose flag.\n");
    printf("    -s <num>   Number of set index bits.\n");
    printf("    -E <num>   Number of lines per set.\n");
    printf("    -b <num>   Number of block offset bits.\n");
    printf("    -t <file>  Trace file.\n\n");
    printf("Examples:\n");
    printf(" linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf(" linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

struct Cache {
    unsigned long E, S, B;
    unsigned long*** c;
} cache;

// 对此次输入的缓存进行e、b、s的初始化
void init_cache(unsigned long _E, unsigned long _S, unsigned long _B) { 
    cache.E = _E;
    cache.S = _S;
    cache.B = _B;
}

// 对cache开辟空间并对相应的数据赋初值
void create_cache() { 

    // 一层层开辟空间
    cache.c = (unsigned long***)malloc(cache.S * sizeof(unsigned long**));

    for(int i = 0; i < cache.S; ++ i) 
        cache.c[i] = (unsigned long**)malloc(cache.E * sizeof(unsigned long*));

    for(int i = 0; i < cache.S; ++ i) 
        for(int j = 0; j < cache.E; ++ j)
            cache.c[i][j] = (unsigned long*)malloc(3 * sizeof(unsigned long));
    
    // 赋初值
    for(int i = 0; i < cache.S; ++ i) 
        for(int j = 0; j < cache.E; ++ j) {
            cache.c[i][j][0] = 0; // 有效位
            cache.c[i][j][1] = 0; // tag
            cache.c[i][j][2] = 0; // LRU
        }
}  

// 释放内存
void free_cache() {
    for(int i = 0; i < cache.S; ++ i) {
        for(int j = 0; j < cache.E; ++ j) {
            free(cache.c[i][j]);
        }
    }

    for(int i = 0; i < cache.S; ++ i) {
        free(cache.c[i]);
    }

    free(cache.c);
}

// LRU算法下应该被替换的行
int LRU_line(int se) { 
    int line = 0;
    int max = 0;
    for(int i = 0; i < E; ++ i) {
        if(cache.c[se][i][0] == 0)
            return i;
        if(cache.c[se][i][2] > max) {
            line = i;
            max = cache.c[se][i][2];
        }
    }
    return line;
}

// 除开本次替换的列其他都进行一次++，代表多了一次没用
void lru_change(int se, int line) { 
    for(int i = 0; i < E; ++ i) {
        if(i != line)
            ++cache.c[se][i][2];
    }
}

// 每一次输入新的地址，判断是否击中+更新缓存
void refresh(unsigned long add) { 
    int tag = add >> (b + s);
    int set = (add >> b) & ((1u << s) - 1);

    int replace_line = -1;

    // 判断是否hit
    for(int i = 0; i < E; ++ i) {
        if(cache.c[set][i][0] == 1 && cache.c[set][i][1] == tag) {
            replace_line = i;
            break;
        }
    }

    // hit
    if(replace_line != -1) {
        if(v)
            printf("hit");
        ++hit_count;
        cache.c[set][replace_line][2] = 0; // LRU
        lru_change(set, replace_line);
    }

    // miss
    else {
        ++miss_count;
        if(v)
            printf("miss ");
        int line = LRU_line(set);

        // 有效位为0时的miss
        if(cache.c[set][line][0] == 0) {
            cache.c[set][line][0] = 1; // 有效位
            cache.c[set][line][1] = tag; // tag
            cache.c[set][line][2] = 0; // LRU
        }

        // 用lru来替换
        else {
            if(v)
            printf("eviction");
            ++eviction_count;
            cache.c[set][line][1] = tag; // tag
            cache.c[set][line][2] = 0; // LRU
        }

        lru_change(set, line);
    }
    
    if(v)
        printf("\n");
}

int main(int argc, char * argv[]) {
    char opt;
    s = -1;
    E = -1;
    b = -1;
    FILE *path = NULL;

    // 对不同指令的不同操作
    while((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) { 
        switch(opt) {
            case 'h':
                print_help();
                return 0;
                break;
            case 'v':
                v = 1;
                break;
            case 's':
                s = atol(optarg);
                S = 1 << s;
                break;
            case 'E':
                E = atol(optarg);
                break;
            case 'b':
                b = atol(optarg);
                B = 1 << b;
                break;
            case 't':
                path = fopen(optarg,"r");
                break;
            default:
                print_help();
                return 0;
        }
    }

    // 不符合要求，直接退出
    if(s <= 0 || E <= 0 || b <= 0 || path == NULL) {
        print_help();
        return 0;
    }

    init_cache(E, S, B);
    create_cache();

    char operation[2];
    unsigned long address;
    char c;
    int size;

    // 输入检测
    while(fscanf(path, "%s%lx%c%d", operation, &address, &c, &size) != EOF) { 
        if(operation[0] == 'I') 
            continue;

        if(v) 
            printf("%c %lx%c%d: ", operation[0], address, c, size);
        
        refresh(address);
        // 是M的话多进行一次缓存更新
        if(operation[0] == 'M') {
            refresh(address);
        }
    }

    printSummary(hit_count, miss_count, eviction_count);
    free_cache();
    return 0;
}