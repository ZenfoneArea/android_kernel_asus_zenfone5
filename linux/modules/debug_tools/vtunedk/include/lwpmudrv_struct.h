/*COPYRIGHT**
 * -------------------------------------------------------------------------
 *               INTEL CORPORATION PROPRIETARY INFORMATION
 *  This software is supplied under the terms of the accompanying license
 *  agreement or nondisclosure agreement with Intel Corporation and may not
 *  be copied or disclosed except in accordance with the terms of that
 *  agreement.
 *        Copyright (c) 2007-2013 Intel Corporation.  All Rights Reserved.
 * -------------------------------------------------------------------------
**COPYRIGHT*/

#ifndef _LWPMUDRV_STRUCT_H_
#define _LWPMUDRV_STRUCT_H_

#if defined(__cplusplus)
extern "C" {
#endif

// processor execution modes
#define MODE_UNKNOWN    99
// the following defines must start at 0
#define MODE_64BIT      3
#define MODE_32BIT      2
#define MODE_16BIT      1
#define MODE_V86        0

// sampling methods
#define SM_RTC          2020     // real time clock
#define SM_VTD          2021     // OS Virtual Timer Device
#define SM_NMI          2022     // non-maskable interrupt time based
#define SM_EBS          2023     // event based

// sampling mechanism bitmap definitions
#define INTERRUPT_RTC   0x1
#define INTERRUPT_VTD   0x2
#define INTERRUPT_NMI   0x4
#define INTERRUPT_EBS   0x8

// eflags defines
#define EFLAGS_VM       0x00020000  // V86 mode
#define EFLAGS_IOPL0    0
#define EFLAGS_IOPL1    0x00001000
#define EFLAGS_IOPL2    0x00002000
#define EFLAGS_IOPL3    0x00003000

extern float freq_multiplier;

typedef struct DRV_CONFIG_NODE_S  DRV_CONFIG_NODE;
typedef        DRV_CONFIG_NODE   *DRV_CONFIG;

struct DRV_CONFIG_NODE_S {
    U32          size;
    U32          num_events;
    DRV_BOOL     start_paused;
    DRV_BOOL     counting_mode;
    U32          dispatch_id;
    DRV_BOOL     enable_chipset;
    U32          num_chipset_events;
    U32          chipset_offset;
    DRV_BOOL     enable_gfx;
    DRV_BOOL     enable_pwr;
    DRV_BOOL     emon_mode;
#if defined(DRV_IA32) || defined(DRV_EM64T)
    U32          pebs_mode;
    U32          pebs_capture;
    DRV_BOOL     collect_lbrs;
    DRV_BOOL     debug_inject;
    DRV_BOOL     virt_phys_translation;
    DRV_BOOL     latency_capture;
    U32          max_gp_counters;
    DRV_BOOL     htoff_mode;
    DRV_BOOL     power_capture;
    U32          results_offset;   // this is to store the offset for this device's results
    DRV_BOOL     eventing_ip_capture;
    DRV_BOOL     hle_capture;
    U32          emon_unc_offset;
#else
    DRV_BOOL     collect_ro;
#endif
    S32          seed_name_len;
#if defined(DRV_IA32) || defined(DRV_EM64T)
    U32          padding1;
#endif
    U64          target_pid;
    DRV_BOOL     use_pcl;
    DRV_BOOL     enable_ebc;
    DRV_BOOL     enable_tbc;
#if defined(DRV_IA32) || defined(DRV_EM64T)
    U32          padding2;
#endif
    union {
        S8      *seed_name;
        U64      dummy1;
    } u1;
    union {
        S8      *cpu_mask;
        U64      dummy2;
    } u2;
    DRV_BOOL     ds_area_available;
#if defined(DRV_IA32) || defined(DRV_EM64T)
    U32          padding3;
#endif

};

#define DRV_CONFIG_size(cfg)                      (cfg)->size
#define DRV_CONFIG_num_events(cfg)                (cfg)->num_events
#define DRV_CONFIG_start_paused(cfg)              (cfg)->start_paused
#define DRV_CONFIG_counting_mode(cfg)             (cfg)->counting_mode
#define DRV_CONFIG_dispatch_id(cfg)               (cfg)->dispatch_id
#define DRV_CONFIG_enable_chipset(cfg)            (cfg)->enable_chipset
#define DRV_CONFIG_num_chipset_events(cfg)        (cfg)->num_chipset_events
#define DRV_CONFIG_chipset_offset(cfg)            (cfg)->chipset_offset
#define DRV_CONFIG_enable_gfx(cfg)                (cfg)->enable_gfx
#define DRV_CONFIG_enable_pwr(cfg)                (cfg)->enable_pwr
#define DRV_CONFIG_emon_mode(cfg)                 (cfg)->emon_mode
#if defined(DRV_IA32) || defined(DRV_EM64T)
#define DRV_CONFIG_pebs_mode(cfg)                 (cfg)->pebs_mode
#define DRV_CONFIG_pebs_capture(cfg)              (cfg)->pebs_capture
#define DRV_CONFIG_collect_lbrs(cfg)              (cfg)->collect_lbrs
#define DRV_CONFIG_debug_inject(cfg)              (cfg)->debug_inject
#define DRV_CONFIG_virt_phys_translation(cfg)     (cfg)->virt_phys_translation
#define DRV_CONFIG_latency_capture(cfg)           (cfg)->latency_capture
#define DRV_CONFIG_max_gp_counters(cfg)           (cfg)->max_gp_counters
#define DRV_CONFIG_htoff_mode(cfg)                (cfg)->htoff_mode
#define DRV_CONFIG_power_capture(cfg)             (cfg)->power_capture
#define DRV_CONFIG_results_offset(cfg)            (cfg)->results_offset
#define DRV_CONFIG_eventing_ip_capture(cfg)       (cfg)->eventing_ip_capture
#define DRV_CONFIG_hle_capture(cfg)               (cfg)->hle_capture

#define DRV_CONFIG_emon_unc_offset(cfg)           (cfg)->emon_unc_offset
#else
#define DRV_CONFIG_collect_ro(cfg)                (cfg)->collect_ro
#endif
#define DRV_CONFIG_seed_name(cfg)                 (cfg)->u1.seed_name
#define DRV_CONFIG_seed_name_len(cfg)             (cfg)->seed_name_len
#define DRV_CONFIG_cpu_mask(cfg)                  (cfg)->u2.cpu_mask
#define DRV_CONFIG_target_pid(cfg)                (cfg)->target_pid
#define DRV_CONFIG_use_pcl(cfg)                   (cfg)->use_pcl
#define DRV_CONFIG_event_based_counts(cfg)        (cfg)->enable_ebc
#define DRV_CONFIG_timer_based_counts(cfg)        (cfg)->enable_tbc
#define DRV_CONFIG_ds_area_available(cfg)         (cfg)->ds_area_available

/*
 *    X86 processor code descriptor
 */
typedef struct CodeDescriptor_s {
    union {
        U32 lowWord;                   // low dword of descriptor
        struct {                       // low broken out by fields
            U16 limitLow;              // segment limit 15:00
            U16 baseLow;               // segment base 15:00
        } s1;
    } u1;
    union {
        U32   highWord;               // high word of descriptor
        struct {                      // high broken out by bit fields
            U32   baseMid      : 8;   // base 23:16
            U32   accessed     : 1;   // accessed
            U32   readable     : 1;   // readable
            U32   conforming   : 1;   // conforming code segment
            U32   oneOne       : 2;   // always 11
            U32   dpl          : 2;   // Dpl
            U32   pres         : 1;   // present bit
            U32   limitHi      : 4;   // limit 19:16
            U32   sys          : 1;   // available for use by system
            U32   reserved_0   : 1;   // reserved, always 0
            U32   default_size : 1;   // default operation size (1=32bit, 0=16bit)
            U32   granularity  : 1;   // granularity (1=32 bit, 0=20 bit)
            U32   baseHi       : 8;   // base hi 31:24
        } s2;
    } u2;
} CodeDescriptor;

/*
 *  Sample record.  Size can be determined by looking at the header record.
 *  There can be up to 3 sections.  The SampleFileHeader defines the presence
 *  of sections and their offsets. Within a sample file, all of the sample
 *  records have the same number of sections and the same size.  However,
 *  different sample record sections and sizes can exist in different
 *  sample files.  Since recording counters and the time stamp counter for
 *  each sample can be space consuming, the user can determine whether or not
 *  this information is kept at sample collection time.
 */

typedef struct SampleRecordPC_s {   // Program Counter section
    U64   descriptor_id;
    union {
        struct {
            U64 iip;            // IA64 interrupt instruction pointer
            U64 ipsr;           // IA64 interrupt processor status register
        } s1;
        struct {
            U32  eip;           // IA32 instruction pointer
            U32  eflags;        // IA32 eflags
            CodeDescriptor csd; // IA32 code seg descriptor (8 bytes)
        } s2;
    } u1;
    U16    cs;                  // IA32 cs (0 for IA64)
    union {
        U16 cpuAndOS;                  // cpu and OS info as one word
        struct {                       // cpu and OS info broken out
            U16 cpuNum          : 12;  // cpu number (0 - 4096)
            U16 notVmid0        : 1;   // win95, vmid0 flag (1 means NOT vmid 0)
            U16 codeMode        : 2;   // processor mode, see MODE_ defines
            U16 uncore_valid    : 1;   // identifies if the uncore count is valid
        } s3;
    } u2;
    U32   tid;            // OS thread ID  (may get reused, see tidIsRaw)
    U32   pidRecIndex;    // process ID rec index (index into start of pid
                          // record section) .. can validly be 0 if not raw
                          // (array index).  Use ReturnPid() to
                          // ..access this field .. (see pidRecIndexRaw)
    union {
        U32 bitFields2;
        struct {
            U32   mrIndex        : 20;   // module record index (index into start of
                                         // module rec section) .. (see mrIndexNone)
            U32   eventIndex     : 8;    // index into the Events section
            U32   tidIsRaw       : 1;    // tid is raw OS tid
            U32   IA64PC         : 1;    // TRUE=this is a IA64 PC sample record
            U32   pidRecIndexRaw : 1;    // pidRecIndex is raw OS pid
            U32   mrIndexNone    : 1;    // no mrIndex (unknown module)
        } s4;
    } u3;
    U64 tsc;                          // processor timestamp counter
} SampleRecordPC, *PSampleRecordPC;

#define SAMPLE_RECORD_descriptor_id(x)       (x)->descriptor_id
#define SAMPLE_RECORD_iip(x)                 (x)->u1.s1.iip
#define SAMPLE_RECORD_ipsr(x)                (x)->u1.s1.ipsr
#define SAMPLE_RECORD_eip(x)                 (x)->u1.s2.eip
#define SAMPLE_RECORD_eflags(x)              (x)->u1.s2.eflags
#define SAMPLE_RECORD_csd(x)                 (x)->u1.s2.csd
#define SAMPLE_RECORD_cs(x)                  (x)->cs
#define SAMPLE_RECORD_cpu_and_os(x)          (x)->u2.cpuAndOS
#define SAMPLE_RECORD_cpu_num(x)             (x)->u2.s3.cpuNum
#define SAMPLE_RECORD_uncore_valid(x)        (x)->u2.s3.uncore_valid
#define SAMPLE_RECORD_not_vmid0(x)           (x)->u2.s3.notVmid0
#define SAMPLE_RECORD_code_mode(x)           (x)->u2.s3.codeMode
#define SAMPLE_RECORD_tid(x)                 (x)->tid
#define SAMPLE_RECORD_pid_rec_index(x)       (x)->pidRecIndex
#define SAMPLE_RECORD_bit_fields2(x)         (x)->u3.bitFields2
#define SAMPLE_RECORD_mr_index(x)            (x)->u3.s4.mrIndex
#define SAMPLE_RECORD_event_index(x)         (x)->u3.s4.eventIndex
#define SAMPLE_RECORD_tid_is_raw(x)          (x)->u3.s4.tidIsRaw
#define SAMPLE_RECORD_ia64_pc(x)             (x)->u3.s4.IA64PC
#define SAMPLE_RECORD_pid_rec_index_raw(x)   (x)->u3.s4.pidRecIndexRaw
#define SAMPLE_RECORD_mr_index_none(x)       (x)->u3.s4.mrIndexNone
#define SAMPLE_RECORD_tsc(x)                 (x)->tsc

// end of SampleRecord sections


/*
 *  Module record.  These are emitted whenever a DLL or EXE is loaded or unloaded.
 *  The filename fields may be 0 on an unload.  The records reperesent a module for a
 *  certain span of time, delineated by the load / unload samplecounts.
 *  Note:
 *  The structure contains 64 bit fields which may cause the compiler to pad the
 *  length of the structure to an 8 byte boundary.
 */
typedef struct ModuleRecord_s {
   U16    recLength;          // total length of this record (including this length,
                              // always U32 multiple)  output from sampler is variable
                              // length (pathname at end of record) sampfile builder moves
                              // path names to a separate "literal pool" area
                              // so that these records become fixed length, and can be treated
                              // as an array see modrecFixedLen in header

   U16    segmentType :  2;   // V86, 16, 32, 64 (see MODE_ defines), maybe inaccurate for Win95
                              // .. a 16 bit module may become a 32 bit module, inferred by
                              // ..looking at 1st sample record that matches the module selector
   U16    loadEvent   :  1;   // 0 for load, 1 for unload
   U16    processed   :  1;   // 0 for load, 1 for unload
   U16    reserved0   : 12;

   U16    selector;           // code selector or V86 segment
   U16    segmentNameLength;  // length of the segment name if the segmentNameSet bit is set
   U32    segmentNumber;      // segment number, Win95 (and now Java) can have multiple pieces for one module
   union {
      U32 flags;                            // all the flags as one dword
      struct {
         U32 exe                     : 1;   // this module is an exe
         U32 globalModule            : 1;   // globally loaded module.  There may be multiple
                                            // module records for a global module, but the samples
                                            // will only point to the 1st one, the others will be
                                            // ignored.  NT's Kernel32 is an example of this.
                                            // REVISIT this??
         U32 bogusWin95              : 1;   // "bogus" win95 module.  By bogus, we mean a
                                            // module that has a pid of 0, no length and no base.
                                            // Selector actually used as a 32 bit module.
         U32 pidRecIndexRaw          : 1;   // pidRecIndex is raw OS pid
         U32 sampleFound             : 1;   // at least one sample referenced this module
         U32 tscUsed                 : 1;   // tsc set when record written
         U32 duplicate               : 1;   // 1st pass analysis has determined this is a
                                            // duplicate load
         U32 globalModuleTB5         : 1;   // module mapped into all processes on system
         U32 segmentNameSet          : 1;   // set if the segment name was collected
                                            // (initially done for xbox collections)
         U32 firstModuleRecInProcess : 1;   // if the pidCreatesTrackedInModuleRecs flag is set
                                            //  in the SampleHeaderEx struct and this flag
                                            //  is set, the associated module indicates
                                            //  the beginning of a new process
         U32  source                 : 1;   // 0 for path in target system, 1 for path in host system (offloaded)
         U32  reserved1              : 21;
      } s1;
   } u2;
   U64   length64;         // module length
   U64   loadAddr64;       // load address
   U32   pidRecIndex;      // process ID rec index (index into  start of pid record section).
                           // .. (see pidRecIndexRaw).  If pidRecIndex == 0 and pidRecIndexRaw == 1
                           // ..then this is a kernel or global module.  Can validly
                           // ..be 0 if not raw (array index).  Use ReturnPid() to access this
                           // ..field
   U32   reserved2;
   U64   unloadTsc;        // TSC collected on an unload event
   U32   path;             // module path name (section offset on disk)
                           // ..when initally written by sampler name is at end of this
                           // ..struct, when merged with main file names are pooled at end
                           // ..of ModuleRecord Section so ModulesRecords can be
                           // ..fixed length
   U16   pathLength;       // path name length (inludes terminating \0)
   U16   filenameOffset;   // offset into path name of base filename
   U32   segmentName;      // offset to the segmentName from the beginning of the
                           //  module section in a processed module section
                           //  (s/b 0 in a raw module record)
                           // in a raw module record, the segment name will follow the
                           //  module name and the module name's terminating NULL char
   U32   reserved3;
   U64   tsc;              // time stamp counter module event occurred
   U32   parent_pid;       // Parent PID of the process
   U32   reserved4;
} ModuleRecord;

#define MR_unloadTscSet(x,y)        (x)->unloadTsc = (y)
#define MR_unloadTscGet(x)          (x)->unloadTsc

// Accessor macros for ModuleRecord
#define MODULE_RECORD_rec_length(x)                     (x)->recLength
#define MODULE_RECORD_segment_type(x)                   (x)->segmentType
#define MODULE_RECORD_load_event(x)                     (x)->loadEvent
#define MODULE_RECORD_processed(x)                      (x)->processed
#define MODULE_RECORD_selector(x)                       (x)->selector
#define MODULE_RECORD_segment_name_length(x)            (x)->segmentNameLength
#define MODULE_RECORD_segment_number(x)                 (x)->segmentNumber
#define MODULE_RECORD_flags(x)                          (x)->u2.flags
#define MODULE_RECORD_exe(x)                            (x)->u2.s1.exe
#define MODULE_RECORD_global_module(x)                  (x)->u2.s1.globalModule
#define MODULE_RECORD_bogus_win95(x)                    (x)->u2.s1.bogusWin95
#define MODULE_RECORD_pid_rec_index_raw(x)              (x)->u2.s1.pidRecIndexRaw
#define MODULE_RECORD_sample_found(x)                   (x)->u2.s1.sampleFound
#define MODULE_RECORD_tsc_used(x)                       (x)->u2.s1.tscUsed
#define MODULE_RECORD_duplicate(x)                      (x)->u2.s1.duplicate
#define MODULE_RECORD_global_module_tb5(x)              (x)->u2.s1.globalModuleTB5
#define MODULE_RECORD_segment_name_set(x)               (x)->u2.s1.segmentNameSet
#define MODULE_RECORD_first_module_rec_in_process(x)    (x)->u2.s1.firstModuleRecInProcess
#define MODULE_RECORD_source(x)                         (x)->u2.s1.source
#define MODULE_RECORD_length64(x)                       (x)->length64
#define MODULE_RECORD_load_addr64(x)                    (x)->loadAddr64
#define MODULE_RECORD_pid_rec_index(x)                  (x)->pidRecIndex
#define MODULE_RECORD_load_sample_count(x)              (x)->u5.s2.loadSampleCount
#define MODULE_RECORD_unload_sample_count(x)            (x)->u5.s2.unloadSampleCount
#define MODULE_RECORD_unload_tsc(x)                     (x)->unloadTsc
#define MODULE_RECORD_path(x)                           (x)->path
#define MODULE_RECORD_path_length(x)                    (x)->pathLength
#define MODULE_RECORD_filename_offset(x)                (x)->filenameOffset
#define MODULE_RECORD_segment_name(x)                   (x)->segmentName
#define MODULE_RECORD_tsc(x)                            (x)->tsc
#define MODULE_RECORD_parent_pid(x)                     (x)->parent_pid

/*
 *  The VTSA_SYS_INFO_STRUCT information that is shared across kernel mode
 *  and user mode code, very specifically for tb5 file generation
 */

typedef enum {
    GT_UNK     = 0,
    GT_PER_CPU,
    GT_PER_CHIPSET,
    GT_CPUID,
    GT_NODE,
    GT_SYSTEM,
    GT_SAMPLE_RECORD_INFO
} GEN_ENTRY_TYPES;

typedef enum {
    GST_UNK = 0,
    GST_X86,
    GST_ITANIUM,
    GST_SA,//strong arm
    GST_XSC,
    GST_EM64T,
    GST_CS860
} GEN_ENTRY_SUBTYPES;

typedef struct __fixed_size_pointer {
    union  {
        U64     fs_force_alignment;
        struct {
            U32     fs_unused;
            U32     is_ptr:1;
        } s1;
    } u1;
    union {
        U64     fs_offset;
        void   *fs_ptr;
    } u2;
} VTSA_FIXED_SIZE_PTR;

#define VTSA_FIXED_SIZE_PTR_is_ptr(fsp)     (fsp)->u1.s1.is_ptr
#define VTSA_FIXED_SIZE_PTR_fs_offset(fsp)  (fsp)->u2.fs_offset
#define VTSA_FIXED_SIZE_PTR_fs_ptr(fsp)     (fsp)->u2.fs_ptr


typedef struct __generic_array_header {
    //
    // Information realted to the generic header
    //
    U32 hdr_size;       // size of this generic header
                        // (for versioning and real data starts
                        //  after the header)

    U32 next_field_hdr_padding;    // make sure the next field is 8-byte aligned

    //
    // VTSA_FIXED_SIZE_PTR should always be on an 8-byte boundary...
    //
    // pointer to the next generic header if there is one
    //
    VTSA_FIXED_SIZE_PTR hdr_next_gen_hdr;

    U32 hdr_reserved[7];     // padding for future use - force to 64 bytes...

    //
    // Information related to the array this header is describing
    //
    U32 array_num_entries;
    U32 array_entry_size;
    U16 array_type;         // from the GEN_ENTRY_TYPES enumeration
    U16 array_subtype;      // from the GEN_ENTRY_SUBTYPES enumeration
} VTSA_GEN_ARRAY_HDR;

#define VTSA_GEN_ARRAY_HDR_hdr_size(gah)            (gah)->hdr_size
#define VTSA_GEN_ARRAY_HDR_hdr_next_gen_hdr(gah)    (gah)->hdr_next_gen_hdr
#define VTSA_GEN_ARRAY_HDR_array_num_entries(gah)   (gah)->array_num_entries
#define VTSA_GEN_ARRAY_HDR_array_entry_size(gah)    (gah)->array_entry_size
#define VTSA_GEN_ARRAY_HDR_array_type(gah)          (gah)->array_type
#define VTSA_GEN_ARRAY_HDR_array_subtype(gah)       (gah)->array_subtype

typedef struct __cpuid_x86 {
    U32 cpuid_eax_input;
    U32 cpuid_eax;
    U32 cpuid_ebx;
    U32 cpuid_ecx;
    U32 cpuid_edx;
} VTSA_CPUID_X86;

#define VTSA_CPUID_X86_cpuid_eax_input(cid) (cid)->cpuid_eax_input
#define VTSA_CPUID_X86_cpuid_eax(cid)       (cid)->cpuid_eax
#define VTSA_CPUID_X86_cpuid_ebx(cid)       (cid)->cpuid_ebx
#define VTSA_CPUID_X86_cpuid_ecx(cid)       (cid)->cpuid_ecx
#define VTSA_CPUID_X86_cpuid_edx(cid)       (cid)->cpuid_edx

typedef struct __cpuid_ipf {
    U64 cpuid_select;
    U64 cpuid_val;
} VTSA_CPUID_IPF;

#define VTSA_CPUID_IPF_cpuid_select(cid)    (cid)->cpuid_select
#define VTSA_CPUID_IPF_cpuid_val(cid)       (cid)->cpuid_val

typedef struct __generic_per_cpu {
    //
    // per cpu information
    //
    U32 cpu_number;             // cpu number (as defined by the OS)
    U32 cpu_speed_mhz;          // cpu speed (in Mhz)
    U32 cpu_fsb_mhz;            // Front Side Bus speed (in Mhz) (if known)
    U32 cpu_cache_L2;           // ??? USER: cpu L2 (marketing definition) cache size (if known)

    //
    // And pointer to other structures. Keep this on an 8-byte boundary
    //
    // "pointer" to generic array header that should contain
    // cpuid information for this cpu
    //
    VTSA_FIXED_SIZE_PTR cpu_cpuid_array;

    S64 cpu_tsc_offset;         // TSC offset from CPU 0 computed as (TSC CPU N - TSC CPU 0)
    //
    // intel processor number (from mkting).
    // Currently 3 decimal digits (3xx, 5xx and 7xx)
    //
    U32 cpu_intel_processor_number;

    U32 cpu_cache_L3;           // ??? USER: cpu L3 (marketing definition) cache size (if known)

    U64 platform_id;

    //
    // package/mapping information
    //
    // The hierarchy for uniquely identifying a logical processor
    // in a system is node number/id (from the node structure),
    // package number, core number, and thread number.
    // Core number is for identifying a core within a package.
    //
    // Actually, on Itanium getting all this information is
    // pretty involved with complicated algorithm using PAL calls.
    // I don't know how important all this stuff is to the user.
    // Maybe we can just have the place holder now and figure out
    // how to fill them later.
    //
    U16 cpu_package_num;            // package number for this cpu (if known)
    U16 cpu_core_num;               // core number (if known)
    U16 cpu_hw_thread_num;          // hw thread number inside the core (if known)

    U16 cpu_threads_per_core;       // total number of h/w threads per core (if known)
    U16 cpu_module_id;               // Processor module number
    U16 cpu_num_modules;             // Number of processor modules
    U32 reserved;

} VTSA_GEN_PER_CPU;

#define VTSA_GEN_PER_CPU_cpu_number(p_cpu)                  (p_cpu)->cpu_number
#define VTSA_GEN_PER_CPU_cpu_speed_mhz(p_cpu)               (p_cpu)->cpu_speed_mhz
#define VTSA_GEN_PER_CPU_cpu_fsb_mhz(p_cpu)                 (p_cpu)->cpu_fsb_mhz
#define VTSA_GEN_PER_CPU_cpu_cache_L2(p_cpu)                (p_cpu)->cpu_cache_L2
#define VTSA_GEN_PER_CPU_cpu_cpuid_array(p_cpu)             (p_cpu)->cpu_cpuid_array
#define VTSA_GEN_PER_CPU_cpu_tsc_offset(p_cpu)              (p_cpu)->cpu_tsc_offset
#define VTSA_GEN_PER_CPU_cpu_intel_processor_number(p_cpu)  (p_cpu)->cpu_intel_processor_number
#define VTSA_GEN_PER_CPU_cpu_cache_L3(p_cpu)                (p_cpu)->cpu_cache_L3
#define VTSA_GEN_PER_CPU_platform_id(p_cpu)                 (p_cpu)->platform_id
#define VTSA_GEN_PER_CPU_cpu_package_num(p_cpu)             (p_cpu)->cpu_package_num
#define VTSA_GEN_PER_CPU_cpu_core_num(p_cpu)                (p_cpu)->cpu_core_num
#define VTSA_GEN_PER_CPU_cpu_hw_thread_num(p_cpu)           (p_cpu)->cpu_hw_thread_num
#define VTSA_GEN_PER_CPU_cpu_threads_per_core(p_cpu)        (p_cpu)->cpu_threads_per_core
#define VTSA_GEN_PER_CPU_cpu_module_num(p_cpu)              (p_cpu)->cpu_module_id
#define VTSA_GEN_PER_CPU_cpu_num_modules(p_cpu)             (p_cpu)->cpu_num_modules


typedef struct __node_info {
    U32 node_type_from_shell;
    U32 node_id;                   // The node number/id (if known)

    U32 node_num_available;        // total number cpus on this node
    U32 node_num_used;             // USER: number used based on cpu mask at time of run

    U64 node_physical_memory;      // amount of physical memory (bytes) on this node

    //
    // pointer to the first generic header that
    // contains the per-cpu information
    //
    // Keep the VTSA_FIXED_SIZE_PTR on an 8-byte boundary...
    //
    VTSA_FIXED_SIZE_PTR node_percpu_array;

    U32 node_reserved[2];           // leave some space

} VTSA_NODE_INFO;

#define VTSA_NODE_INFO_node_type_from_shell(vni)    (vni)->node_type_from_shell
#define VTSA_NODE_INFO_node_id(vni)                 (vni)->node_id
#define VTSA_NODE_INFO_node_num_available(vni)      (vni)->node_num_available
#define VTSA_NODE_INFO_node_num_used(vni)           (vni)->node_num_used
#define VTSA_NODE_INFO_node_physical_memory(vni)    (vni)->node_physical_memory
#define VTSA_NODE_INFO_node_percpu_array(vni)       (vni)->node_percpu_array


typedef struct __sys_info {
    //
    // Keep this on an 8-byte boundary
    //
    VTSA_FIXED_SIZE_PTR node_array;  // the per-node information

    U64 min_app_address;         // USER: lower allowed user space address (if known)
    U64 max_app_address;         // USER: upper allowed user space address (if known)
    U32 page_size;               // Current page size
    U32 allocation_granularity;  // USER: Granularity of allocation requests (if known)

    U32 reserved[2];              // leave some space

} VTSA_SYS_INFO;

#define VTSA_SYS_INFO_node_array(sys_info)                (sys_info)->node_array
#define VTSA_SYS_INFO_min_app_address(sys_info)           (sys_info)->min_app_address
#define VTSA_SYS_INFO_max_app_address(sys_info)           (sys_info)->max_app_address
#define VTSA_SYS_INFO_page_size(sys_info)                 (sys_info)->page_size
#define VTSA_SYS_INFO_allocation_granularity(sys_info)    (sys_info)->allocation_granularity

typedef struct DRV_TOPOLOGY_INFO_NODE_S DRV_TOPOLOGY_INFO_NODE;
typedef        DRV_TOPOLOGY_INFO_NODE  *DRV_TOPOLOGY_INFO;

struct DRV_TOPOLOGY_INFO_NODE_S {
    U32 cpu_number;                 // cpu number (as defined by the OS)
    U16 cpu_package_num;            // package number for this cpu (if known)
    U16 cpu_core_num;               // core number (if known)
    U16 cpu_hw_thread_num;          // T0 or T1 if HT enabled
    S32 socket_master;
    S32 core_master;
    S32 thr_master;
    U32 cpu_module_num;
    U32 cpu_module_master;
    U32 cpu_num_modules;
} ;

#define DRV_TOPOLOGY_INFO_cpu_number(dti)          (dti)->cpu_number
#define DRV_TOPOLOGY_INFO_cpu_package_num(dti)     (dti)->cpu_package_num
#define DRV_TOPOLOGY_INFO_cpu_core_num(dti)        (dti)->cpu_core_num
#define DRV_TOPOLOGY_INFO_socket_master(dti)       (dti)->socket_master
#define DRV_TOPOLOGY_INFO_core_master(dti)         (dti)->core_master
#define DRV_TOPOLOGY_INFO_thr_master(dti)          (dti)->thr_master
#define DRV_TOPOLOGY_INFO_cpu_hw_thread_num(dti)   (dti)->cpu_hw_thread_num
#define DRV_TOPOLOGY_INFO_cpu_module_num(dti)      (dti)->cpu_module_num
#define DRV_TOPOLOGY_INFO_cpu_module_master(dti)   (dti)->cpu_module_master
#define DRV_TOPOLOGY_INFO_cpu_num_modules(dti)     (dti)->cpu_num_modules

//platform information. need to get from driver
typedef struct DRV_PLATFORM_INFO_NODE_S DRV_PLATFORM_INFO_NODE;
typedef        DRV_PLATFORM_INFO_NODE  *DRV_PLATFORM_INFO;
 
struct DRV_PLATFORM_INFO_NODE_S {
    U64 info;                     // platform info
    U64 ddr_freq_index;           // freq table index
};
#define DRV_PLATFORM_INFO_info(data)           (data)->info
#define DRV_PLATFORM_INFO_ddr_freq_index(data) (data)->ddr_freq_index
 
//platform information. need to get from Platform picker
typedef struct PLATFORM_FREQ_INFO_NODE_S PLATFORM_FREQ_INFO_NODE;
typedef        PLATFORM_FREQ_INFO_NODE  *PLATFORM_FREQ_INFO;
 
struct PLATFORM_FREQ_INFO_NODE_S {
    float   multiplier;          // freq multiplier
    double *table;               // freq table
    U32     table_size;          // freq table size
};
#define PLATFORM_FREQ_INFO_multiplier(data)       (data)->multiplier
#define PLATFORM_FREQ_INFO_table(data)            (data)->table
#define PLATFORM_FREQ_INFO_table_size(data)       (data)->table_size
              
// Definitions for user markers data
// The instances of these structures will be written to the user markers temp file.
#define MARKER_DEFAULT_TYPE   "Default_Marker"
#define MARKER_DEFAULT_ID     0
#define MAX_MARKER_LENGTH     136

#define MARK_ID     4
#define MARK_DATA   2
#define THREAD_INFO 8

/* do not use it at ths moment
typedef enum {
        SMRK_USER_DEFINED = 0,
        SMRK_THREAD_NAME,
        SMRK_WALLCLOCK,
        SMRK_TEXT,
        SMRK_TYPE_ID
}  SMRK_TYPE;
*/

typedef struct _MarkerType {
        U32      type;               // The type is the SMRK_TYPE - one of SMRK_TYPE_ID or SMRK_WALLCLOCK
                                     // helps in identifying the marker data structure in the temp file
} MarkerType;

#define MARKER_TYPE_type(pdata)             (pdata)->type

typedef struct _MarkerIdentifierData {
   U32      marker_id;                      // Marker Unique Identifier
   U32      marker_name_length;             // Marker Name length
   char     marker_name[136];               // Marker Name
} MarkerIdentifierData, *PMarkerIdentifierData;

#define MARKER_ID_id(pdata)               (pdata)->marker_id
#define MARKER_ID_name_len(pdata)         (pdata)->marker_name_length
#define MARKER_ID_name(pdata)             (pdata)->marker_name


#define MAX_THREAD_NAME       120

////////////////////////////////
// THREAD_NAME info structure
// this structure contains information needed to support thread naming
////////////////////////////////
/*!\struct THREAD_NAME_INFO_NODE
 * \var thread_name_length        Thread Name Length
 * \var pid                       Process id of the process the thread belongs to
 * \var tid                       Thread id of the thread
 * \var tsc                       Time stamp when this information was added
 * \var thread_name               Thread Name
 *
 * \brief Instances of these structures will be written to the output temp file
 */

typedef struct THREAD_NAME_INFO_NODE_S THREAD_NAME_INFO_NODE;
typedef        THREAD_NAME_INFO_NODE  *THREAD_NAME_INFO;

struct THREAD_NAME_INFO_NODE_S {
        U32     thread_name_length;
        U64     pid;
        U64     tid;
        U64     tsc;
        char    thread_name[MAX_THREAD_NAME];
};

#define THREAD_NAME_pid(tinfo)                        (tinfo)->pid
#define THREAD_NAME_tid(tinfo)                        (tinfo)->tid
#define THREAD_NAME_tsc(tinfo)                        (tinfo)->tsc
#define THREAD_NAME_thread_name(tinfo)                (tinfo)->thread_name
#define THREAD_NAME_thread_name_length(tinfo)         (tinfo)->thread_name_length

// This structure contains the user marker wallclock information
// The instances of these structures will be written to the user markers temp file.
typedef struct _MarkerWallClockData {
    U8      end_marker;                     // End Marker Flag in case of continuous or range marker
    U32     marker_id;                      // Marker Unique Identifier, maps to the marker_id member in MarkerIdentifierData structure
    U64     tsc;                            // tsc at the marker was added
    U64     wallclock;                      // wallclock information
} MarkerWallClockData, *PMarkerWallClockData;

#define MARKER_DATA_marker_id(pdata)       (pdata)->marker_id
#define MARKER_DATA_end_marker(pdata)      (pdata)->end_marker
#define MARKER_DATA_tsc(pdata)             (pdata)->tsc
#define MARKER_DATA_wallclock(pdata)       (pdata)->wallclock


#define OSSNAMELEN      64
#define OSNAMELEN       128

typedef struct _SOFTWARE_INFO_NODE_S SOFTWARE_INFO_NODE;
typedef SOFTWARE_INFO_NODE          *SOFTWARE_INFO;

struct _SOFTWARE_INFO_NODE_S {
    char sysname[OSSNAMELEN];
    char hostname[OSSNAMELEN];
    char ipaddr[OSSNAMELEN];
    char osname[OSNAMELEN];
    char release[OSSNAMELEN];
    char machine[OSSNAMELEN];
};

#define SOFTWARE_INFO_sysname(info)        (info)->sysname
#define SOFTWARE_INFO_hostname(info)       (info)->hostname
#define SOFTWARE_INFO_ipaddr(info)         (info)->ipaddr
#define SOFTWARE_INFO_osname(info)         (info)->osname
#define SOFTWARE_INFO_release(info)        (info)->release
#define SOFTWARE_INFO_machine(info)        (info)->machine


/*
 *  Common Register descriptions
 */

#if defined(DRV_IA32) || defined(DRV_EM64T)

/*
 *  Bits used in the debug control register
 */
#define DEBUG_CTL_LBR                          0x0000001
#define DEBUG_CTL_BTF                          0x0000002
#define DEBUG_CTL_TR                           0x0000040
#define DEBUG_CTL_BTS                          0x0000080
#define DEBUG_CTL_BTINT                        0x0000100
#define DEBUG_CTL_BT_OFF_OS                    0x0000200
#define DEBUG_CTL_BTS_OFF_USR                  0x0000400
#define DEBUG_CTL_FRZ_LBR_ON_PMI               0x0000800
#define DEBUG_CTL_FRZ_PMON_ON_PMI              0x0001000
#define DEBUG_CTL_ENABLE_UNCORE_PMI_BIT        0x0002000


#define DEBUG_CTL_NODE_lbr_get(reg)                   (reg) &   DEBUG_CTL_LBR
#define DEBUG_CTL_NODE_lbr_set(reg)                   (reg) |=  DEBUG_CTL_LBR
#define DEBUG_CTL_NODE_lbr_clear(reg)                 (reg) &= ~DEBUG_CTL_LBR

#define DEBUG_CTL_NODE_btf_get(reg)                   (reg) &   DEBUG_CTL_BTF
#define DEBUG_CTL_NODE_btf_set(reg)                   (reg) |=  DEBUG_CTL_BTF
#define DEBUG_CTL_NODE_btf_clear(reg)                 (reg) &= ~DEBUG_CTL_BTF

#define DEBUG_CTL_NODE_tr_get(reg)                    (reg) &   DEBUG_CTL_TR
#define DEBUG_CTL_NODE_tr_set(reg)                    (reg) |=  DEBUG_CTL_TR
#define DEBUG_CTL_NODE_tr_clear(reg)                  (reg) &= ~DEBUG_CTL_TR

#define DEBUG_CTL_NODE_bts_get(reg)                   (reg) &   DEBUG_CTL_BTS
#define DEBUG_CTL_NODE_bts_set(reg)                   (reg) |=  DEBUG_CTL_BTS
#define DEBUG_CTL_NODE_bts_clear(reg)                 (reg) &= ~DEBUG_CTL_BTS

#define DEBUG_CTL_NODE_btint_get(reg)                 (reg) &   DEBUG_CTL_BTINT
#define DEBUG_CTL_NODE_btint_set(reg)                 (reg) |=  DEBUG_CTL_BTINT
#define DEBUG_CTL_NODE_btint_clear(reg)               (reg) &= ~DEBUG_CTL_BTINT

#define DEBUG_CTL_NODE_bts_off_os_get(reg)            (reg) &   DEBUG_CTL_BTS_OFF_OS
#define DEBUG_CTL_NODE_bts_off_os_set(reg)            (reg) |=  DEBUG_CTL_BTS_OFF_OS
#define DEBUG_CTL_NODE_bts_off_os_clear(reg)          (reg) &= ~DEBUG_CTL_BTS_OFF_OS

#define DEBUG_CTL_NODE_bts_off_usr_get(reg)           (reg) &   DEBUG_CTL_BTS_OFF_USR
#define DEBUG_CTL_NODE_bts_off_usr_set(reg)           (reg) |=  DEBUG_CTL_BTS_OFF_USR
#define DEBUG_CTL_NODE_bts_off_usr_clear(reg)         (reg) &= ~DEBUG_CTL_BTS_OFF_USR

#define DEBUG_CTL_NODE_frz_lbr_on_pmi_get(reg)        (reg) &   DEBUG_CTL_FRZ_LBR_ON_PMI
#define DEBUG_CTL_NODE_frz_lbr_on_pmi_set(reg)        (reg) |=  DEBUG_CTL_FRZ_LBR_ON_PMI
#define DEBUG_CTL_NODE_frz_lbr_on_pmi_clear(reg)      (reg) &= ~DEBUG_CTL_FRZ_LBR_ON_PMI

#define DEBUG_CTL_NODE_frz_pmon_on_pmi_get(reg)       (reg) &   DEBUG_CTL_FRZ_PMON_ON_PMI
#define DEBUG_CTL_NODE_frz_pmon_on_pmi_set(reg)       (reg) |=  DEBUG_CTL_FRZ_PMON_ON_PMI
#define DEBUG_CTL_NODE_frz_pmon_on_pmi_clear(reg)     (reg) &= ~DEBUG_CTL_FRZ_PMON_ON_PMI

#define DEBUG_CTL_NODE_enable_uncore_pmi_get(reg)     (reg) &   DEBUG_CTL_ENABLE_UNCORE_PMI
#define DEBUG_CTL_NODE_enable_uncore_pmi_set(reg)     (reg) |=  DEBUG_CTL_ENABLE_UNCORE_PMI
#define DEBUG_CTL_NODE_enable_uncore_pmi_clear(reg)   (reg) &= ~DEBUG_CTL_ENABLE_UNCORE_PMI

#endif /* defined(DRV_IA32) || defined(DRV_EM64T) */

/*
 * @macro SEP_VERSION_NODE_S
 * @brief
 * This structure supports versioning in Sep. The field major indicates the major version,
 * minor indicates the minor version and api indicates the api version for the current
 * sep build. This structure is initialized at the time when the driver is loaded.
 */

typedef struct SEP_VERSION_NODE_S  SEP_VERSION_NODE;
typedef        SEP_VERSION_NODE   *SEP_VERSION;

struct SEP_VERSION_NODE_S {
    union {
        U32      sep_version;
        struct {
            S32  major:8;
            S32  minor:8;
            S32  api:16;
        }s1;
    }u1;
};

#define SEP_VERSION_NODE_sep_version(version) (version)->u1.sep_version
#define SEP_VERSION_NODE_major(version)       (version)->u1.s1.major
#define SEP_VERSION_NODE_minor(version)       (version)->u1.s1.minor
#define SEP_VERSION_NODE_api(version)         (version)->u1.s1.api

typedef struct DEVICE_INFO_NODE_S  DEVICE_INFO_NODE;
typedef        DEVICE_INFO_NODE   *DEVICE_INFO;

struct DEVICE_INFO_NODE_S {
    S8                 *dll_name;
    PVOID               dll_handle;
    S8                 *cpu_name;
    S8                 *pmu_name;
    S8                 *event_db_file_name;
    //PLATFORM_IDENTITY plat_identity;  // this is undefined right now. Please take this as structure containing U64
    U32                 plat_type;      // device type (e.g., DEVICE_INFO_CORE, etc. ... see enum below)
    U32                 plat_sub_type;  // cti_type (e.g., CTI_Sandybridge, etc., ... see env_info_types.h)
    S32                 dispatch_id;    // this will be set in user mode dlls and will be unique across all IPF, IA32 (including MIDS).
    ECB                *ecb;
    EVENT_CONFIG        ec;
    DRV_CONFIG          pcfg;
    U32                 num_of_groups;
    U32                 size_of_alloc;  // size of each event control block
    PVOID               drv_event;
    U32                 num_events;
    U32                 event_id_index; // event id index of device (basically how many events processed before this device)
    U32                 num_counters;
    U32                 group_index;
    U32                 num_packages;
    U32                 num_units;
};

#define MAX_EVENT_NAME_LENGTH 64

#define DEVICE_INFO_dll_name(pdev)                  (pdev)->dll_name
#define DEVICE_INFO_dll_handle(pdev)                (pdev)->dll_handle
#define DEVICE_INFO_cpu_name(pdev)                  (pdev)->cpu_name
#define DEVICE_INFO_pmu_name(pdev)                  (pdev)->pmu_name
#define DEVICE_INFO_event_db_file_name(pdev)        (pdev)->event_db_file_name
#define DEVICE_INFO_plat_type(pdev)                 (pdev)->plat_type
#define DEVICE_INFO_plat_sub_type(pdev)             (pdev)->plat_sub_type
#define DEVICE_INFO_dispatch_id(pdev)               (pdev)->dispatch_id
#define DEVICE_INFO_ecb(pdev)                       (pdev)->ecb
#define DEVICE_INFO_ec(pdev)                        (pdev)->ec
#define DEVICE_INFO_pcfg(pdev)                      (pdev)->pcfg
#define DEVICE_INFO_num_groups(pdev)                (pdev)->num_of_groups
#define DEVICE_INFO_size_of_alloc(pdev)             (pdev)->size_of_alloc
#define DEVICE_INFO_drv_event(pdev)                 (pdev)->drv_event
#define DEVICE_INFO_num_events(pdev)                (pdev)->num_events
#define DEVICE_INFO_event_id_index(pdev)            (pdev)->event_id_index
#define DEVICE_INFO_num_counters(pdev)              (pdev)->num_counters
#define DEVICE_INFO_group_index(pdev)               (pdev)->group_index
#define DEVICE_INFO_num_packages(pdev)              (pdev)->num_packages
#define DEVICE_INFO_num_units(pdev)                 (pdev)->num_units


typedef struct DEVICE_INFO_DATA_NODE_S DEVICE_INFO_DATA_NODE;
typedef        DEVICE_INFO_DATA_NODE  *DEVICE_INFO_DATA;

struct DEVICE_INFO_DATA_NODE_S {
    DEVICE_INFO         pdev_info;
    U32                 num_elements;
    U32                 num_allocated;
};

#define DEVICE_INFO_DATA_pdev_info(d)           (d)->pdev_info
#define DEVICE_INFO_DATA_num_elements(d)        (d)->num_elements
#define DEVICE_INFO_DATA_num_allocated(d)       (d)->num_allocated

typedef enum
{
    DEVICE_INFO_CORE        =   0,
    DEVICE_INFO_UNCORE      =   1,
    DEVICE_INFO_CHIPSET     =   2,
    DEVICE_INFO_GFX         =   3,
    DEVICE_INFO_PWR         =   4,
    DEVICE_INFO_TELEMETRY   =   5
}   DEVICE_INFO_TYPE;

#if defined(__cplusplus)
}
#endif


