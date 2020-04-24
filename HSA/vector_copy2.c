/* Copyright 2014 HSA Foundation Inc.  All Rights Reserved.
 *
 * HSAF is granting you permission to use this software and documentation (if
 * any) (collectively, the "Materials") pursuant to the terms and conditions
 * of the Software License Agreement included with the Materials.  If you do
 * not have a copy of the Software License Agreement, contact the  HSA Foundation for a copy.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "/p/hal/private/rocm/hsa/include/hsa/hsa.h"
#include "/p/hal/private/rocm/hsa/include/hsa/hsa_ext_finalize.h"

/*
#define check(msg, status) \
if (status != HSA_STATUS_SUCCESS) { \
    printf("%s failed.\n", #msg); \
    exit(1); \
} else { \
   printf("%s succeeded.\n", #msg); \
}
*/

/*
 * Loads a BRIG module from a specified file. This
 * function does not validate the module.
 */
int load_module_from_file(const char* file_name, hsa_ext_module_t* module) {
    int rc = -1;

    FILE *fp = fopen(file_name, "rb");

    rc = fseek(fp, 0, SEEK_END);

    size_t file_size = (size_t) (ftell(fp) * sizeof(char));

    rc = fseek(fp, 0, SEEK_SET);

    char* buf = (char*) malloc(file_size);

    memset(buf,0,file_size);

    size_t read_size = fread(buf,sizeof(char),file_size,fp);

    if(read_size != file_size) {
        free(buf);
    } else {
        rc = 0;
        *module = (hsa_ext_module_t) buf;
    }

    fclose(fp);

    return rc;
}

/*
 * Determines if the given agent is of type HSA_DEVICE_TYPE_GPU
 * and sets the value of data to the agent handle if it is.
 */
static hsa_status_t get_gpu_agent1(hsa_agent_t agent, void *data) {
    hsa_status_t status;
    hsa_device_type_t device_type;
    status = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
    if (HSA_STATUS_SUCCESS == status && HSA_DEVICE_TYPE_GPU == device_type) {
        hsa_agent_t* ret = (hsa_agent_t*)data;
        *ret = agent;
        return HSA_STATUS_INFO_BREAK;
    }
    return HSA_STATUS_SUCCESS;
}

/*
 * Determines if the given agent is of type HSA_DEVICE_TYPE_GPU
 * and sets the value of data to the agent handle if it is.
 */
static hsa_status_t get_gpu_agent2(hsa_agent_t agent, void *data) {
    hsa_status_t status;
    hsa_device_type_t device_type;
    status = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
    if (HSA_STATUS_SUCCESS == status && HSA_DEVICE_TYPE_GPU == device_type) {
        hsa_agent_t* ret = (hsa_agent_t*)data;
        *ret = agent;
        //return HSA_STATUS_INFO_BREAK;
    }
    return HSA_STATUS_SUCCESS;
}

/*
 * Determines if a memory region can be used for kernarg
 * allocations.
 */
static hsa_status_t get_kernarg_memory_region(hsa_region_t region, void* data) {
    hsa_region_segment_t segment;
    hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &segment);
    if (HSA_REGION_SEGMENT_GLOBAL != segment) {
        return HSA_STATUS_SUCCESS;
    }

    hsa_region_global_flag_t flags;
    hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &flags);
    if (flags & HSA_REGION_GLOBAL_FLAG_KERNARG) {
        hsa_region_t* ret = (hsa_region_t*) data;
        *ret = region;
        return HSA_STATUS_INFO_BREAK;
    }

    return HSA_STATUS_SUCCESS;
}

