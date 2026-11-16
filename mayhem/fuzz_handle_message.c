/* Fuzz handle_message(), the receiving end of every MQTT publish and HTTP /pub request.
 * Input byte 0: bit0 = HTTP mode, bit1 = retained, bit2 = ignore retained; the rest is the payload. */
#include "otr_fuzz.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "json.h"
#include "udata.h"
#include "fences.h"
#include "gcache.h"
#include "recorder.h"

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc; (void)argv;
	otr_init_storage();
	return 0;
}

/* Mayhem's startup probe is `-runs=5` from an empty corpus, where libFuzzer's first inputs are one or
 * two bytes long. recorder's chomp() underflows on any whitespace-only payload (see
 * known-findings/chomp-underflow), and those tiny inputs hit it about every other probe, so a cold
 * run is reported as "failed to fuzz". Tiny inputs are therefore started from a minimal valid JSON
 * payload; every input longer than that is mutated by libFuzzer's own mutators, unfiltered, so the
 * whitespace-only payload stays reachable. */
extern size_t LLVMFuzzerMutate(uint8_t *data, size_t size, size_t max_size) __attribute__((weak));

size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t max_size, unsigned int seed)
{
	if (max_size < 3 || LLVMFuzzerMutate == NULL)
		return size;
	if (size < 3) {
		data[0] = seed & 7;
		data[1] = '{';
		data[2] = '}';
		size = 3;
	}
	return LLVMFuzzerMutate(data, size, max_size);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct udata ud;
	JsonNode *jnode = NULL;
	char topic[] = "owntracks/fuzz/dev";
	char wpdir[PATH_MAX + 8];
	char *payload;
	int flags;

	if (size < 2)
		return -1;
	flags = data[0];
	data++; size--;

	otr_reset_storage();

	memset(&ud, 0, sizeof(ud));
	ud.revgeo = 0;			/* reverse geo is an HTTP call out */
	ud.ignoreretained = (flags >> 2) & 1;
	ud.http_prefix = "http://localhost";

	snprintf(wpdir, sizeof(wpdir), "%s/wp", otr_root);
	mkdir(wpdir, 0700);
	ud.wpdb = gcache_open(wpdir, "wp", 0);

	payload = malloc(size + 1);
	if (payload == NULL)
		return 0;
	memcpy(payload, data, size);
	payload[size] = 0;

	handle_message(&ud, topic, payload, size, (flags >> 1) & 1, flags & 1, 0, &jnode);

	if (jnode != NULL)
		json_delete(jnode);
	free(payload);
	gcache_close(ud.wpdb);
	return 0;
}
