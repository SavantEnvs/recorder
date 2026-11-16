/* Fuzz the small decoders the recorder runs on untrusted strings: base64, geohash encoding,
 * topic/list splitting and the HTTP from/to time parser. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "base64.h"
#include "geohash.h"
#include "json.h"
#include "storage.h"
#include "util.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *s = malloc(size + 1);
	char *enc, *geo, *parts[64], *nl;
	void *dec;
	size_t dlen = 0;
	time_t lo, hi;
	int n;

	if (s == NULL)
		return 0;
	memcpy(s, data, size);
	s[size] = 0;

	/* base64: arbitrary text must decode or be refused; encode->decode is the identity */
	dec = base64_decode(s, &dlen);
	free(dec);
	enc = base64_encode(data, size);
	if (enc != NULL) {
		dec = base64_decode(enc, &dlen);
		if (dec == NULL || dlen != size || (size && memcmp(dec, data, size) != 0))
			abort();
		free(dec);
		free(enc);
	}

	/* geohash: lat/lon come from the payload's JSON numbers (NaN is rejected before the call) */
	if (size >= 17) {
		double lat, lon;

		memcpy(&lat, data, 8);
		memcpy(&lon, data + 8, 8);
		if (!isnan(lat) && !isnan(lon)) {
			geo = geohash_encode(lat, lon, data[16] % 16);
			free(geo);
		}
	}

	/* topic and list splitting */
	n = splitter(s, "/", parts);
	if (n >= 0 && n < 64) {
		parts[n] = NULL;
		splitterfree(parts);
	}
	{
		JsonNode *arr = json_splitter(s, ",");

		if (arr != NULL)
			json_delete(arr);
	}

	/* HTTP `from`/`to` query parameters: "from\nto" */
	nl = strchr(s, '\n');
	if (nl != NULL) {
		*nl = 0;
		(void)make_times(s, &lo, nl + 1, &hi, 0);
	} else {
		(void)make_times(s, &lo, NULL, &hi, 0);
	}

	free(s);
	return 0;
}