typedef struct DRV_EVENT_MASK_NODE_S  DRV_EVENT_MASK_NODE;
typedef        DRV_EVENT_MASK_NODE    *DRV_EVENT_MASK;

struct DRV_EVENT_MASK_NODE_S {
    U8 event_idx;    // 0 <= index < MAX_EVENTS
    union {
        U8 bitFields1;
        struct {
            U8 precise        : 1;
            U8 lbr_capture    : 1;
            U8 dear_capture   : 1;  // Indicates which events need to have additional registers read
                                    // because they are DEAR events.
            U8 iear_capture   : 1;  // Indicates which events need to have additional registers read
                                    // because they are IEAR events.
            U8 btb_capture    : 1;  // Indicates which events need to have additional registers read
                                    // because they are BTB events.
            U8 ipear_capture  : 1;  // Indicates which events need to have additional registers read
                                    // because they are IPEAR events.
            U8 uncore_capture : 1;
            U8 reserved0      : 2;
        } s1;
    } u1;
};

#define DRV_EVENT_MASK_event_idx(d)             (d)->event_idx
#define DRV_EVENT_MASK_bitFields1(d)            (d)->u1.bitFields1
#define DRV_EVENT_MASK_precise(d)               (d)->u1.s1.precise
#define DRV_EVENT_MASK_lbr_capture(d)           (d)->u1.s1.lbr_capture
#define DRV_EVENT_MASK_dear_capture(d)          (d)->u1.s1.dear_capture
#define DRV_EVENT_MASK_iear_capture(d)          (d)->u1.s1.iear_capture
#define DRV_EVENT_MASK_btb_capture(d)           (d)->u1.s1.btb_capture
#define DRV_EVENT_MASK_ipear_capture(d)         (d)->u1.s1.ipear_capture
#define DRV_EVENT_MASK_uncore_capture(d)        (d)->u1.s1.uncore_capture

