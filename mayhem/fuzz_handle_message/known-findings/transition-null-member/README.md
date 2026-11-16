# `_type":"transition"` without `event`/`desc` dereferences NULL

In `handle_message()` (recorder.c:1214-1218) `json_find_member(json, "event")` and
`json_find_member(json, "desc")` return NULL when the member is missing, and the next line reads
`e->tag` / `d->tag`: UBSan reports "member access within null pointer of type 'JsonNode'".

Reach: any MQTT publish or HTTP `/pub` body that is a location-shaped JSON object (`lat`, `lon`, `tst`) with `"_type":"transition"` and no `event` or no `desc` member.
`event` or no `desc` member.

Reproduce: `/mayhem/fuzz_handle_message-standalone mayhem/fuzz_handle_message/known-findings/transition-null-member/input.bin`
(byte 0 is the harness flag byte, the rest is the seed `transition.bin` payload with the `event` member removed).

Fix: test `e == NULL || d == NULL` before reading `->tag`.

Kept out of `testsuite/` on purpose: seeds are replayed by every run, and a crashing seed fails
Mayhem's startup probe.
