#!/bin/bash
set -euo pipefail
docker run --rm --user 0 --entrypoint python3 \
  -v "$BILLING_TEST_ROOT/state:/billing-state" \
  -v "$(dirname "$0")/native_protocol.py:/test.py:ro" \
  "${SDK_TEST_IMAGE:-hyperion/grey:billing-poc}" /test.py
