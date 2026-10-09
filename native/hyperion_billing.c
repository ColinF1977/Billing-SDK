#define _POSIX_C_SOURCE 200809L
#include "hyperion_billing.h"
#include <curl/curl.h>
#include <json-c/json.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t once=PTHREAD_ONCE_INIT;
static char rid[65], error[256], path[1024];
static int frames, used, flight, paused, recovered;
struct response { char data[32768]; size_t size; };
static size_t receive(char *p,size_t a,size_t b,void *v) { struct response *r=v; size_t n=a*b; if(n>=sizeof(r->data)-r->size)return 0; memcpy(r->data+r->size,p,n);r->size+=n;r->data[r->size]=0;return n; }
static void fail(const char *s) { paused=1;snprintf(error,sizeof(error),"%s",s); }
static json_object *request(const char *route,json_object *body) {
    const char *socket=getenv("HYPERION_BILLING_SOCKET"),*token=getenv("HYPERION_BILLING_TOKEN");
    if(!socket || !token || strlen(token)<32) {fail("Billing is not configured. Register this instance with the local billing service.");return NULL;}
    CURL *c=curl_easy_init(); if(!c){fail("Cannot initialize billing transport.");return NULL;}
    char url[256],auth[512];snprintf(url,sizeof(url),"http://localhost/v1/%s",route);snprintf(auth,sizeof(auth),"Authorization: Bearer %s",token);
    struct curl_slist *h=NULL;h=curl_slist_append(h,auth);h=curl_slist_append(h,"Content-Type: application/json");struct response r={0};
    curl_easy_setopt(c,CURLOPT_URL,url);curl_easy_setopt(c,CURLOPT_UNIX_SOCKET_PATH,socket);curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,2000L);curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(c,CURLOPT_WRITEDATA,&r);
    if(body)curl_easy_setopt(c,CURLOPT_POSTFIELDS,json_object_to_json_string_ext(body,JSON_C_TO_STRING_PLAIN));
    CURLcode rc=curl_easy_perform(c);long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);curl_easy_cleanup(c);curl_slist_free_all(h);
    if(rc!=CURLE_OK || status<200 || status>=300){fail(status==409?"Shared credit exhausted or pending. Add credit and press Resume billing.":"Billing unavailable. Check the local service and press Resume billing.");return NULL;}
    return r.size?json_tokener_parse(r.data):json_object_new_object();
}
static int persist(void) {
    char temp[1100];snprintf(temp,sizeof(temp),"%s.tmp",path);
    int fd=open(temp,O_WRONLY|O_CREAT|O_TRUNC,0600);if(fd<0){fail("Cannot save billing journal.");return 0;}
    char data[512];int n=snprintf(data,sizeof(data),"{\"id\":\"%s\",\"frames\":%d,\"used\":%d,\"flight\":%d,\"paused\":%d}",rid,frames,used,flight,paused);
    int ok=write(fd,data,(size_t)n)==n && fsync(fd)==0;close(fd);
    if(!ok || rename(temp,path)){fail("Cannot commit billing journal.");return 0;}
    char parent[1024];snprintf(parent,sizeof(parent),"%s",path);char *slash=strrchr(parent,'/');if(slash){*slash=0;int dir=open(parent,O_RDONLY);if(dir>=0){fsync(dir);close(dir);}}
    return 1;
}
static void init(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    const char *dir=getenv("HYPERION_DATA");if(!dir)dir="/data";snprintf(path,sizeof(path),"%s/billing-journal.json",dir);
    json_object *j=json_object_from_file(path);
    if(j){json_object *v;if(json_object_object_get_ex(j,"id",&v))snprintf(rid,sizeof(rid),"%s",json_object_get_string(v));
        if(json_object_object_get_ex(j,"frames",&v))frames=json_object_get_int(v);
        if(json_object_object_get_ex(j,"used",&v))used=json_object_get_int(v);
        if(json_object_object_get_ex(j,"flight",&v))flight=json_object_get_int(v);
        if(json_object_object_get_ex(j,"paused",&v))paused=json_object_get_int(v);
        json_object_put(j);
        if(flight){recovered=1;fail("Interrupted batch has a pending reservation. Press Resume billing to continue with a new reservation.");}
        else if(paused)fail("Billing paused. Check shared credit and press Resume billing.");
    } else if(access(path,F_OK)==0)fail("Billing journal is damaged; restore it before processing.");
}
static int settle(int close) {
    if(!rid[0])return 1;
    /* Checkpoint known completions before an idempotent service request. */
    if(!persist())return 0;
    json_object *j=json_object_new_object();json_object_object_add(j,"id",json_object_new_string(rid));json_object_object_add(j,"used",json_object_new_int(used));json_object_object_add(j,"close",json_object_new_boolean(close));
    json_object *r=request("meter/settle",j);json_object_put(j);if(!r)return 0;json_object_put(r);
    if(close){rid[0]=0;frames=used=0;return persist();}return 1;
}
static int reserve(void) {
    if(rid[0] && frames>0){if(!settle(1))return 0;}
    if(!rid[0]) {unsigned char b[16];FILE *f=fopen("/dev/urandom","rb");if(!f)return 0;size_t n=fread(b,1,16,f);fclose(f);if(n!=16)return 0;
        snprintf(rid,sizeof(rid),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);if(!persist())return 0;}
    json_object *j=json_object_new_object();json_object_object_add(j,"id",json_object_new_string(rid));json_object_object_add(j,"frames",json_object_new_int(24));json_object *r=request("meter/reserve",j);json_object_put(j);if(!r)return 0;
    json_object *v;int closed=json_object_object_get_ex(r,"closed",&v)&&json_object_get_boolean(v);
    frames=json_object_object_get_ex(r,"frames",&v)?json_object_get_int(v):0;json_object_put(r);
    if(closed || frames<1){fail("Reservation already closed or invalid.");return 0;}
    /* Durable intent covers the entire batch. No disk or network operations
     * occur on ordinary per-frame begin/end calls. A crash before settlement
     * leaves this bounded batch pending instead of refunding possible usage. */
    flight=1;int ok=persist();flight=0;return ok;
}
int hb_begin(void) {
    pthread_once(&once,init);
    hb_allowed(); /* Apply an explicit resume request before taking a permit. */
    pthread_mutex_lock(&mutex);int ok=0;
    if(!paused && !flight && (used<frames || reserve())){flight=1;ok=1;}
    if(!ok && !paused)fail("Billing frame gate unavailable.");
    if(!ok)persist();
    pthread_mutex_unlock(&mutex);return ok;
}
void hb_end(int success) {
    pthread_once(&once,init);pthread_mutex_lock(&mutex);
    if(flight){if(success)used++;flight=0;if(used==frames && !settle(1))persist();}
    pthread_mutex_unlock(&mutex);
}
static int resume_locked(void) {
    int ok=1;
    if(flight && !recovered)return 0;
    /* An ambiguous commit stays reserved; never claim it was unused. */
    if(flight){ok=settle(0);if(ok){char archive[1150];snprintf(archive,sizeof(archive),"%s.pending-%s",path,rid);ok=rename(path,archive)==0;if(ok){rid[0]=0;frames=used=flight=recovered=0;}}}
    if(ok){paused=0;error[0]=0;ok=reserve();}if(!ok){paused=1;persist();}return ok;
}
int hb_resume(void){pthread_once(&once,init);pthread_mutex_lock(&mutex);int ok=resume_locked();pthread_mutex_unlock(&mutex);return ok;}
int hb_allowed(void){pthread_once(&once,init);pthread_mutex_lock(&mutex);int ok=!paused;if(!ok){char resume[1100];snprintf(resume,sizeof(resume),"%s.resume",path);if(unlink(resume)==0)ok=resume_locked();}pthread_mutex_unlock(&mutex);return ok;}
const char *hb_error(void){static _Thread_local char copy[256];pthread_once(&once,init);pthread_mutex_lock(&mutex);memcpy(copy,error,sizeof(copy));pthread_mutex_unlock(&mutex);return copy;}
int hb_main_allowed(void){pthread_once(&once,init);pthread_mutex_lock(&mutex);int saved=paused;char saved_error[256];memcpy(saved_error,error,sizeof(error));json_object *r=request("meter/status",NULL),*v;int ok=r && json_object_object_get_ex(r,"mainAllowed",&v)&&json_object_get_boolean(v);if(r)json_object_put(r);paused=saved;memcpy(error,saved_error,sizeof(error));pthread_mutex_unlock(&mutex);return ok;}
void hb_flush(void){pthread_once(&once,init);pthread_mutex_lock(&mutex);if(!flight)settle(1);pthread_mutex_unlock(&mutex);}
