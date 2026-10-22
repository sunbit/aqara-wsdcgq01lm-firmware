# Development and validation

This repository is the source of truth for firmware changes, converters, tests
and documentation. Work on source here; do not regenerate it from the earlier
home-automation development tree.

## Host tests

Python 3 and a C99 compiler (`clang` or `cc`) are sufficient for the firmware tests:

```sh
python3 -m unittest discover -s tests -v
```

These compile the actual configuration/persistence, button, LED, event, battery,
elapsed-clock and report-policy modules with a simulated SDK. Tests cover range
checks, signed offsets and clamping, record migration, persistence failure,
unchanged-value EEPROM writes, delta/minimum/heartbeat rules, manual refresh near
a timer wake, a short press, the five-second boundary, three confirmation flashes,
a held button during reboot and timer failure. They never connect to hardware.

For converter tests, install Node.js and the pinned pnpm version in `package.json`:

```sh
pnpm install --frozen-lockfile --ignore-scripts
pnpm test
```

Tests use the actual converter dependency, mocked endpoints and the herdsman ZCL
codec. No MQTT broker or coordinator is involved.

## Cross-compilation

Follow [building](building.md). Both debug and release profiles must compile.
SDK and compiler inputs remain external and all generated products are ignored.
The clock reserves wake timer 0 with interrupts disabled to track elapsed time
across sleep; the power manager continues using wake timer 1. There are no extra
periodic clock wakeups. During a physical press, the 10 ms debounce/hold scan keeps
the CPU awake until release or the five-second action; normal idle sampling still
uses retained-RAM sleep.

## Physical validation still required

Before considering a release, verify on a pilot sensor:

1. Read all ten defaults, change each setting, and confirm its readback.
2. Remove/reinsert the battery and verify offsets and intervals survive.
3. Check corrected measurements, delta thresholds and unchanged-value heartbeats
   against MQTT timestamps, including a manual refresh just before a timer wake.
4. Confirm a short press sends fresh temperature/humidity/battery and one flash.
5. Release just below five seconds: no network reset. Hold for five seconds:
   three confirmation flashes, re-pair, same IEEE and preserved settings.
6. Hold through reboot, then release: no repeated reset.
7. Repeat normal sampling from the battery and measure current consumption and
   multi-day stability. A successful build or simulated test does not establish
   battery life or radio behavior.

Live installation changes and flashing are separate from host tests. Keep the
current binary/backup and record which source commit was flashed.
