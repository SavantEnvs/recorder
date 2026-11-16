/* Fuzz the bundled ccan JSON parser/serializer that decodes every MQTT/HTTP payload and .rec line. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

static void walk(JsonNode *n, int depth)
{
	JsonNode *c;

	if (n->tag == JSON_STRING) {
		char *e = json_encode_string(n->string_);
		free(e);
	}
	if (n->tag == JSON_OBJECT || n->tag == JSON_ARRAY) {
		int i = 0;

		json_foreach(c, n) {
			if (n->tag == JSON_OBJECT) {
				if (json_find_member(n, c->key) == NULL)
					abort();
			} else if (json_find_element(n, i) != c) {
				abort();
			}
			i++;
			if (depth < 64)
				walk(c, depth + 1);
		}
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *s = malloc(size + 1);
	JsonNode *n;
	char err[256];

	if (s == NULL)
		return 0;
	memcpy(s, data, size);
	s[size] = 0;

	(void)json_validate(s);
	n = json_decode(s);
	if (n != NULL) {
		char *enc, *pretty;

		if (!json_check(n, err))
			abort();	/* a tree the decoder built must be well formed */
		enc = json_encode(n);
		pretty = json_stringify(n, "  ");
		free(enc);
		free(pretty);
		walk(n, 0);
		json_delete(n);
	}
	free(s);
	return 0;
}
