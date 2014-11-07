/*
    TKB Thermal Coretemp
*/
#ifndef TKB_H_
#define TKB_H_

extern void tkb_update_coretemp(int index, int temp);
extern int tkb_get_coretemp_thr(int index, int val, int mask);
extern bool tkb_coretemp_trigger(
    int index,  int temp, int event,
    int *l_thr, int *h_thr, int *is_enabled);
extern void core_threshold_set(int low_thr, int high_thr);
#endif /* TKB_H_ */
