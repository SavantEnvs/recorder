/* Fuzz the .rec file reader behind `ocat` and the HTTP /api/0/locations endpoint: line parsing,
 * forwards (cat) and backwards (tac) reading, and every output renderer.
 * Input byte 0: low 3 bits pick the output type, bit3 = limit mode (tac), bit4 = field filter. */
#include "otr_fuzz.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "storage.h"

static const output_type otypes[] = { GEOJSON, CSV, JSON, GPX, LINESTRING, XML, GEOJSONPOI, GEOJSON };

static void sink(char *line, void *param)
{
	(void)param;
	if (line != NULL && strlen(line) > (size_t)1 << 30)
		abort();
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc; (void)argv;
	otr_init_storage();
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char path[PATH_MAX + 16];
	output_type otype;
	int limit;
	JsonNode *obj, *locs, *fields = NULL;
	FILE *fp;

	if (size < 1)
		return -1;
	otype = otypes[data[0] & 7];
	limit = (data[0] & 8) ? 5 : 0;
	if (data[0] & 16) {
		fields = json_mkarray();
		json_append_element(fields, json_mkstring("lat"));
		json_append_element(fields, json_mkstring("lon"));
		json_append_element(fields, json_mkstring("tst"));
		json_append_element(fields, json_mkstring("tid"));
	}
	data++; size--;

	snprintf(path, sizeof(path), "%s/input.rec", otr_root);
	fp = fopen(path, "w");
	if (fp == NULL)
		return 0;
	fwrite(data, 1, size, fp);
	fclose(fp);

	obj = json_mkobject();
	locs = json_mkarray();
	locations(path, obj, locs, 0, (time_t)1 << 40, otype, limit, fields, NULL, NULL);
	json_append_member(obj, "locations", locs);

	if (otype == JSON) {
		free(json_stringify(obj, NULL));
	} else if (otype == CSV) {
		csv_output(locs, CSV, fields, sink, NULL);
	} else if (otype == XML) {
		xml_output(locs, XML, fields, sink, NULL);
	} else if (otype == LINESTRING) {
		JsonNode *g = geo_linestring(locs);

		if (g != NULL) {
			free(json_stringify(g, NULL));
			json_delete(g);
		}
	} else if (otype == GEOJSON || otype == GEOJSONPOI) {
		JsonNode *g = geo_json(locs, otype == GEOJSONPOI);

		if (g != NULL) {
			free(json_stringify(g, NULL));
			json_delete(g);
		}
	} else if (otype == GPX) {
		(void)gpx_string(locs); /* static buffer, not owned by the caller */
	}

	json_delete(obj);
	if (fields != NULL)
		json_delete(fields);
	return 0;
}
