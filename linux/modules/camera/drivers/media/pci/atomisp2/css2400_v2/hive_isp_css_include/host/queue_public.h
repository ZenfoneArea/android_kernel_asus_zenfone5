/* Release Version: ci_master_byt_20130905_2200 */
/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2013 Intel Corporation. All Rights Reserved.
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

#ifndef __QUEUE_PUBLIC_H_INCLUDED__
#define __QUEUE_PUBLIC_H_INCLUDED__


#ifdef __KERNEL__
#include <linux/kernel.h>
#else
#include <stdint.h>
#include <stdbool.h>	/* bool */
#endif

#include "sh_css_internal.h"	/* enum sh_css_frame_id */


/*! The Host initialize the "host2sp" queues.
 */
extern void init_host2sp_queues(void);

/*! The Host initialize the "sp2host" queues.
 */
extern void init_sp2host_queues(void);

/************************************************************
 *
 * Buffer queues (the host -> the SP).
 *
 ************************************************************/
/*! The Host puts the buffer at the tail of the queue

 \param	pipe_num[in]			the pipe number
 \param	stage_num[in]			the stage number
 \param	index[in]			the queue to enqueue to
 \param	buffer_ptr[in]			the buffer that will be enqueued

 \return !isFull(host2sp_queue[pipe_num][stage_num][index])
 */
extern bool host2sp_enqueue_buffer(
	unsigned int pipe_num,
	unsigned int stage_num,
	enum sh_css_buffer_queue_id index,
	uint32_t buffer_ptr);

/*! The Host gets the buffer at the head of the queue

 \param	thread_id[in]			the thread_id
 \param	stage_num[in]			the stage number
 \param	index[in]			the queue to dequeue from
 \param	buffer_ptr[out]			the buffer address to which will be dequeued

 \return !isFull(host2sp_queue[thread_id][stage_num][index])
 */
extern bool host2sp_dequeue_buffer(
	unsigned int thread_id,
	unsigned int stage_num,
	enum sh_css_buffer_queue_id index,
	uint32_t *buffer_ptr);

/*! The numer of empty slots in a queue

 \param	pipe_num[in]			the pipe number
 \param	index[in]			the queue to enqueue to

 \return the number of empty slots in the queue
 */
uint32_t host2sp_empty_slots(
	unsigned int pipe_num,
	enum sh_css_buffer_queue_id index);

/************************************************************
 *
 * Buffer queues (the SP -> the host).
 *
 ************************************************************/
/*! The sp gets the buffer at the head of the queue

 \param	pipe_num[in]			the pipe number
 \param	stage_num[in]			the stage number
 \param	index[in]			the queue to dequeue from
 \param	buffer_ptr[out]			the buffer address to which will be dequeued

 \return !isEmpty(sp2host_queue[frame_id])
 */
extern bool sp2host_dequeue_buffer(
	unsigned int pipe_num,
	unsigned int stage_num,
	enum sh_css_buffer_queue_id index,
	uint32_t *buffer_ptr);

/************************************************************
 *
 * Event queues (the host -> the SP).
 *
 ************************************************************/
/*! The Host puts the SP event at the tail of the queue.

 \param	event[in]			the SP event that will be queued

 \return !isFull(host2sp_queue)
 */
extern bool host2sp_enqueue_sp_event(
		uint32_t event);

/************************************************************
 *
 * Event queues (the SP -> the host).
 *
 ************************************************************/
/*! The Host gets the IRQ event from the IRQ queue

 \param	event[out]			the IRQ event that will be dequeued

 \return !isEmpty(sp2host_queue)
 */
extern bool sp2host_dequeue_irq_event(
	uint32_t *event);

#endif /* __QUEUE_PUBLIC_H_INCLUDED__ */
