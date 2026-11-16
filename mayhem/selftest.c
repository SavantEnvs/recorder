/* Known-answer checks over recorder's own code, built with the project's normal flags and run by
 * mayhem/test.sh. Prints "PASS <name>" / "FAIL <name>" and a "SUMMARY: N tests run; F failed" line. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "base64.h"
#include "udata.h"
#include "fences.h"
#include "gcache.h"
#include "geohash.h"
#include "json.h"
#include "recorder.h"
#include "storage.h"
#include "udata.h"
#include "util.h"
#include "zonedetect.h"

extern char STORAGEDIR[];

static int ran, failed;

static void check(const char *name, int ok)
{
	ran++;
	if (!ok)
		failed++;
	printf("%s %s\n", ok ? "PASS" : "FAIL", name);
}

static int streq(const char *a, const char *b)
{
	return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static void test_geohash(void)
{
	char *h = geohash_encode(57.64911, 10.40744, 11);

	check("geohash 57.64911,10.40744 p11 = u4pruydqqvj", streq(h, "u4pruydqqvj"));
	free(h);
	h = geohash_encode(57.64911, 10.40744, 5);
	check("geohash 57.64911,10.40744 p5 = u4pru", streq(h, "u4pru"));
	free(h);
	h = geohash_encode(-25.382708, -49.265506, 7);
	check("geohash -25.382708,-49.265506 p7 = 6gkzwgj", streq(h, "6gkzwgj"));
	free(h);
}

static void test_base64(void)
{
	char *e = base64_encode("hello", 5);
	size_t n = 0;
	char *d;

	check("base64 encode hello = aGVsbG8=", streq(e, "aGVsbG8="));
	free(e);
	d = base64_decode("aGVsbG8=", &n);
	check("base64 decode aGVsbG8= = hello", d != NULL && n == 5 && memcmp(d, "hello", 5) == 0);
	free(d);
}

static void test_json(void)
{
	JsonNode *n = json_decode("{\"a\":[1,2,{\"b\":\"x y\"}],\"c\":true}");
	char *s = n ? json_stringify(n, NULL) : NULL;
	JsonNode *a = n ? json_find_member(n, "a") : NULL;

	check("json round trip", streq(s, "{\"a\":[1,2,{\"b\":\"x y\"}],\"c\":true}"));
	check("json element 1 of a = 2", a != NULL && json_find_element(a, 1) != NULL &&
	      json_find_element(a, 1)->number_ == 2);
	check("json rejects trailing garbage", json_decode("{\"a\":1} x") == NULL);
	free(s);
	json_delete(n);
}

static void test_splitter(void)
{
	char *parts[42];
	int n = splitter("owntracks/alice/phone/event", "/", parts);

	check("splitter yields 4 parts", n == 4);
	check("splitter part 1 = alice", n == 4 && streq(parts[1], "alice"));
	splitterfree(parts);
}

static void test_timezone(void)
{
	ZoneDetect *zd = ZDOpenDatabase("contrib/tzdatadb/timezone16.bin");
	char *tz;

	check("timezone db opens", zd != NULL);
	if (zd == NULL)
		return;
	tz = ZDHelperSimpleLookupString(zd, 52.5200f, 13.4050f);
	check("timezone of Berlin contains Europe/Berlin", tz != NULL && strstr(tz, "Europe/Berlin") != NULL);
	if (tz != NULL)
		ZDHelperSimpleLookupStringFree(tz);
	tz = ZDHelperSimpleLookupString(zd, 40.7128f, -74.0060f);
	check("timezone of New York contains America/New_York", tz != NULL && strstr(tz, "America/New_York") != NULL);
	if (tz != NULL)
		ZDHelperSimpleLookupStringFree(tz);
	ZDCloseDatabase(zd);
}

/* A location publish is recorded to <store>/rec/<user>/<device>/<YYYY-MM>.rec and reads back. */
static void test_record_and_read(void)
{
	char root[] = "/tmp/otr-selftest-XXXXXX";
	char path[512], wpdir[512], line[1024];
	char topic[] = "owntracks/alice/phone";
	char payload[] = "{\"_type\":\"location\",\"lat\":52.5,\"lon\":13.25,\"tst\":1440395361,\"acc\":10,\"tid\":\"AL\"}";
	struct udata ud;
	JsonNode *jnode = NULL, *obj, *locs, *l0;
	FILE *fp;
	int found = 0;

	if (mkdtemp(root) == NULL) {
		check("scratch dir", 0);
		return;
	}
	snprintf(STORAGEDIR, BUFSIZ, "%s", root);
	snprintf(wpdir, sizeof(wpdir), "%s/wp", root);
	mkdir(wpdir, 0700);
	memset(&ud, 0, sizeof(ud));
	ud.http_prefix = "http://localhost";
	ud.wpdb = gcache_open(wpdir, "wp", 0);

	handle_message(&ud, topic, payload, strlen(payload), 0, 0, 0, &jnode);
	if (jnode != NULL)
		json_delete(jnode);
	gcache_close(ud.wpdb);

	snprintf(path, sizeof(path), "%s/rec/alice/phone/2015-08.rec", root);
	fp = fopen(path, "r");
	check("handle_message wrote rec file 2015-08.rec", fp != NULL);
	if (fp != NULL) {
		while (fgets(line, sizeof(line), fp) != NULL)
			if (strstr(line, "\"lat\":52.5") != NULL && strstr(line, "\"tid\":\"AL\"") != NULL &&
			    strstr(line, "2015-08-24T05:49:21Z") != NULL)
				found++;
		fclose(fp);
	}
	check("rec line has timestamp, lat and tid", found == 1);

	obj = json_mkobject();
	locs = json_mkarray();
	locations(path, obj, locs, 0, (time_t)1 << 40, JSON, 0, NULL, NULL, NULL);
	check("locations() reads one record back", json_find_element(locs, 1) == NULL && json_find_element(locs, 0) != NULL);
	l0 = json_find_element(locs, 0);
	check("locations() lat = 52.5", l0 != NULL && json_find_member(l0, "lat") != NULL &&
	      json_find_member(l0, "lat")->number_ == 52.5);
	check("locations() lon = 13.25", l0 != NULL && json_find_member(l0, "lon") != NULL &&
	      json_find_member(l0, "lon")->number_ == 13.25);
	json_delete(locs);
	json_delete(obj);

	snprintf(line, sizeof(line), "rm -rf '%s'", root);
	if (system(line) != 0)
		fprintf(stderr, "cleanup of %s failed\n", root);
}

int main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	test_geohash();
	test_base64();
	test_json();
	test_splitter();
	test_timezone();
	test_record_and_read();
	printf("SUMMARY: %d tests run; %d failed\n", ran, failed);
	return failed != 0;
}
