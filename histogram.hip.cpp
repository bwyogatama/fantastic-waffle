#include <math.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <hip/hip_runtime.h>
#include "histogram.h"
using namespace std;

#define BLOCK_SIZE 128
#define NUM_BINS 128
#define MAX_VAL 127
#define MAX_GPU_COUNT 7
#define INPUT_LENGTH 128

// histogramCPU computes the histogram of an input array on the CPU
void histogramCPU(unsigned int* input, unsigned int* bins, unsigned int numElems) {
    for (int i=0; i<numElems; i++) {
        bins[input[i]]++;
    }
}

// histogramGPU computes the histogram of an input array on the GPU
__global__ void histogramGPU(unsigned int* input, unsigned int* bins, unsigned int numElems) {
    int threadN = hipGridDim_x * hipBlockDim_x;

    // compute global thread coordinates
    int tx = (hipBlockIdx_x * hipBlockDim_x) + hipThreadIdx_x;

    // update private histogram
    for (int pos = tx; pos < numElems; pos += threadN) { 
        //bins[input[pos]]++;
        atomicAdd(&(bins[input[pos]]), 1);
    }

}

int main(void) {
    // data params
    TGPUplan plan[MAX_GPU_COUNT];
    int GPU_N, i, j, gpuBase;
    unsigned int* hostBins_CPU;
    unsigned int* hostInput; unsigned int* hostBins;

    // determine
    size_t histoSize = NUM_BINS * sizeof(unsigned int);
    size_t inSize = INPUT_LENGTH * sizeof(unsigned int);

    // allocate host memory
    hostInput = (unsigned int*)malloc(inSize);
    hostBins = (unsigned int*)malloc(histoSize);
    hostBins_CPU = (unsigned int*)malloc(histoSize);

    printf("Starting histogram\n");
    hipGetDeviceCount(&GPU_N);

    if (GPU_N > MAX_GPU_COUNT) {
        GPU_N = MAX_GPU_COUNT;
    }

    printf("CUDA-capable device count: %i\n", GPU_N);

    for (i=0; i < GPU_N; i++) {
        plan[i].dataN = INPUT_LENGTH/GPU_N;
        //plan[i].dataN = INPUT_LENGTH;
    }

    for (i=0; i<INPUT_LENGTH; i++) {
        hostInput[i] = i;
    }

    //for (i=0; i<INPUT_LENGTH/GPU_N; i++) {
    //   for (j=0; j<GPU_N; j++)
    //        hostInput[i+INPUT_LENGTH/GPU_N*j] = i;
    //}

    for (i=0; i<NUM_BINS; i++) {
        hostBins[i] = 0;
    }

    // allocate device memory
    for (i=0; i < GPU_N; i++) {
        hipSetDevice(i);
        hipStreamCreate(&plan[i].stream);
        plan[i].input_h = (unsigned int*) malloc(plan[i].dataN*sizeof(unsigned int));
        for (j=0; j < plan[i].dataN; j++) {
            plan[i].input_h[j] = hostInput[j+(plan[i].dataN*i)];
            //plan[i].input_h[j] = hostInput[j];
            //printf("plan[%d].input_h[%d] = %d\n", i, j, plan[i].input_h[j]);
       }
    }


    // kernel launch
    for (i=0; i < GPU_N; i++) {
        hipSetDevice(i);
        dim3 threadPerBlock(BLOCK_SIZE, 1, 1);
        //dim3 threadPerBlock(1, 1, 1);
        dim3 blockPerGrid(ceil(plan[i].dataN/(float)BLOCK_SIZE), 1, 1);
        //dim3 blockPerGrid(1, 1, 1);
        //printf("plan[%d].dataN = %d\n", i, plan[i].dataN);
        hipLaunchKernelGGL(HIP_KERNEL_NAME(histogramGPU), dim3(blockPerGrid), dim3(threadPerBlock), 0, plan[i].stream, plan[i].input_h, hostBins, plan[i].dataN);
    }

    
    for (i=0; i < GPU_N; i++) {
        hipSetDevice(i);
        hipStreamSynchronize(plan[i].stream);
        //for (j=0; j < NUM_BINS; j++) {
        //    hostBins[j]+= plan[i].bins_h[j];
        //}
        hipStreamDestroy(plan[i].stream);
    }
    

    // initialize CPU histogram array to 0
    for (int i=0; i<NUM_BINS; i++) {
        hostBins_CPU[i] = 0;
    }

    // run the CPU version
    histogramCPU(hostInput, hostBins_CPU, INPUT_LENGTH);

    for (int i=0; i<NUM_BINS; i++) {
        if (abs(int(hostBins_CPU[i] - hostBins[i])) > 0) {
            fprintf(stderr, "Result verification failed at element (%d)!\n", i);
            printf("CPU: %d\n", hostBins_CPU[i]);
            printf("GPU: %d\n", hostBins[i]);
            exit(1);
        }
    }
    printf("Test PASSED\n");


    for (i=0; i<GPU_N; i++) {
          hipSetDevice(i);
          free(plan[i].input_h);
          hipDeviceReset();
    }

    // release resources
    free(hostBins); free(hostBins_CPU); free(hostInput);
    printf("end\n");
    return 0;
}
