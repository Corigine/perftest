/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <tc_runtime.h>
#include <tc_runtime_api.h>
#include "topca_memory.h"
#include "perftest_parameters.h"

int hx_map_addr_dev2dma(int dev_no, void *dev_addr, uint64_t *dma_addr);
void hx_free_dma_addr(uint64_t dma_addr);

#define TOPCA_CHECK(ret)			\
	do {					\
	int result = (ret);		\
	ASSERT(TC_SUCCESS == result);		\
} while (0)

#define TOPCA_PAGE_SIZE (4 * 1024)
#define SLICE_SIZE  (4 * 1024 * 1024)

struct topca_memory_ctx {
	struct memory_ctx base;
	uint64_t dma_addr;
	void* d_adress;
	int device_id;
};

static int init_topca(int device_id) {
	int deviceCount = 0;
	struct tcDeviceProp deviceProp;
	int tcError = tcDeviceGetCount(&deviceCount);

	if (tcError != TC_SUCCESS) {
		printf("tcDeviceGetCount() returned %d\n", tcError);
		return FAILURE;
	}

	if (device_id >= deviceCount) {
		printf("Requested hx gpu device %d but found only %d device(s)\n",
               device_id, deviceCount);
		return FAILURE;
	}

	TOPCA_CHECK(tcSetDevice(device_id));

	TOPCA_CHECK(tcDeviceGetProperties(&deviceProp, device_id));
	printf("Using hx gpu Device with ID: %d, Name: %s, PCI BDF %02x:%02x.0\n",
	       device_id, deviceProp.name, deviceProp.pci_bus_id, deviceProp.pci_dev_id);

	return SUCCESS;
}

int topca_memory_init(struct memory_ctx *ctx) {
	struct topca_memory_ctx *topca_ctx = container_of(ctx, struct topca_memory_ctx, base);

	if (init_topca(topca_ctx->device_id)) {
		fprintf(stderr, "Couldn't initialize hx gpu device\n");
		return FAILURE;
	}
	return SUCCESS;
}

int topca_memory_destroy(struct memory_ctx *ctx) {
	struct topca_memory_ctx *topca_ctx = container_of(ctx, struct topca_memory_ctx, base);

	free(topca_ctx);
	return SUCCESS;
}

int topca_memory_allocate_buffer(struct memory_ctx *ctx, int alignment, uint64_t size, int *dmabuf_fd,
				uint64_t *dmabuf_offset, void **addr, bool *can_init) {
	int error;
	size_t buf_size = (size + TOPCA_PAGE_SIZE - 1) & ~(TOPCA_PAGE_SIZE - 1);
	struct topca_memory_ctx *topca_ctx = container_of(ctx, struct topca_memory_ctx, base);

	if (buf_size > SLICE_SIZE) {
		printf("malloc gpu memory size %ld can not exceed %d for mr!\n", buf_size, SLICE_SIZE);
		return FAILURE;
	}

	error = tcMalloc(&topca_ctx->d_adress, buf_size);
	if (error != TC_SUCCESS) {
		printf("tcMalloc error=%d\n", error);
		return FAILURE;
	}

	if (hx_map_addr_dev2dma(topca_ctx->device_id,
				topca_ctx->d_adress,
				&topca_ctx->dma_addr) != TC_SUCCESS) {
		printf("fail to map gpu dma address for device address:%p\n", topca_ctx->d_adress);
		return FAILURE;
	}
	*addr = (void*)topca_ctx->dma_addr;
	*can_init = false;
	printf("allocated %lu bytes of GPU buffer at %p(dma_addr:%lx)\n",
		(unsigned long)buf_size,
		topca_ctx->d_adress,
		topca_ctx->dma_addr);
	return SUCCESS;
}

int topca_memory_free_buffer(struct memory_ctx *ctx, int dmabuf_fd, void *addr, uint64_t size) {
	int ret;
	struct topca_memory_ctx *topca_ctx = container_of(ctx, struct topca_memory_ctx, base);

	printf("deallocating GPU buffer %p(dma:%lx)\n",
		topca_ctx->d_adress,
		topca_ctx->dma_addr);

    	ret = tcFree(topca_ctx->d_adress);
	if(ret != TC_SUCCESS) {
		printf("tc free failed\n");
		return FAILURE;
	}
	hx_free_dma_addr(topca_ctx->dma_addr);
	return SUCCESS;
}

void *topca_memory_copy_host_to_buffer(void *dest, const void *src, size_t size) {
	printf("not support\n");
	return dest;
}

void *topca_memory_copy_buffer_to_host(void *dest, const void *src, size_t size) {
	printf("not support\n");
	return dest;
}

void *topca_memory_copy_buffer_to_buffer(void *dest, const void *src, size_t size) {
	printf("not support\n");
	return dest;
}

bool topca_memory_supported() {
	return true;
}

struct memory_ctx *topca_memory_create(struct perftest_parameters *params) {
	struct topca_memory_ctx *ctx;

	ALLOCATE(ctx, struct topca_memory_ctx, 1);
	ctx->base.init = topca_memory_init;
	ctx->base.destroy = topca_memory_destroy;
	ctx->base.allocate_buffer = topca_memory_allocate_buffer;
	ctx->base.free_buffer = topca_memory_free_buffer;
	ctx->base.copy_host_to_buffer = topca_memory_copy_host_to_buffer;
	ctx->base.copy_buffer_to_host = topca_memory_copy_buffer_to_host;
	ctx->base.copy_buffer_to_buffer = topca_memory_copy_buffer_to_buffer;
	ctx->device_id = params->topca_device_id;

	return &ctx->base;
}
