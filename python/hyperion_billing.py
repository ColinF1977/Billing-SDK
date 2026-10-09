"""Shared frame gate. All applications use the same native SDK and wallet."""
import ctypes
import os
import json
import pathlib
import urllib.parse

class Meter:
    def __init__(self):
        self._lib = None

    @property
    def lib(self):
        if self._lib is None:
            self._lib = ctypes.CDLL(os.getenv("HYPERION_BILLING_LIBRARY", "/opt/hyperion-billing/libhyperion_billing.so"), mode=ctypes.RTLD_GLOBAL)
            self._lib.hb_error.restype = ctypes.c_char_p
        return self._lib

    def begin(self):
        return bool(self.lib.hb_begin())

    def end(self, success):
        self.lib.hb_end(int(bool(success)))

    def resume(self):
        if not self.lib.hb_resume():
            raise ValueError(self.error)

    def require_main(self):
        if not self.lib.hb_main_allowed():
            raise ValueError("Hyperion subscription unavailable or expired. Open Billing to renew.")

    @property
    def allowed(self):
        return bool(self.lib.hb_allowed())

    @property
    def error(self):
        return self.lib.hb_error().decode("utf-8", "replace")

    def status(self):
        return {"allowed": self.allowed, "reason": self.error}

    def flush(self):
        self.lib.hb_flush()

def handle_billing(handler, method, meter, resume=None, status=None):
    path = urllib.parse.urlsplit(handler.path).path
    if method == "GET" and path == "/billing.js":
        handler.respond(200, pathlib.Path(__file__).with_name("billing.js").read_bytes(), "text/javascript")
        return True
    if method == "GET" and path == "/api/billing/status":
        handler.respond(200, status() if status else meter.status())
        return True
    if method == "POST" and path == "/api/billing/resume":
        origin = urllib.parse.urlsplit(handler.headers.get("Origin", ""))
        if origin.scheme != "http" or origin.netloc != handler.headers.get("Host", "") or handler.headers.get("Content-Type", "").split(";")[0] != "application/json":
            handler.respond(403, {"error": "Same-origin JSON required."})
            return True
        try:
            length = int(handler.headers.get("Content-Length", "0"))
            if length < 0 or length > 4096:
                raise ValueError("Invalid request size.")
            handler.connection.settimeout(5)
            if length: json.loads(handler.rfile.read(length))
            (resume or meter.resume)()
            handler.respond(200, status() if status else meter.status())
        except (ValueError, OSError) as error:
            handler.respond(409, {"error": str(error)})
        return True
    return False
