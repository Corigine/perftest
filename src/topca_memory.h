/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All rights reserved.
 */

#ifndef TOPCA_MEMORY_H
#define TOPCA_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "memory.h"
#include "config.h"

struct perftest_parameters;

bool topca_memory_supported();

struct memory_ctx *topca_memory_create(struct perftest_parameters *params);

#ifndef HAVE_TOPCA
inline bool topca_memory_supported() {
	return false;
}

inline struct memory_ctx *topca_memory_create(struct perftest_parameters *params) {
	return NULL;
}
#endif

#endif /* TOPCA_MEMORY_H */
