#pragma once
/* One process-wide meter per app instance. begin/end must bracket one video
 * output operation. end(1) records success; end(0) returns its unused permit. */
int hb_begin(void);
void hb_end(int success);
int hb_resume(void);
int hb_allowed(void);
int hb_main_allowed(void);
const char *hb_error(void);
void hb_flush(void);
