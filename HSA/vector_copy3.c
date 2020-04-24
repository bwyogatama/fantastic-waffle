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

#define check(msg, status) \
if (status != HSA_STATUS_SUCCESS) { \
    printf("%s failed.\n", #msg); \
    exit(1); \
} else { \
   printf("%s succeeded.\n", #msg); \
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


int main(int argc, char **argv) {
    hsa_status_t err;

    err = hsa_init();
    check(Initializing the hsa runtime, err);

    /*
     * Determine if the finalizer 1.0 extension is supported.
     */
    bool support;

    err = hsa_system_extension_supported(HSA_EXTENSION_FINALIZER, 1, 0, &support);

    check(Checking finalizer 1.0 extension support, err);

    /*
     * Generate the finalizer function table.
     */
    hsa_ext_finalizer_1_00_pfn_t table_1_00;

    err = hsa_system_get_extension_table(HSA_EXTENSION_FINALIZER, 1, 0, &table_1_00);

    check(Generating function table for finalizer, err);

    /* 
     * Iterate over the agents and pick the gpu agent using 
     * the get_gpu_agent callback.
     */
    hsa_agent_t agent1;
    hsa_agent_t agent2;
    err = hsa_iterate_agents(get_gpu_agent1, &agent1);
    err = hsa_iterate_agents(get_gpu_agent2, &agent2);
    if(err == HSA_STATUS_INFO_BREAK) { err = HSA_STATUS_SUCCESS; }
    check(Getting a gpu agent, err);

    /*
     * Query the name of the agent.
     */
    char name1[64] = { 0 };
    char name2[64] = { 0 };
    err = hsa_agent_get_info(agent1, HSA_AGENT_INFO_NAME, name1);
    err = hsa_agent_get_info(agent2, HSA_AGENT_INFO_NAME, name2);
    check(Querying the agent name, err);
    printf("The agent1 name is %s.\n", name1);
    printf("The agent2 name is %s.\n", name2);

    /*
     * Query the name of the agent.
     */
    uint8_t node1;
    uint8_t node2;
    err = hsa_agent_get_info(agent1, HSA_AGENT_INFO_NODE, &node1);
    err = hsa_agent_get_info(agent2, HSA_AGENT_INFO_NAME, &node2);
    check(Querying the agent node, err);
    printf("The agent1 node is %d.\n", node1);
    printf("The agent2 node is %d.\n", node2);


    /*
     * Query the maximum size of the queue.
     */
    uint32_t queue_size1 = 0;
    uint32_t queue_size2 = 0;
    err = hsa_agent_get_info(agent1, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size1);
    err = hsa_agent_get_info(agent2, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size2);
    check(Querying the agent maximum queue size, err);
    printf("The maximum queue size of agent1 is %u.\n", (unsigned int) queue_size1);
    printf("The maximum queue size of agent2 is %u.\n", (unsigned int) queue_size2);


    /*
     * Create a queue using the maximum size.
     */
    hsa_queue_t* queue1;
    hsa_queue_t* queue2; 
    err = hsa_queue_create(agent1, queue_size1, HSA_QUEUE_TYPE_SINGLE, NULL, NULL, UINT32_MAX, UINT32_MAX, &queue1);
    err = hsa_queue_create(agent2, queue_size2, HSA_QUEUE_TYPE_SINGLE, NULL, NULL, UINT32_MAX, UINT32_MAX, &queue2);
    check(Creating the queue, err);

    /*
     * Create a signal to wait for the dispatch to finish.
     */ 
    hsa_agent_t agent_list[2] = {agent1, agent2}; 
    hsa_signal_t signal1, signal2;
    err=hsa_signal_create(1, 0, NULL, &signal1);
	err=hsa_signal_create(1, 0, NULL, &signal2);
    check(Creating a HSA signal, err);

    /*
     * Wait on the dispatch completion signal until the kernel is finished.
     */

    hsa_signal_store_relaxed(signal1, 0);
    hsa_signal_store_relaxed(signal2, 0);
    
    hsa_signal_value_t value1 = hsa_signal_wait_acquire(signal1, HSA_SIGNAL_CONDITION_LT, 1, UINT64_MAX, HSA_WAIT_STATE_BLOCKED);
    hsa_signal_value_t value2 = hsa_signal_wait_acquire(signal2, HSA_SIGNAL_CONDITION_LT, 1, UINT64_MAX, HSA_WAIT_STATE_BLOCKED);

    /*
     * Cleanup all allocated resources.
     */

    err=hsa_signal_destroy(signal1);
    err=hsa_signal_destroy(signal2);
    check(Destroying the signal, err);

    err=hsa_queue_destroy(queue1);
    //check(Destroying the queue, err);
    err=hsa_queue_destroy(queue2);
    //check(Destroying the queue, err);
 
    err=hsa_shut_down();
    check(Shutting down the runtime, err);

    return 0;
}
