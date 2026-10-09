#!/bin/sh
set -eu
mkdir -p /opt/hyperion-billing
cc -std=c11 -O2 -Wall -Wextra -fPIC -shared native/hyperion_billing.c -o /opt/hyperion-billing/libhyperion_billing.so $(pkg-config --cflags --libs libcurl json-c) -lpthread
cp python/hyperion_billing.py /opt/hyperion-billing/
cp python/billing.js /opt/hyperion-billing/
cc -std=c11 -O2 -Wall -Wextra -fPIC -shared native/tsg_hook.c -L/opt/hyperion-billing -lhyperion_billing -Wl,-rpath,/opt/hyperion-billing -ldl -o /opt/hyperion-billing/libtsg_billing.so
