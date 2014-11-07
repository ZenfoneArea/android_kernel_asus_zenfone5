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

#include "ia_css_types.h"
#include "assert_support.h"
#include "ia_css_queue.h" /* sp2host_dequeue_irq_event() */
#include "ia_css_eventq.h"
#include "ia_css_event.h"	/* ia_css_event_encode()
				ia_css_event_decode()
				*/
#include "platform_support.h" /* hrt_sleep() */

enum ia_css_err ia_css_eventq_recv(
		ia_css_queue_t *eventq_handle,
		uint8_t *payload)
{
	enum ia_css_err status;
	uint32_t sp_event;

	/* dequeue the IRQ event */
	status = ia_css_queue_dequeue(eventq_handle, &sp_event);

	/* check whether the IRQ event is available or not */
	if (IA_CSS_SUCCESS == status)
		ia_css_event_decode(sp_event, payload);
	return status;
}

/**
 * @brief The Host sends the event to the SP.
 * Refer to "sh_css_sp.h" for details.
 */
enum ia_css_err ia_css_eventq_send(
			ia_css_queue_t *eventq_handle,
			uint8_t evt_id,
			uint8_t evt_payload_0,
			uint8_t evt_payload_1,
			uint8_t evt_payload_2)
{
	uint8_t tmp[4];
	uint32_t sw_event;
	enum ia_css_err status;
	/*
	 * Encode the queue type, the thread ID and
	 * the queue ID into the event.
	 */
	tmp[0] = evt_id;
	tmp[1] = evt_payload_0;
	tmp[2] = evt_payload_1;
	tmp[3] = evt_payload_2;
	ia_css_event_encode(tmp, 4, &sw_event);

	/* queue the software event (busy-waiting) */
	do {
		status = ia_css_queue_enqueue(eventq_handle, sw_event);
		if (IA_CSS_ERR_QUEUE_IS_FULL != status ) {
			/* We were able to successfully send the event
			   or had a real failure. return the status*/
			return status;
		}
		/* Wait for the queue to be not full and try again*/
		hrt_sleep();
	} while(1);

	return IA_CSS_ERR_INTERNAL_ERROR;
}
