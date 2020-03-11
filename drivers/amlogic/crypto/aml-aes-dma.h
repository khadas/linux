/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_AES_DMA_H_
#define _AML_AES_DMA_H_

int aml_aes_process(struct ablkcipher_request *req);
//void aml_sha_finish_req(struct ahash_request *req, int err);
#endif  // _AML_AES_DMA_H_