#define MAX_OVERFLOW_EVENTS 11    // This defines the maximum number of overflow events per interrupt.
                                  // In order to reduce memory footprint, the value should be at least
                                  // the number of fixed and general PMU registers.
                                  // Sandybridge with HT off has 11 PMUs(3 fixed and 8 generic)

typedef struct DRV_MASKS_NODE_S  DRV_MASKS_NODE;
typedef        DRV_MASKS_NODE    *DRV_MASKS;

/*
 * @macro DRV_EVENT_MASK_NODE_S
 * @brief
 * The structure is used to store overflow events when handling PMU interrupt.
 * This approach should be more efficient than checking all event masks
 * if there are many events to be monitored
 * and only a few events among them have overflow per interrupt.
 */
struct DRV_MASKS_NODE_S {
    DRV_EVENT_MASK_NODE eventmasks[MAX_OVERFLOW_EVENTS];
    U8 masks_num;               // 0 <= mask_num <= MAX_OVERFLOW_EVENTS
    U8 padding;                 // data structure alignment
};

#define DRV_MASKS_masks_num(d)           (d)->masks_num
#define DRV_MASKS_eventmasks(d)          (d)->eventmasks

typedef struct EMON_SCHED_INFO_NODE_S   EMON_SCHED_INFO_NODE;
typedef        EMON_SCHED_INFO_NODE     *EMON_SCHED_INFO;

struct EMON_SCHED_INFO_NODE_S {
     U32   max_counters_for_all_pmus;
     U32   num_cpus;
     U32   group_index;
     U32   offset_for_next_device;
     U32   device_id;
     U32   num_packages;
     U32   num_units;
};

#define EMON_SCHED_INFO_max_counters_for_all_pmus(x)           (x)->max_counters_for_all_pmus
#define EMON_SCHED_INFO_num_cpus(x)                            (x)->num_cpus
#define EMON_SCHED_INFO_group_index(x)                (x)->group_index
#define EMON_SCHED_INFO_offset_for_next_device(x)     (x)->offset_for_next_device
#define EMON_SCHED_INFO_device_id(x)                           (x)->device_id
#define EMON_SCHED_INFO_num_packages(x)                        (x)->num_packages
#define EMON_SCHED_INFO_num_units(x)                           (x)->num_units

#endif

