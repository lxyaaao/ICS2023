/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k, l;
    int tmp1, tmp2, tmp3, tmp4;

    // 32 * 32
    // 进行8*8的划分（2^5=32=8*4，所以一行可以放8个int）
    // 若直接进行转置赋值，对角线依旧会产生miss和eviction，因此分类讨论
    if(M == 32 && N == 32) {
        for(i = 0; i < N; i += 8)
			for(j = 0; j < M; j += 8)
				for(k = i; k < i + 8; ++ k) {
					for (l = j; l < j + 8; ++ l) {
                        // 如果不在对角线上直接赋值
                        if(k != l)
    						B[l][k] = A[k][l];
                        // 若在对角线上，记录
                        else {
                            tmp1 = l;
                            tmp2 = A[k][l];
                        }
					}
                    // 只有i==j时会可能出现对角线，因此用此判断
                    if(i == j)
                        B[tmp1][tmp1] = tmp2;
                }
    }

    // 64 * 64
    // 先进行8*8的划分，再在8*8的划分里进行4*4的划分
    if(M == 64 && N == 64) {
        for(i = 0; i < N; i += 8)
			for(j = 0; j < M; j += 8) {
                // 先对左上角的分块进行转置，对角线处理同32 * 32
                // 再将原矩阵右上角调换到新矩阵的右上角作临时存储
				for(k = i; k < i + 4; ++ k) {
					for (l = j; l < j + 4; ++ l) {
                        if(k != l)
    						B[l][k] = A[k][l];
                        else {
                            tmp1 = l;
                            tmp2 = A[k][l];
                        }
                        B[l][k + 4] = A[k][l + 4];
					}
                    if(i == j)
                        B[tmp1][tmp1] = tmp2;
                }

                for (l = j; l < j + 4; ++l) {
                    // 临时变量来存储在第一次在新矩阵临时存储的右上角的数据
                    tmp1 = B[l][i + 4];
                    tmp2 = B[l][i + 5];
                    tmp3 = B[l][i + 6];
                    tmp4 = B[l][i + 7];

                    // 右上角的转置
                    B[l][i + 4] = A[i + 4][l];
                    B[l][i + 5] = A[i + 5][l];
                    B[l][i + 6] = A[i + 6][l];
                    B[l][i + 7] = A[i + 7][l];

                    // 将B右上角的调换到左下角
                    B[l + 4][i] = tmp1;
                    B[l + 4][i + 1] = tmp2;
                    B[l + 4][i + 2] = tmp3;
                    B[l + 4][i + 3] = tmp4;
                }

                // 对右下角块进行类似32 * 32的对对角线上及其他元素的操作
                for(k = i + 4; k < i + 8; ++ k) {
					for (l = j + 4; l < j + 8; ++ l) {
    					if(k != l)
    						B[l][k] = A[k][l];
                        else {
                            tmp1 = l;
                            tmp2 = A[k][l];
                        }
					}
                    if(i == j)
                        B[tmp1][tmp1] = tmp2;
                }
            }
    }

    // 60 * 68
    // 进行12 * 4的展开
    // 同时用临时变量存储，以减少miss
    if(M == 60 && N == 68) {
        for(i = 0; i < N; i += 12) {
            for(j = 0; j < M; j += 4) {
                for(k = i; k < i + 12; ++ k) {
                    // 判断是否越界
                    if(k >= N) 
                        break;
                    tmp1 = A[k][j];
                    if(j + 1 < M)
                        tmp2 = A[k][j + 1];
                    if(j + 2 < M)
                        tmp3 = A[k][j + 2];
                    if(j + 3 < M)
                        tmp4 = A[k][j + 3];
                    B[j][k] = tmp1;
                    if(j + 1 < M)
                        B[j + 1][k] = tmp2;
                    if(j + 2 < M)
                        B[j + 2][k] = tmp3;
                    if(j + 3 < M)
                        B[j + 3][k] = tmp4;
                }
            }
        }
    }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

 /*
  * trans - A simple baseline transpose function, not optimized for the cache.
  */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);

}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

