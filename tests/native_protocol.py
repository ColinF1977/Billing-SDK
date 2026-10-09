"""Executed inside an app image against an isolated local billing service."""
import sqlite3
import os
import subprocess
import sys
import tempfile
from hyperion_billing import Meter

db=sqlite3.connect('/billing-state/billing.db')
db.execute('UPDATE account SET balance=5');db.commit()
meter=Meter()
for _ in range(5):
    assert meter.begin()
    meter.end(True)
assert not meter.begin(), 'sixth frame must be denied'
assert not meter.allowed
assert db.execute('SELECT SUM(frames) FROM usage').fetchone()[0]==5
assert db.execute('SELECT balance FROM account').fetchone()[0]==0
db.execute('UPDATE account SET balance=3');db.commit()
assert not meter.begin(), 'top-up must not automatically restart paused media'
meter.resume()
assert meter.begin();meter.end(False)
assert meter.begin();meter.end(True)
meter.flush()
assert db.execute('SELECT balance FROM account').fetchone()[0]==2
assert db.execute('SELECT SUM(frames) FROM usage').fetchone()[0]==6
assert db.execute('SELECT COUNT(*) FROM reservations WHERE closed=0').fetchone()[0]==0
print('PASS native gate: exhaustion, explicit resume, failed-frame refund, exact settlement')
db.execute('UPDATE account SET balance=100');db.commit()
env={**os.environ,'HYPERION_DATA':tempfile.mkdtemp(prefix='billing-crash-')}
subprocess.run([sys.executable,'-c','from hyperion_billing import Meter; import os; m=Meter(); assert m.begin(); m.end(True); os._exit(0)'],env=env,check=True)
assert db.execute('SELECT SUM(frames-used) FROM reservations WHERE closed=0').fetchone()[0]==24
subprocess.run([sys.executable,'-c','from hyperion_billing import Meter; m=Meter(); assert not m.allowed; m.resume(); assert m.begin(); m.end(True); m.flush()'],env=env,check=True)
assert db.execute('SELECT SUM(frames-used) FROM reservations WHERE closed=0').fetchone()[0]==24
assert db.execute('SELECT SUM(frames) FROM usage').fetchone()[0]==7
print('PASS crash recovery: ambiguous batch remains reserved, explicit resume starts a new batch')
