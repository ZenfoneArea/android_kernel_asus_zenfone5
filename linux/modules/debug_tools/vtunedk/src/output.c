/*COPYRIGHT**
    Copyright (C) 2005-2012 Intel Corporation.  All Rights Reserved.
 
    This file is part of SEP Development Kit
 
    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.
 
    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 
    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
**COPYRIGHT*/

#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv.h"
#include "lwpmudrv_ioctl.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"

#include "control.h"
#include "output.h"

#define OTHER_C_DEVICES  1     // one for module
/*
 *  Global data: Buffer control structure
 */
static wait_queue_head_t flush_queue;
static atomic_t          flush_writers;
static volatile int      flush = 0;

extern S32               abnormal_terminate;

/*
 *  @fn output_Free_Buffers(output, size)
 *
 *  @param    IN  outbuf      - The output buffer to manipulate
 *
 *  @brief   Deallocate the memory associated with the buffer descriptor
 *
 */
static VOID
output_Free_Buffers (
    BUFFER_DESC   buffer,
    size_t        size
)
{
    int       j;
    OUTPUT    outbuf;

    if (buffer == NULL) {
        return;
    }
    outbuf = &BUFFER_DESC_outbuf(buffer);
    for (j = 0; j < OUTPUT_NUM_BUFFERS; j++) {
        CONTROL_Free_Memory(OUTPUT_buffer(outbuf,j));
        OUTPUT_buffer(outbuf,j) = NULL;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 *  @fn  int OUTPUT_Reserve_Buffer_Space (OUTPUT      outbuf,
 *                                        U32         size)
 *
 *  @param  outbuf        IN output buffer to manipulate
 *  @param  size          IN The size of data to reserve
 *
 *  @result outloc - to the location where data is to be written
 *
 *  Reserve space in the output buffers for data.  If a buffer is full,
 *  signal the caller that the flush routine needs to be called.
 *
 * <I>Special Notes:</I>
 *
 */
extern void* 
OUTPUT_Reserve_Buffer_Space (
    BUFFER_DESC  bd,
    U32          size
)
{
    int     signal_full = FALSE;
    char   *outloc      = NULL;
    OUTPUT  outbuf      = &BUFFER_DESC_outbuf(bd);

    if (OUTPUT_remaining_buffer_size(outbuf) >= size) {
        outloc = (OUTPUT_buffer(outbuf,OUTPUT_current_buffer(outbuf)) + 
          (OUTPUT_total_buffer_size(outbuf) - OUTPUT_remaining_buffer_size(outbuf)));
    }
    else {
        U32  i, j, start;
        OUTPUT_buffer_full(outbuf,OUTPUT_current_buffer(outbuf)) = 
                OUTPUT_total_buffer_size(outbuf) - OUTPUT_remaining_buffer_size(outbuf);

        //
        // Massive Naive assumption:  Must find a way to fix it.
        // In spite of the loop.
        // The next buffer to fill are monotonically increasing
        // indicies.
        //
        signal_full = TRUE;
        start = OUTPUT_current_buffer(outbuf);
        for (i = start+1; i < start+OUTPUT_NUM_BUFFERS; i++) {

            j = i%OUTPUT_NUM_BUFFERS;

            if (!OUTPUT_buffer_full(outbuf,j)) {
                OUTPUT_current_buffer(outbuf) = j;
                OUTPUT_remaining_buffer_size(outbuf) = OUTPUT_total_buffer_size(outbuf);
                outloc = OUTPUT_buffer(outbuf,j);
            }
            else {
                signal_full = FALSE;
            }
        }
    }
    if (outloc) {
        OUTPUT_remaining_buffer_size(outbuf) -= size;
        memset(outloc, 0, size);
    }
#if !defined(CONFIG_PREEMPT_RT)
    if (signal_full) {
        wake_up_interruptible_sync(&BUFFER_DESC_queue(bd));
    }
#endif

    return outloc;
}

/* ------------------------------------------------------------------------- */
/*!
 *
 * @fn  int  OUTPUT_Buffer_Fill (BUFFER_DESC buf, 
 *                               PVOID  data,
 *                               U16    size)
 *
 * @brief     Place a record (can be module, marker, etc) in a buffer
 *
 * @param     data - pointer to a buffer to copy
 * @param     size - size of the buffer to cpu
 *
 * @return    number of bytes copied into buffer
 *
 * Start by ensuring that output buffer space is available.
 * If so, then copy the input data to the output buffer and make the necessary
 * adjustments to manage the output buffers.
 * If not, signal the read event for this buffer and get another buffer.
 *
 * <I>Special Notes:</I>
 *
 */
static int
output_Buffer_Fill (
    BUFFER_DESC   bd,
    PVOID         data,
    U16           size
)
{
    char        *outloc;

    outloc = OUTPUT_Reserve_Buffer_Space (bd, size);
    if (outloc) {
        memcpy(outloc, data, size);
        return size;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn  int  OUTPUT_Module_Fill (PVOID  data,
 *                               U16    size)
 *
 * @brief     Place a module record in a buffer
 *
 * @param     data - pointer to a buffer to copy
 * @param     size - size of the buffer to cpu
 *
 * @return    number of bytes copied into buffer
 *
 *
 */
extern int
OUTPUT_Module_Fill (
    PVOID     data,
    U16       size
)
{
    int     ret_size;
    OUTPUT  outbuf = &BUFFER_DESC_outbuf(module_buf);
    spin_lock(&OUTPUT_buffer_lock(outbuf));
    ret_size = output_Buffer_Fill(module_buf, data, size);
    spin_unlock(&OUTPUT_buffer_lock(outbuf));

    return ret_size;
}


/* ------------------------------------------------------------------------- */
/*!
 *  @fn  ssize_t  output_Read(struct file  *filp, 
 *                            char         *buf, 
 *                            size_t        count, 
 *                            loff_t       *f_pos,
 *                            BUFFER_DESC   kernel_buf)
 *
 *  @brief  Return a sample buffer to user-mode. If not full or flush, wait
 *
 *  @param *filp          a file pointer
 *  @param *buf           a sampling buffer
 *  @param  count         size of the user's buffer
 *  @param  f_pos         file pointer (current offset in bytes)
 *  @param  kernel_buf    the kernel output buffer structure
 *
 *  @return number of bytes read. zero indicates end of file. Neg means error
 *
 *  Place no more than count bytes into the user's buffer.
 *  Block if unavailable on "BUFFER_DESC_queue(buf)"
 *
 * <I>Special Notes:</I>
 *
 */
static ssize_t
output_Read (
    struct file  *filp, 
    char         *buf, 
    size_t        count, 
    loff_t       *f_pos,
    BUFFER_DESC   kernel_buf
)
{
    ssize_t  to_copy;
    ssize_t  uncopied;
    OUTPUT   outbuf = &BUFFER_DESC_outbuf(kernel_buf);
    U32      cur_buf, i;

/* Buffer is filled by output_fill_modules. */

    cur_buf = OUTPUT_current_buffer(outbuf);
    for (i=0; i<OUTPUT_NUM_BUFFERS; i++) { //iterate through all buffers
        cur_buf++;
        if (cur_buf >= OUTPUT_NUM_BUFFERS) { cur_buf = 0; } //circularly
        if ((to_copy = OUTPUT_buffer_full(outbuf, cur_buf))) {
            break;
        }
    }
    SEP_PRINT_DEBUG("buffer %d has %d bytes ready\n", (S32)cur_buf, (S32)to_copy);
    if (!flush && to_copy == 0) {
#if defined(CONFIG_PREEMPT_RT)
        do {
            unsigned long delay;
            delay = msecs_to_jiffies(1000);
            wait_event_interruptible_timeout(BUFFER_DESC_queue(kernel_buf),
                                 flush||OUTPUT_buffer_full(outbuf, cur_buf), delay);
        } while (!(flush||OUTPUT_buffer_full(outbuf, cur_buf)));
#else
        if (wait_event_interruptible(BUFFER_DESC_queue(kernel_buf),
                                 flush||OUTPUT_buffer_full(outbuf, cur_buf))) {
            return OS_RESTART_SYSCALL;
        }
#endif
        SEP_PRINT_DEBUG("Get to copy\n", (S32)cur_buf);
        to_copy = OUTPUT_buffer_full(outbuf, cur_buf);
        SEP_PRINT_DEBUG("output_Read awakened, buffer %d has %d bytes\n",cur_buf, (int)to_copy );
    }

    /* Ensure that the user's buffer is large enough */
    if (to_copy > count) {
        SEP_PRINT_DEBUG("user buffer is too small\n");
        return OS_NO_MEM;
    }

    /* Copy data to user space. Note that we use cur_buf as the source */ 
    if (abnormal_terminate == 0) {
        uncopied = copy_to_user(buf,
                                OUTPUT_buffer(outbuf, cur_buf),
                                to_copy);
        /* Mark the buffer empty */
        OUTPUT_buffer_full(outbuf, cur_buf) = 0;
        *f_pos += to_copy-uncopied;
        if (uncopied) {
            SEP_PRINT_DEBUG("only copied %d of %lld bytes of module records\n", 
                    (S32)to_copy, (long long)uncopied);
            return (to_copy - uncopied);
        }
    }
    else {
        to_copy = 0;
        SEP_PRINT_DEBUG("to copy set to 0\n");
    }

    // At end-of-file, decrement the count of active buffer writers

    if (to_copy == 0) {
        DRV_BOOL flush_val = atomic_dec_and_test(&flush_writers);
        SEP_PRINT_DEBUG("output_Read decremented flush_writers\n");
        if (flush_val == TRUE) {
            wake_up_interruptible_sync(&flush_queue);
        }
    }

    return to_copy;
}

/* ------------------------------------------------------------------------- */
/*!
 *  @fn  ssize_t  OUTPUT_Module_Read(struct file  *filp, 
 *                                   char         *buf, 
 *                                   size_t        count, 
 *                                   loff_t       *f_pos)
 *
 *  @brief  Return a module buffer to user-mode. If not full or flush, wait
 *
 *  @param *filp   a file pointer
 *  @param *buf    a sampling buffer
 *  @param  count  size of the user's buffer
 *  @param  f_pos  file pointer (current offset in bytes)
 *  @param  buf    the kernel output buffer structure
 *
 *  @return number of bytes read. zero indicates end of file. Neg means error
 *
 *  Place no more than count bytes into the user's buffer.
 *  Block on "BUFFER_DESC_queue(kernel_buf)" if buffer isn't full.
 *
 * <I>Special Notes:</I>
 *
 */
extern ssize_t
OUTPUT_Module_Read (
    struct file  *filp, 
    char         *buf, 
    size_t        count, 
    loff_t       *f_pos
)
{
    SEP_PRINT_DEBUG("read request for modules on minor\n");

    return output_Read(filp, buf, count, f_pos, module_buf);
}


/* ------------------------------------------------------------------------- */
/*!
 *  @fn  ssize_t  OUTPUT_Sample_Read(struct file  *filp, 
 *                                   char         *buf, 
 *                                   size_t        count, 
 *                                   loff_t       *f_pos)
 *
 *  @brief  Return a sample buffer to user-mode. If not full or flush, wait
 *
 *  @param *filp   a file pointer
 *  @param *buf    a sampling buffer
 *  @param  count  size of the user's buffer
 *  @param  f_pos  file pointer (current offset in bytes)
 *  @param  buf    the kernel output buffer structure
 *
 *  @return number of bytes read. zero indicates end of file. Neg means error
 *
 *  Place no more than count bytes into the user's buffer.
 *  Block on "BUFFER_DESC_queue(kernel_buf)" if buffer isn't full.
 *
 * <I>Special Notes:</I>
 *
 */
extern ssize_t
OUTPUT_Sample_Read (
    struct file  *filp, 
    char         *buf, 
    size_t        count, 
    loff_t       *f_pos
)
{
    int     i;

    i = iminor(filp->f_dentry->d_inode); // kernel pointer - not user pointer
    SEP_PRINT_DEBUG("read request for samples on minor %d\n", i);

    return output_Read(filp, buf, count, f_pos, &(cpu_buf[i]));
}

/*
 *  @fn output_Initialized_Buffers()
 *
 *  @result OUTPUT
 *  @param  BUFFER_DESC desc   - descriptor for the buffer being initialized
 *  @param  U32         factor - multiplier for OUTPUT_BUFFER_SIZE.
 *                               1 for cpu buffers, 2 for module buffers.
 *
 *  @brief  Allocate, initialize, and return an output data structure
 *
 * <I>Special Notes:</I>
 *     Multiple (OUTPUT_NUM_BUFFERS) buffers will be allocated
 *     Each buffer is of size (OUTPUT_BUFFER_SIZE)
 *     Each field in the buffer is initialized
 *     The event queue for the OUTPUT is initialized
 *
 */
static BUFFER_DESC
output_Initialized_Buffers (
    BUFFER_DESC desc,
    U32         factor
)
{
    OUTPUT       outbuf;
    int          j;

/*
 *  Allocate the BUFFER_DESC, then allocate its buffers
 */
    if (desc == NULL) {
        desc = (BUFFER_DESC)CONTROL_Allocate_Memory(sizeof(BUFFER_DESC_NODE));
        if (desc == NULL) {
            SEP_PRINT_DEBUG("OUTPUT Initialize_Buffer: Failed Allocation\n");
            return(desc);
        }
    }
    outbuf = &(BUFFER_DESC_outbuf(desc));
    spin_lock_init(&OUTPUT_buffer_lock(outbuf));
    for (j = 0; j < OUTPUT_NUM_BUFFERS; j++) {
        if (OUTPUT_buffer(outbuf,j) == NULL) {
            OUTPUT_buffer(outbuf,j) = CONTROL_Allocate_Memory(OUTPUT_BUFFER_SIZE * factor);
        }
        OUTPUT_buffer_full(outbuf,j) = 0;
        if (!OUTPUT_buffer(outbuf,j)) {
            SEP_PRINT_DEBUG("OUTPUT Initialize_Buffer: Failed Allocation\n");
            /*return NULL to tell the caller that allocation failed*/
            return NULL;
        }
    }
    /*
     *  Initialize the remaining fields in the BUFFER_DESC
     */
    OUTPUT_current_buffer(outbuf)        = 0;
    OUTPUT_remaining_buffer_size(outbuf) = OUTPUT_BUFFER_SIZE * factor;
    OUTPUT_total_buffer_size(outbuf)     = OUTPUT_BUFFER_SIZE * factor;
    init_waitqueue_head(&BUFFER_DESC_queue(desc));

    return(desc);
}

/*
 *  @fn extern void OUTPUT_Initialize(buffer, len)
 *
 *  @param   buffer  -  seed name of the output file
 *  @param   len     -  length of the seed name
 *  @returns None
 *  @brief  Allocate, initialize, and return all output data structure
 *
 * <I>Special Notes:</I>
 *      Initialize the output structures.
 *      For each CPU in the system, allocate the output buffers.
 *      Initialize a module buffer and temp file to hold module information
 *      Initialize the read queues for each sample buffer
 *
 */
extern OS_STATUS
OUTPUT_Initialize (
    char          *buffer, 
    unsigned long  len
)
{
    BUFFER_DESC    unused;
    int            i;
    OS_STATUS      status = OS_SUCCESS;

    flush = 0;
    for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
        unused = output_Initialized_Buffers(&cpu_buf[i], 1);
        if (!unused) {
            SEP_PRINT_ERROR("OUTPUT_Initialize: Failed to allocate cpu output buffers\n");
            OUTPUT_Destroy();
            return OS_NO_MEM;
        }
    }

    /*
     *  Just need one module buffer
     */
    module_buf = output_Initialized_Buffers(module_buf, MODULE_BUFF_SIZE);
    if (!module_buf) {
        SEP_PRINT_ERROR("OUTPUT_Initialize: Failed to create module output buffers\n");
        OUTPUT_Destroy();
        return OS_NO_MEM;
    }

    return status;
}


/*
 *  @fn OS_STATUS  OUTPUT_Flush()
 *
 *  @brief  Flush the module buffers and sample buffers
 *
 *  @return OS_STATUS 
 *
 *  For each CPU in the system, set buffer full to the byte count to flush.
 *  Flush the modules buffer, as well.
 *
 */
extern int 
OUTPUT_Flush (
    VOID
)
{
    int        i;
    int        writers = 0;
    OUTPUT     outbuf;

    /*
     *  Flush all remaining data to files
     *  set up a flush event
     */
    init_waitqueue_head(&flush_queue);
    SEP_PRINT_DEBUG("flush: waiting for %d writers\n",(GLOBAL_STATE_num_cpus(driver_state)+ OTHER_C_DEVICES));
    for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
        if (CPU_STATE_initial_mask(&pcb[i]) == 0) {
            continue;
        }
        outbuf = &(cpu_buf[i].outbuf);
        writers += 1;
        OUTPUT_buffer_full(outbuf,OUTPUT_current_buffer(outbuf)) = 
            OUTPUT_total_buffer_size(outbuf) - OUTPUT_remaining_buffer_size(outbuf);
    }
    atomic_set(&flush_writers, writers + OTHER_C_DEVICES);   
    // Flip the switch to terminate the output threads
    // Do not do this earlier, as threads may terminate before all the data is flushed
    flush = 1;
    for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
        if (CPU_STATE_initial_mask(&pcb[i]) == 0) {
            continue;
        }
        outbuf = &BUFFER_DESC_outbuf(&cpu_buf[i]);
        OUTPUT_buffer_full(outbuf,OUTPUT_current_buffer(outbuf)) = 
            OUTPUT_total_buffer_size(outbuf) - OUTPUT_remaining_buffer_size(outbuf);
        wake_up_interruptible_sync(&BUFFER_DESC_queue(&cpu_buf[i]));
    }

    // Flush all data from the module buffers

    outbuf = &BUFFER_DESC_outbuf(module_buf);
    OUTPUT_buffer_full(outbuf,OUTPUT_current_buffer(outbuf)) = 
                              OUTPUT_total_buffer_size(outbuf) - OUTPUT_remaining_buffer_size(outbuf);
    SEP_PRINT_DEBUG("OUTPUT_Flush - waking up module_queue\n");
    wake_up_interruptible_sync(&BUFFER_DESC_queue(module_buf));

    //Wait for buffers to empty
    if (wait_event_interruptible(flush_queue, atomic_read(&flush_writers)==0)) {
        return OS_RESTART_SYSCALL;
    }
    SEP_PRINT_DEBUG("OUTPUT_Flush - awakened from flush_queue\n");
    flush = 0;

    return 0;
}

/*
 *  @fn extern void OUTPUT_Destroy()
 *
 *  @param   buffer  -  seed name of the output file
 *  @param   len     -  length of the seed name
 *  @returns OS_STATUS
 *  @brief   Deallocate output structures
 *
 * <I>Special Notes:</I>
 *      Free the module buffers
 *      For each CPU in the system, free the sampling buffers
 */
extern int 
OUTPUT_Destroy (
    VOID
)
{
    int    i, n;
    OUTPUT outbuf;

    if (module_buf != NULL) {
        outbuf = &BUFFER_DESC_outbuf(module_buf);
        output_Free_Buffers(module_buf, OUTPUT_total_buffer_size(outbuf));
    }

    if (cpu_buf != NULL) {
        n = GLOBAL_STATE_num_cpus(driver_state);
        for (i = 0; i < n; i++) {
            outbuf = &BUFFER_DESC_outbuf(&cpu_buf[i]);
            output_Free_Buffers(&cpu_buf[i], OUTPUT_total_buffer_size(outbuf));
        }
    }

    return 0;
}
