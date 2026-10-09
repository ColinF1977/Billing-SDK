# Hyperion Billing SDK 0.1.0 POC

One protocol and native frame gate for all apps. The Python wrapper uses the same C library; the .NET client serves Hyperion's control/UI integration. Apps use one shared wallet at one micropound per completed video frame. Do not infer usage from observed MXL head movement, elapsed time or frame rate.

`hb_begin()` reserves permission before opening a video output frame; call `hb_end(1)` after successful commit or `hb_end(0)` after failure. Farcaster brackets a complete video write to its encoder pipe. Audio is not independently charged. `hb_resume()` explicitly unlocks processing after exhaustion/outage; `hb_flush()` settles known usage on orderly shutdown. Do not call begin concurrently for multiple video operations within one instance.

The C library uses libcurl and json-c. Build with `sh build.sh`. Python consumers import `Meter` and load its native library globally before loading native media bridges. Configure `HYPERION_BILLING_SOCKET`, instance-scoped `HYPERION_BILLING_TOKEN`, and durable `HYPERION_DATA` (default `/data`). The journal records batch intent before media output and checkpoints completed counts at settlement; ordinary per-frame calls do no disk or network I/O. Uncertain crash reservations stay pending. Never share `/data` between instances.

TSG uses a small ABI adapter preloaded only into its pinned MXL engine. It brackets the actual MXL open/commit/cancel functions and blocks associated audio while paused. Its UI reconciles the child journal only after the child exits. This avoids modifying the upstream engine or charging estimated frame rates.

`sync-consumers.ps1` refreshes checked-in SDK snapshots in sibling app repositories and .NET clients in Hyperion. Snapshots allow offline builds from source without fetching a private repository; VERSION and SHA256SUMS identify their contents. Edit this SDK first and sync it, rather than editing five copies.

## Tests

Build `hyperion/billing:billing-poc` from Billing and the relevant app images. From Linux/WSL:

```sh
python3 tests/with_billing.py hyperion/billing:billing-poc native-test bash tests/run-native.sh
python3 tests/with_billing.py hyperion/billing:billing-poc grey bash ../Grey/scripts/test.sh hyperion/grey:billing-poc
```

Use the same wrapper for ARC, Player, TSG and Farcaster. It starts an isolated real billing service and injects socket credentials into test containers, then verifies that completed frames reached the ledger. Test containers are removed; state and media fixtures remain for diagnosis. Source-engine fixtures are intentionally not app-metered.

There is no runtime unmetered fallback when billing is unavailable. The .NET `Billing:Enabled=false` setting is reserved for isolated legacy control-contract fixtures and must not be set in deployment.
