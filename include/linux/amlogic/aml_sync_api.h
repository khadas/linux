/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

struct dma_fence;

void *aml_sync_create_timeline(const char *tname);
int aml_sync_create_fence(void *timeline, unsigned int value);
int aml_sync_inc_timeline(void *timeline, unsigned int value);

struct dma_fence *aml_sync_get_fence(int syncfile_fd);
int aml_sync_wait_fence(struct dma_fence *syncfile, long timeout);
void aml_sync_put_fence(struct dma_fence *syncfile);
