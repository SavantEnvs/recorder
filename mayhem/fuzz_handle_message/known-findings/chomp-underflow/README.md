# chomp() reads before the payload on a whitespace-only message

`handle_message()` (recorder.c:772) calls `chomp(payload)` (util.c:645). `chomp` starts at the last
byte and walks backwards while `isspace(*p)`, with no lower bound, so a payload made only of
whitespace (for example `"\n\n"`) reads and writes `s[-1]`: ASan reports a heap-buffer-overflow
(READ of size 1, util.c:654) in `chomp`.

Reach: any MQTT publish or HTTP `/pub` body made only of whitespace.

Reproduce: `/mayhem/fuzz_handle_message-standalone mayhem/fuzz_handle_message/known-findings/chomp-underflow/input.bin`
(byte 0 is the harness flag byte, the rest is the payload `"\n\n"`).

Fix: stop at `s` (`while (p >= s && isspace(*p))`).

Kept out of `testsuite/` on purpose: seeds are replayed by every run, and a crashing seed fails
Mayhem's startup probe.
