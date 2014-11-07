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

#include "ia_css_queue.h"
#include "ia_css_circbuf.h"
#include "assert_support.h"
#include "platform_support.h"
#include "queue_access.h"

/* MW: The queue should be application agnostic */
#include "sh_css_internal.h"
#include "sp.h"
/* sh_css_frame_id,  struct sh_css_circular_buf */

/* MW: The queue should not depend on principal interface types */
#include "ia_css_types.h"		/* ia_css_fw_info */


/*****************************************************************************
 * Queue Public APIs
 *****************************************************************************/
enum ia_css_err
ia_css_queue_local_init(ia_css_queue_t *qhandle, ia_css_queue_local_t *desc)
{
	if (NULL == qhandle || NULL == desc
		|| NULL == desc->cb_elems || NULL == desc->cb_desc) {
		/* Invalid parameters, return error*/
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}

	/* Mark the queue as Local */
	qhandle->type = IA_CSS_QUEUE_TYPE_LOCAL;

	/* Create a local circular buffer queue*/
	ia_css_circbuf_create(&qhandle->desc.cb_local,
	      desc->cb_elems,
	      desc->cb_desc);

	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_queue_remote_init(ia_css_queue_t *qhandle, ia_css_queue_remote_t *desc)
{
	if (NULL == qhandle || NULL == desc) {
		/* Invalid parameters, return error*/
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}
	/* Mark the queue as remote*/
	qhandle->type = IA_CSS_QUEUE_TYPE_REMOTE;

	/* Copy over the local queue descriptor*/
	qhandle->location = desc->location;
	qhandle->proc_id = desc->proc_id;
	qhandle->desc.remote.cb_desc_addr = desc->cb_desc_addr;
	qhandle->desc.remote.cb_elems_addr = desc->cb_elems_addr;

	/* If queue is remote, we let the local processor
	 * do its init, before using it. This is just to get us
	 * started, we can remove this restriction as we go ahead
	 */

	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_queue_uninit(ia_css_queue_t *qhandle)
{
	if (!qhandle)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	/* Load the required queue object */
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		/* Local queues are created. Destroy it*/
		ia_css_circbuf_destroy(&qhandle->desc.cb_local);
	}

	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_queue_enqueue(ia_css_queue_t *qhandle, uint32_t item)
{
	if (0 == qhandle)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	/* 1. Load the required queue object */
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		/* Directly de-ref the object and
		 * operate on the queue
		 */
		if (ia_css_circbuf_is_full(&qhandle->desc.cb_local)) {
			/* Cannot push the element. Return*/
			return IA_CSS_ERR_QUEUE_IS_FULL;
		}

		/* Push the element*/
		ia_css_circbuf_push(&qhandle->desc.cb_local, item);
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		ia_css_circbuf_desc_t cb_desc;
		ia_css_circbuf_elem_t cb_elem;
		uint32_t ignore_desc_flags = QUEUE_IGNORE_STEP_FLAG;

		/* a. Load the queue desc from remote */
		QUEUE_CB_DESC_INIT(&cb_desc);
		if (0 != ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags))
			return IA_CSS_ERR_INTERNAL_ERROR;

		/* b. Operate on the queue */
		if (ia_css_circbuf_desc_is_full(&cb_desc))
			return IA_CSS_ERR_QUEUE_IS_FULL;

		cb_elem.val = item;
		if (0 != ia_css_queue_item_store(qhandle, cb_desc.end, &cb_elem))
			return IA_CSS_ERR_INTERNAL_ERROR;

		cb_desc.end = (cb_desc.end + 1) % cb_desc.size;

		/* c. Store the queue object */
		/* Set only fields requiring update with
		 * valid value. Avoids uncessary calls
		 * to load/store functions
		 */
		ignore_desc_flags = QUEUE_IGNORE_SIZE_START_STEP_FLAGS;
		if (0 != ia_css_queue_store(qhandle, &cb_desc, ignore_desc_flags))
			return IA_CSS_ERR_INTERNAL_ERROR;
	}

	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_queue_dequeue(ia_css_queue_t *qhandle, uint32_t *item)
{
	if (qhandle == 0 || NULL == item)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	/* 1. Load the required queue object */
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		/* Directly de-ref the object and
		 * operate on the queue
		 */
		if (ia_css_circbuf_is_empty(&qhandle->desc.cb_local)) {
			/* Nothing to pop. Return empty queue*/
			return IA_CSS_ERR_QUEUE_IS_EMPTY;
		}

		*item = ia_css_circbuf_pop(&qhandle->desc.cb_local);
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		/* a. Load the queue from remote */
		ia_css_circbuf_desc_t cb_desc;
		ia_css_circbuf_elem_t cb_elem;
		uint32_t ignore_desc_flags = QUEUE_IGNORE_STEP_FLAG;

		QUEUE_CB_DESC_INIT(&cb_desc);

		if (0 != ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags))
			return IA_CSS_ERR_INTERNAL_ERROR;

		/* b. Operate on the queue */
		if (ia_css_circbuf_desc_is_empty(&cb_desc))
			return IA_CSS_ERR_QUEUE_IS_EMPTY;

		if (0 != ia_css_queue_item_load(qhandle, cb_desc.start, &cb_elem))
			return IA_CSS_ERR_INTERNAL_ERROR;

		*item = cb_elem.val;
		cb_desc.start = (cb_desc.start + 1) % cb_desc.size;
		/* c. Store the queue object */
		/* Set only fields requiring update with
		 * valid value. Avoids uncessary calls
		 * to load/store functions
		 */
		ignore_desc_flags = QUEUE_IGNORE_SIZE_END_STEP_FLAGS;
		if (0 != ia_css_queue_store(qhandle, &cb_desc, ignore_desc_flags))
			return IA_CSS_ERR_INTERNAL_ERROR;
	}
	return IA_CSS_SUCCESS;
}

