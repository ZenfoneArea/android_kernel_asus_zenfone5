/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __IA_CSS_QUEUE_H
#define __IA_CSS_QUEUE_H

#include "platform_support.h"
#include "ia_css_queue_comm.h"
#include "../src/queue_access.h"

#include "sh_css_internal.h"

/* Local Queue object descriptor */
struct ia_css_queue_local {
	ia_css_circbuf_desc_t *cb_desc; /*Circbuf desc for local queues*/
	ia_css_circbuf_elem_t *cb_elems; /*Circbuf elements*/
};
typedef struct ia_css_queue_local ia_css_queue_local_t;

/* Handle for queue object*/
typedef struct ia_css_queue ia_css_queue_t;


/*****************************************************************************
 * Queue Public APIs
 *****************************************************************************/
/** @brief Initialize a local queue instance.
 *
 * @param[out] qhandle. Handle to queue instance for use with API
 * @param[in]  desc.   Descriptor with queue properties filled-in
 * @return     IA_CSS_SUCCESS on successful init of queue instance.
 * @return     IA_CSS_ERR_INVALID_ARGUMENTS if args are invalid.
 *
 */
extern enum ia_css_err
ia_css_queue_local_init(ia_css_queue_t *qhandle, ia_css_queue_local_t *desc);

/** @brief Initialize a remote queue instance
 *
 * @param[out] qhandle. Handle to queue instance for use with API
 * @param[in]  desc.   Descriptor with queue properties filled-in
 * @return     IA_CSS_SUCCESS on successful init of queue instance.
 * @return     IA_CSS_ERR_INVALID_ARGUMENTS if args are invalid.
 *
 */
extern enum ia_css_err
ia_css_queue_remote_init(ia_css_queue_t *qhandle, ia_css_queue_remote_t *desc);

/** @brief Uninitialize a queue instance
 *
 * @param[in]  qhandle. Handle to queue instance
 * @return     IA_CSS_SUCCESS on successful uninit.
 *
 */
extern enum ia_css_err
ia_css_queue_uninit(ia_css_queue_t *qhandle);

/** @brief Enqueue an item in the queue instance
 *
 * @param[in]  qhandle. Handle to queue instance
 * @param[in]  item. Object to be enqueued.
 * @return     IA_CSS_SUCCESS on successful enqueue.
 * @return     IA_CSS_ERR_INVALID_ARGUMENTS if args are invalid.
 * @return     IA_CSS_ERR_QUEUE_IS_FULL if queue is full.
 *
 */
extern enum ia_css_err
ia_css_queue_enqueue(ia_css_queue_t *qhandle, uint32_t item);

/** @brief Dequeue an item from the queue instance
 *
 * @param[in]  qhandle. Handle to queue instance
 * @param[out] item. Object to be dequeued into this item.
 * @return     IA_CSS_SUCCESS on successful enqueue.
 * @return     IA_CSS_ERR_INVALID_ARGUMENTS if args are invalid.
 * @return     IA_CSS_ERR_QUEUE_IS_EMPTY if queue is empty.
 *
 */
extern enum ia_css_err
ia_css_queue_dequeue(ia_css_queue_t *qhandle, uint32_t *item);

/** @brief Check if the queue is empty
 *
 * @param[in]  qhandle. Handle to queue instance
 * @return     IA_CSS_ERR_QUEUE_IS_EMPTY if queue is empty.
 * @return     IA_CSS_ERR_INVALID_ARGUMENTS if args are invalid.
 * @return     IA_CSS_ERR_INTERNAL_ERROR for other errors.
 *
 */
extern enum ia_css_err
ia_css_queue_is_empty(ia_css_queue_t *qhandle);

/** @brief Get the usable size for the queue
 *
 * @param[in]  qhandle. Handle to queue instance
 * @param[out] size.Size value to be returned here.
 * @return     IA_CSS_SUCCESS on success.
 * @return     IA_CSS_ERR_INVALID_ARGUMENTS if args are invalid.
 *
 */
extern enum ia_css_err
ia_css_queue_get_size(ia_css_queue_t *qhandle, uint32_t *size);

#endif /* __IA_CSS_QUEUE_H */