int main(int argc, char **argv) {
    hsa_status_t err;

    err = hsa_init();
    //check(Initializing the hsa runtime, err);

    /*
     * Determine if the finalizer 1.0 extension is supported.
     */
    bool support;

    err = hsa_system_extension_supported(HSA_EXTENSION_FINALIZER, 1, 0, &support);

    //check(Checking finalizer 1.0 extension support, err);

    /*
     * Generate the finalizer function table.
     */
    hsa_ext_finalizer_1_00_pfn_t table_1_00;

    err = hsa_system_get_extension_table(HSA_EXTENSION_FINALIZER, 1, 0, &table_1_00);

    //check(Generating function table for finalizer, err);

    /* 
     * Iterate over the agents and pick the gpu agent using 
     * the get_gpu_agent callback.
     */
    hsa_agent_t agent1;
    hsa_agent_t agent2;
    err = hsa_iterate_agents(get_gpu_agent1, &agent1);
    err = hsa_iterate_agents(get_gpu_agent2, &agent2);
    if(err == HSA_STATUS_INFO_BREAK) { err = HSA_STATUS_SUCCESS; }
    //check(Getting a gpu agent, err);

    /*
     * Query the name of the agent.
     */
    char name1[64] = { 0 };
    char name2[64] = { 0 };
    err = hsa_agent_get_info(agent1, HSA_AGENT_INFO_NAME, name1);
    err = hsa_agent_get_info(agent2, HSA_AGENT_INFO_NAME, name2);
    //check(Querying the agent name, err);
    printf("The agent1 name is %s.\n", name1);
    printf("The agent2 name is %s.\n", name2);

    /*
     * Query the maximum size of the queue.
     */
    uint32_t queue_size1 = 0;
    uint32_t queue_size2 = 0;
    err = hsa_agent_get_info(agent1, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size1);
    err = hsa_agent_get_info(agent2, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size2);
    //check(Querying the agent maximum queue size, err);
    printf("The maximum queue size of agent1 is %u.\n", (unsigned int) queue_size1);
    printf("The maximum queue size of agent2 is %u.\n", (unsigned int) queue_size2);


    /*
     * Create a queue using the maximum size.
     */
    hsa_queue_t* queue1;
    hsa_queue_t* queue2; 
    err = hsa_queue_create(agent1, queue_size1, HSA_QUEUE_TYPE_SINGLE, NULL, NULL, UINT32_MAX, UINT32_MAX, &queue1);
    err = hsa_queue_create(agent2, queue_size2, HSA_QUEUE_TYPE_SINGLE, NULL, NULL, UINT32_MAX, UINT32_MAX, &queue2);
    //check(Creating the queue, err);

    /*
     * Load the BRIG binary.
     */
    hsa_ext_module_t module;
    load_module_from_file(argv[1],&module);

    /*
     * Create hsa program.
     */
    hsa_ext_program_t program;
    memset(&program,0,sizeof(hsa_ext_program_t));
    err = table_1_00.hsa_ext_program_create(HSA_MACHINE_MODEL_LARGE, HSA_PROFILE_FULL, HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT, NULL, &program);
    //check(Create the program, err);

    /*
     * Add the BRIG module to hsa program.
     */
    err = table_1_00.hsa_ext_program_add_module(program, module);
    //check(Adding the brig module to the program, err);

    /*
     * Determine the agents ISA.
     */
    hsa_isa_t isa1;
    hsa_isa_t isa2;
    err = hsa_agent_get_info(agent1, HSA_AGENT_INFO_ISA, &isa1);
    err = hsa_agent_get_info(agent2, HSA_AGENT_INFO_ISA, &isa2);
    //check(Query the agents isa, err);

    /*
     * Finalize the program and extract the code object.
     */
    hsa_ext_control_directives_t control_directives1;
    hsa_ext_control_directives_t control_directives2;
    memset(&control_directives1, 0, sizeof(hsa_ext_control_directives_t));
    memset(&control_directives2, 0, sizeof(hsa_ext_control_directives_t));
    hsa_code_object_t code_object1;
    hsa_code_object_t code_object2;
    err = table_1_00.hsa_ext_program_finalize(program, isa1, 0, control_directives1, "", HSA_CODE_OBJECT_TYPE_PROGRAM, &code_object1);
    err = table_1_00.hsa_ext_program_finalize(program, isa2, 0, control_directives2, "", HSA_CODE_OBJECT_TYPE_PROGRAM, &code_object2);
    //check(Finalizing the program, err);

    /*
     * Destroy the program, it is no longer needed.
     */
    err=table_1_00.hsa_ext_program_destroy(program);
    //check(Destroying the program, err);

    /*
     * Create the empty executable.
     */
    hsa_executable_t executable1;
    hsa_executable_t executable2;
    err = hsa_executable_create(HSA_PROFILE_FULL, HSA_EXECUTABLE_STATE_UNFROZEN, "", &executable1);
    err = hsa_executable_create(HSA_PROFILE_FULL, HSA_EXECUTABLE_STATE_UNFROZEN, "", &executable2);
    //check(Create the executable, err);

    /*
     * Load the code object.
     */
    err = hsa_executable_load_code_object(executable1, agent1, code_object1, "");
    err = hsa_executable_load_code_object(executable2, agent2, code_object2, "");
    //check(Loading the code object, err);

    /*
     * Freeze the executable; it can now be queried for symbols.
     */
    err = hsa_executable_freeze(executable1, "");
    err = hsa_executable_freeze(executable2, "");
    //check(Freeze the executable, err);

   /*
    * Extract the symbol from the executable.
    */
    hsa_executable_symbol_t symbol1;
    hsa_executable_symbol_t symbol2;
    err = hsa_executable_get_symbol(executable1, NULL, "&__vector_copy_kernel", agent1, 0, &symbol1);
    err = hsa_executable_get_symbol(executable2, NULL, "&__vector_copy_kernel", agent2, 0, &symbol2);
    //check(Extract the symbol from the executable, err);

    /*
     * Extract dispatch information from the symbol
     */
    uint64_t kernel_object1;
    uint32_t kernarg_segment_size1;
    uint32_t group_segment_size1;
    uint32_t private_segment_size1;
    err = hsa_executable_symbol_get_info(symbol1, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &kernel_object1);
    //check(Extracting the symbol from the executable, err);
    err = hsa_executable_symbol_get_info(symbol1, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_SIZE, &kernarg_segment_size1);
    //check(Extracting the kernarg segment size from the executable, err);
    err = hsa_executable_symbol_get_info(symbol1, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE, &group_segment_size1);
    //check(Extracting the group segment size from the executable, err);
    err = hsa_executable_symbol_get_info(symbol1, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE, &private_segment_size1);
    //check(Extracting the private segment from the executable, err);

    /*
     * Extract dispatch information from the symbol
     */
    uint64_t kernel_object2;
    uint32_t kernarg_segment_size2;
    uint32_t group_segment_size2;
    uint32_t private_segment_size2;
    err = hsa_executable_symbol_get_info(symbol2, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &kernel_object2);
    //check(Extracting the symbol from the executable, err);
    err = hsa_executable_symbol_get_info(symbol2, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_SIZE, &kernarg_segment_size2);
    //check(Extracting the kernarg segment size from the executable, err);
    err = hsa_executable_symbol_get_info(symbol2, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE, &group_segment_size2);
    //check(Extracting the group segment size from the executable, err);
    err = hsa_executable_symbol_get_info(symbol2, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE, &private_segment_size2);
    //check(Extracting the private segment from the executable, err);

    /*
     * Create a signal to wait for the dispatch to finish.
     */ 
    hsa_agent_t agent_list[2] = {agent1, agent2}; 
    hsa_signal_t signal1;
	hsa_signal_t signal2;
    err=hsa_signal_create(1, 0, NULL, &signal1);
    err=hsa_signal_create(1, 0, NULL, &signal2);
    //check(Creating a HSA signal, err);

    /*
     * Allocate and initialize the kernel arguments and data.
     */
    char* in1=(char*)malloc(1024*1024*4);
    memset(in1, 1, 1024*1024*4);
    err=hsa_memory_register(in1, 1024*1024*4);
    //check(Registering argument memory for input parameter, err);

    char* out1=(char*)malloc(1024*1024*4);
    memset(out1, 0, 1024*1024*4);
    err=hsa_memory_register(out1, 1024*1024*4);
    //check(Registering argument memory for output parameter, err);

    struct __attribute__ ((aligned(16))) args1_t {
        void* in1;
        void* out1;
    } args1;

    args1.in1=in1;
    args1.out1=out1;

    /*
     * Allocate and initialize the kernel arguments and data.
     */
    char* in2=(char*)malloc(1024*1024*4);
    memset(in2, 1, 1024*1024*4);
    err=hsa_memory_register(in2, 1024*1024*4);
    //check(Registering argument memory for input parameter, err);

    char* out2=(char*)malloc(1024*1024*4);
    memset(out2, 0, 1024*1024*4);
    err=hsa_memory_register(out2, 1024*1024*4);
    //check(Registering argument memory for output parameter, err);

    struct __attribute__ ((aligned(16))) args2_t {
        void* in2;
        void* out2;
    } args2;

    args2.in2=in2;
    args2.out2=out2;

    /*
     * Find a memory region that supports kernel arguments.
     */
    hsa_region_t kernarg_region1;
    hsa_region_t kernarg_region2;
    kernarg_region1.handle=(uint64_t)-1;
    kernarg_region2.handle=(uint64_t)-1;
    hsa_agent_iterate_regions(agent1, get_kernarg_memory_region, &kernarg_region1);
    hsa_agent_iterate_regions(agent2, get_kernarg_memory_region, &kernarg_region2);
    err = (kernarg_region1.handle == (uint64_t)-1) ? HSA_STATUS_ERROR : HSA_STATUS_SUCCESS;
    err = (kernarg_region2.handle == (uint64_t)-1) ? HSA_STATUS_ERROR : HSA_STATUS_SUCCESS;
    //check(Finding a kernarg memory region, err);
    void* kernarg_address1 = NULL;
    void* kernarg_address2 = NULL;

    /*
     * Allocate the kernel argument buffer from the correct region.
     */   
    err = hsa_memory_allocate(kernarg_region1, kernarg_segment_size1, &kernarg_address1);
    err = hsa_memory_allocate(kernarg_region2, kernarg_segment_size2, &kernarg_address2);
    //check(Allocating kernel argument memory buffer, err);
    memcpy(kernarg_address1, &args1, sizeof(args1));
    memcpy(kernarg_address2, &args2, sizeof(args2));
 
    /*
     * Obtain the current queue write index.
     */
    uint64_t index1 = hsa_queue_load_write_index_relaxed(queue1);
    uint64_t index2 = hsa_queue_load_write_index_relaxed(queue2);

    /*
     * Write the aql packet at the calculated queue index address.
     */
    const uint32_t queueMask1 = queue1->size - 1;
    hsa_kernel_dispatch_packet_t* dispatch_packet1 = &(((hsa_kernel_dispatch_packet_t*)(queue1->base_address))[index1&queueMask1]);

    dispatch_packet1->setup  |= 1 << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
    dispatch_packet1->workgroup_size_x = (uint16_t)256;
    dispatch_packet1->workgroup_size_y = (uint16_t)1;
    dispatch_packet1->workgroup_size_z = (uint16_t)1;
    dispatch_packet1->grid_size_x = (uint32_t) (1024*1024);
    dispatch_packet1->grid_size_y = 1;
    dispatch_packet1->grid_size_z = 1;
    dispatch_packet1->completion_signal = signal1;
    dispatch_packet1->kernel_object = kernel_object1;
    dispatch_packet1->kernarg_address = (void*) kernarg_address1;
    dispatch_packet1->private_segment_size = private_segment_size1;
    dispatch_packet1->group_segment_size = group_segment_size1;

    uint16_t header1 = 0;
    header1 |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
    header1 |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE;
    header1 |= HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;

    __atomic_store_n((uint16_t*)(&dispatch_packet1->header), header1, __ATOMIC_RELEASE);

    /*
     * Write the aql packet at the calculated queue index address.
     */
    const uint32_t queueMask2 = queue2->size - 1;
    hsa_kernel_dispatch_packet_t* dispatch_packet2 = &(((hsa_kernel_dispatch_packet_t*)(queue2->base_address))[index2&queueMask2]);

    dispatch_packet2->setup  |= 1 << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
    dispatch_packet2->workgroup_size_x = (uint16_t)256;
    dispatch_packet2->workgroup_size_y = (uint16_t)1;
    dispatch_packet2->workgroup_size_z = (uint16_t)1;
    dispatch_packet2->grid_size_x = (uint32_t) (1024*1024);
    dispatch_packet2->grid_size_y = 1;
    dispatch_packet2->grid_size_z = 1;
    dispatch_packet2->completion_signal = signal2;
    dispatch_packet2->kernel_object = kernel_object2;
    dispatch_packet2->kernarg_address = (void*) kernarg_address2;
    dispatch_packet2->private_segment_size = private_segment_size2;
    dispatch_packet2->group_segment_size = group_segment_size2;

    uint16_t header2 = 0;
    header2 |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
    header2 |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE;
    header2 |= HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;

    __atomic_store_n((uint16_t*)(&dispatch_packet2->header), header2, __ATOMIC_RELEASE);

    /*
     * Increment the write index and ring the doorbell to dispatch the kernel.
     */
    hsa_queue_store_write_index_relaxed(queue1, index1+1);
    hsa_queue_store_write_index_relaxed(queue2, index2+1);
    hsa_signal_store_relaxed(queue1->doorbell_signal, index1);
    hsa_signal_store_relaxed(queue2->doorbell_signal, index2);
    //check(Dispatching the kernel, err);

    /*
     * Wait on the dispatch completion signal until the kernel is finished.
     */
    hsa_signal_value_t value1 = hsa_signal_wait_acquire(signal1, HSA_SIGNAL_CONDITION_LT, 1, UINT64_MAX, HSA_WAIT_STATE_BLOCKED);
    hsa_signal_value_t value2 = hsa_signal_wait_acquire(signal2, HSA_SIGNAL_CONDITION_LT, 1, UINT64_MAX, HSA_WAIT_STATE_BLOCKED);

    /*
     * Validate the data in the output buffer.
     */
    int valid=1;
    int fail_index=0;
    for(int i=0; i<1024*1024; i++) {
        if(out1[i]!=in1[i]) {
            fail_index=i;
            valid=0;
            break;
        }
    }

    for(int i=0; i<1024*1024; i++) {
        if(out2[i]!=in2[i]) {
            fail_index=i;
            valid=0;
            break;
        }
    }


    if(valid) {
        printf("Passed validation.\n");
    } else {
        printf("VALIDATION FAILED!\nBad index: %d\n", fail_index);
    }

    /*
     * Cleanup all allocated resources.
     */
    err = hsa_memory_free(kernarg_address1);
    err = hsa_memory_free(kernarg_address2);
    //check(Freeing kernel argument memory buffer, err);

    err=hsa_signal_destroy(signal1);
	err=hsa_signal_destroy(signal2);
    //check(Destroying the signal, err);

    err=hsa_executable_destroy(executable1);
    err=hsa_executable_destroy(executable2);
    //check(Destroying the executable, err);

    err=hsa_code_object_destroy(code_object1);
    err=hsa_code_object_destroy(code_object2);
    //check(Destroying the code object, err);

    err=hsa_queue_destroy(queue2);
    err=hsa_queue_destroy(queue1);
    //check(Destroying the queue, err);
    
    err=hsa_shut_down();
    //check(Shutting down the runtime, err);

    free(in1); free(in2);
    free(out1); free(out2);

    return 0;
}