extern enum ia_css_err
ia_css_queue_is_empty(ia_css_queue_t *qhandle)
{
	if (qhandle == 0)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	/* 1. Load the required queue object */
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		/* Directly de-ref the object and
		 * operate on the queue
		 */
		if ( ia_css_circbuf_is_empty(&qhandle->desc.cb_local) )
			return IA_CSS_ERR_QUEUE_IS_EMPTY;

		return IA_CSS_ERR_INTERNAL_ERROR;
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		/* a. Load the queue from remote */
		ia_css_circbuf_desc_t cb_desc;
		uint32_t ignore_desc_flags = QUEUE_IGNORE_STEP_FLAG;
		QUEUE_CB_DESC_INIT(&cb_desc);
		if (0 != ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags))
			return IA_CSS_ERR_INTERNAL_ERROR;

		/* b. Operate on the queue */
		if( ia_css_circbuf_desc_is_empty(&cb_desc) )
			return IA_CSS_ERR_QUEUE_IS_EMPTY;

		return IA_CSS_ERR_INTERNAL_ERROR;
	}

	return IA_CSS_ERR_INVALID_ARGUMENTS;
}

enum ia_css_err
ia_css_queue_get_size(ia_css_queue_t *qhandle, uint32_t *size)
{
	if (qhandle == 0 || size == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	/* 1. Load the required queue object */
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		/* Directly de-ref the object and
		 * operate on the queue
		 */
		/* Return maximum usable capacity */
		*size = ia_css_circbuf_get_size(&qhandle->desc.cb_local);
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		/* a. Load the queue from remote */
		ia_css_circbuf_desc_t cb_desc;

		uint32_t ignore_desc_flags = QUEUE_IGNORE_START_END_STEP_FLAGS;

		QUEUE_CB_DESC_INIT(&cb_desc);

		if (0 != ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags))
			return IA_CSS_ERR_INTERNAL_ERROR;

		/* Return maximum usable capacity */
		*size = cb_desc.size;
	}

	return IA_CSS_SUCCESS;
}
