#include "led_probe.h"

static int stable(const led_probe_io *io, void *ctx, uint32_t bit)
{
    unsigned n;
    int level = !!(io->input(ctx) & bit);
    for (n = 0; n < 10; ++n) {
        if (!io->wait_ms(ctx, 2)) return -2;
        if (!!(io->input(ctx) & bit) != level) return -1;
    }
    return level;
}

static void restore(const led_probe_io *io, void *ctx, uint32_t bit,
                    uint32_t old_output, uint32_t old_pullup)
{
    io->set_direction(ctx, bit, 0);
    io->set_output(ctx, old_output & bit, ~old_output & bit);
    io->set_pullup(ctx, ~old_pullup & bit, old_pullup & bit);
}

int led_probe_run(const led_probe_io *io, void *ctx)
{
    unsigned dio, pulse, polarity;
    if (!io || !io->input || !io->direction || !io->output ||
        !io->pullup_disabled || !io->set_direction || !io->set_output ||
        !io->set_pullup || !io->wait_ms || !io->log) return 0;
    for (dio = 0; dio < 20; ++dio) {
        uint32_t bit = 1UL << dio;
        uint32_t old_output, old_pullup;
        int on, off, current, expected;
        if (!(LED_PROBE_MASK & bit)) continue;
        if (io->direction(ctx) & bit) {
            io->log(ctx, "skip-existing-output", dio, -1);
            continue;
        }
        old_output = io->output(ctx);
        old_pullup = io->pullup_disabled(ctx);
        io->set_direction(ctx, bit, 0);
        io->set_pullup(ctx, bit, 0);
        if (!io->wait_ms(ctx, 50)) goto aborted;
        on = stable(io, ctx, bit);
        if (on == -2) goto aborted;
        io->set_pullup(ctx, 0, bit);
        if (!io->wait_ms(ctx, 700)) goto aborted;
        off = stable(io, ctx, bit);
        if (off == -2) goto aborted;
        io->log(ctx, "weak-pullup-on", dio, on);
        io->log(ctx, "weak-pullup-off", dio, off);
        if (on != 1 || off < 0) {
            io->log(ctx, "skip-low-or-unstable-profile", dio, -1);
            goto next;
        }
        /* A LOW trial requires stable LOW without bias and stable HIGH with
         * weak pull-up. Never sink a pin that remains HIGH without bias.
         * A HIGH trial requires freshly verified HIGH with weak pull-up. */
        for (polarity = 0; polarity < 2; ++polarity) {
            expected = (int)polarity;
            if (!expected && off != 0) {
                io->log(ctx, "skip-low-high-without-pullup", dio, -1);
                continue;
            }
            for (pulse = 0; pulse < 3; ++pulse) {
                io->set_direction(ctx, bit, 0);
                io->set_pullup(ctx, expected ? bit : 0, expected ? 0 : bit);
                if (!io->wait_ms(ctx, expected ? 50 : 700)) goto aborted;
                current = stable(io, ctx, bit);
                if (current == -2) goto aborted;
                if (current != expected) {
                    io->log(ctx, "skip-level-changed-before-drive", dio, current);
                    break;
                }
                io->log(ctx, pulse == 0 ? "pulse1" : pulse == 1 ? "pulse2" : "pulse3", dio, expected);
                /* Preload while input to avoid a transition to the wrong level. */
                io->set_output(ctx, expected ? bit : 0, expected ? 0 : bit);
                io->set_direction(ctx, 0, bit);
                if (!!(io->input(ctx) & bit) != expected) {
                    io->set_direction(ctx, bit, 0);
                    io->log(ctx, "abort-pin-readback-mismatch", dio, expected);
                    goto next;
                }
                if (!io->wait_ms(ctx, 200)) goto aborted;
                io->set_direction(ctx, bit, 0);
                /* Passive opposite bias between visible pulses. */
                io->set_pullup(ctx, expected ? 0 : bit, expected ? bit : 0);
                if (!io->wait_ms(ctx, 600)) goto aborted;
            }
        }
next:
        restore(io, ctx, bit, old_output, old_pullup);
        continue;
aborted:
        restore(io, ctx, bit, old_output, old_pullup);
        io->log(ctx, "aborted-clock-timeout-pin-restored", dio, -1);
        return 0;
    }
    return 1;
}

int led_probe_low_remaining_run(const led_probe_io *io, void *ctx)
{
    unsigned dio, pulse, ms;
    if (!io || !io->input || !io->direction || !io->output ||
        !io->pullup_disabled || !io->set_direction || !io->set_output ||
        !io->set_pullup || !io->wait_ms || !io->log) return 0;
    for (dio = 0; dio < 20; ++dio) {
        uint32_t bit = 1UL << dio;
        uint32_t old_output, old_pullup;
        int on, off;
        if (!(LED_PROBE_LOW_REMAINING_MASK & bit)) continue;
        if (io->direction(ctx) & bit) {
            io->log(ctx, "skip-existing-output", dio, -1);
            continue;
        }
        old_output = io->output(ctx);
        old_pullup = io->pullup_disabled(ctx);
        io->set_direction(ctx, bit, 0);
        io->set_pullup(ctx, bit, 0);
        if (!io->wait_ms(ctx, 50)) goto aborted;
        on = stable(io, ctx, bit);
        if (on == -2) goto aborted;
        io->set_pullup(ctx, 0, bit);
        if (!io->wait_ms(ctx, 700)) goto aborted;
        off = stable(io, ctx, bit);
        if (off == -2) goto aborted;
        io->log(ctx, "weak-pullup-on", dio, on);
        io->log(ctx, "weak-pullup-off", dio, off);
        if (on != 1 || off < 0) {
            io->log(ctx, "skip-low-or-unstable-profile", dio, -1);
            goto next_low;
        }
        for (pulse = 0; pulse < 3; ++pulse) {
            int current;
            io->set_direction(ctx, bit, 0);
            io->set_pullup(ctx, bit, 0);
            if (!io->wait_ms(ctx, 600)) goto aborted;
            current = stable(io, ctx, bit);
            if (current == -2) goto aborted;
            if (current != 1) {
                io->log(ctx, "skip-high-baseline-lost", dio, current);
                goto next_low;
            }
            io->log(ctx, pulse == 0 ? "LOW-pulse1" : pulse == 1 ? "LOW-pulse2" : "LOW-pulse3", dio, 0);
            io->set_output(ctx, 0, bit);
            io->set_direction(ctx, 0, bit);
            /* Release immediately on HIGH; a LOW read does not bound current. */
            if (io->input(ctx) & bit) {
                io->set_direction(ctx, bit, 0);
                io->log(ctx, "abort-not-low-pin-released", dio, 1);
                goto next_low;
            }
            for (ms = 0; ms < 150; ++ms) {
                if (!io->wait_ms(ctx, 1)) goto aborted;
                if (io->input(ctx) & bit) {
                    io->set_direction(ctx, bit, 0);
                    io->log(ctx, "abort-low-lost-pin-released", dio, 1);
                    goto next_low;
                }
            }
            io->set_direction(ctx, bit, 0);
        }
next_low:
        restore(io, ctx, bit, old_output, old_pullup);
        if (!io->wait_ms(ctx, 1000)) return 0;
        continue;
aborted:
        restore(io, ctx, bit, old_output, old_pullup);
        io->log(ctx, "aborted-clock-timeout-pin-restored", dio, -1);
        return 0;
    }
    return 1;
}
