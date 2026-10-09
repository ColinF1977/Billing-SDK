#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stddef.h>
#include <signal.h>
#include "hyperion_billing.h"
static volatile sig_atomic_t stopping;
static void stop_requested(int signal_number){(void)signal_number;stopping=1;}
__attribute__((constructor)) static void install_stop_handler(void){signal(SIGUSR1,stop_requested);}
/* ABI adapter for the pinned MXL v1 writer. Only TSG's child engine preloads
 * this adapter. Every discrete grain from that engine is a video frame. */
int mxlFlowWriterOpenGrain(void *writer,uint64_t index,void *grain,void **payload) {
    if(stopping){hb_flush();return 4;}
    int (*open_grain)(void *,uint64_t,void *,void **)=dlsym(RTLD_NEXT,"mxlFlowWriterOpenGrain");
    if(!open_grain)return 1;
    if(!hb_begin())return 4;
    int result=open_grain(writer,index,grain,payload);if(result)hb_end(0);return result;
}
int mxlFlowWriterCancelGrain(void *writer) {
    int (*cancel)(void *)=dlsym(RTLD_NEXT,"mxlFlowWriterCancelGrain");
    if(!cancel)return 1;
    int result=cancel(writer);hb_end(0);if(stopping)hb_flush();return result;
}
int mxlFlowWriterCommitGrain(void *writer, const void *grain) {
    int (*commit)(void *,const void *)=dlsym(RTLD_NEXT,"mxlFlowWriterCommitGrain");
    if(!commit){hb_end(0);return 1;}
    int result=commit(writer,grain);hb_end(result==0);if(stopping)hb_flush();return result;
}
int mxlFlowWriterOpenSamples(void *writer,uint64_t index,size_t count,void *slices) {
    if(stopping){hb_flush();return 4;}
    int (*open_samples)(void *,uint64_t,size_t,void *)=dlsym(RTLD_NEXT,"mxlFlowWriterOpenSamples");
    if(!hb_allowed() || !open_samples)return 4;
    return open_samples(writer,index,count,slices);
}
