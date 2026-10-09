"""Run an existing app smoke script with an isolated real billing service.
Usage: python3 with_billing.py <billing-image> <app-id> <command> [args...]
No production data, services or network destinations are used.
"""
import http.client
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import sys
import tempfile
import time

class UnixConnection(http.client.HTTPConnection):
    def __init__(self, path):
        super().__init__("localhost", timeout=5)
        self.path = path
    def connect(self):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(self.timeout)
        self.sock.connect(self.path)

def call(path, token, route, body=None):
    c = UnixConnection(str(path))
    try:
        c.request("POST" if body is not None else "GET", "/v1/"+route, None if body is None else json.dumps(body), {"Authorization":"Bearer "+token,"Content-Type":"application/json"})
        r=c.getresponse(); data=r.read()
        if r.status >= 300: raise RuntimeError((r.status,data.decode()))
        return json.loads(data) if data else None
    finally: c.close()

def main():
    image, app, *command = sys.argv[1:]
    root=Path(tempfile.mkdtemp(prefix="hyperion-billing-test-"));root.chmod(0o755)
    for part in ("state","run","secrets"):(root/part).mkdir()
    (root/"run").chmod(0o755)
    admin=secrets.token_hex(32); token=secrets.token_hex(32)
    (root/"secrets/service-key").write_text(admin)
    name="billing-test-"+secrets.token_hex(5)
    subprocess.run(["docker","run","-d","--name",name,"--network","none","-v",f"{root}/state:/state","-v",f"{root}/run:/run/hyperion-billing","-v",f"{root}/secrets:/secrets:ro",image],check=True,stdout=subprocess.DEVNULL)
    try:
        for _ in range(100):
            try:
                call(root/"run/billing.sock",admin,"clients",{"id":app,"app":app,"name":"Isolated "+app,"token":token});break
            except (OSError,RuntimeError):time.sleep(.1)
        else:raise RuntimeError("Billing did not start")
        wrapper=root/"billing-env.sh"
        wrapper.write_text('''docker() {
  if [[ "${1:-}" == run ]]; then
    shift
    command docker run -v "$BILLING_TEST_RUN:/run/hyperion-billing:ro" -e HYPERION_BILLING_SOCKET=/run/hyperion-billing/billing.sock -e "HYPERION_BILLING_TOKEN=$BILLING_TEST_TOKEN" "$@"
  else command docker "$@"; fi
}
''')
        env={**os.environ,"BASH_ENV":str(wrapper),"BILLING_TEST_RUN":str(root/"run"),"BILLING_TEST_TOKEN":token,"BILLING_TEST_ROOT":str(root)}
        result=subprocess.run(command,env=env)
        report=call(root/"run/billing.sock",admin,"report")
        print("BILLING REPORT",json.dumps(report),flush=True)
        (root/"report.json").write_text(json.dumps(report,indent=2))
        if result.returncode:raise SystemExit(result.returncode)
        if not report["instances"]:raise AssertionError("No completed frames were billed")
    finally:
        subprocess.run(["docker","rm","-f",name],stdout=subprocess.DEVNULL)
        print("Isolated billing test data retained:",root,flush=True)

if __name__=="__main__":main()
