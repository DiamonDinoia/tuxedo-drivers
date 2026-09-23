// SPDX-License-Identifier: GPL-2.0+
// Userspace test of the src/clevo_keyboard.h flexicharger code against a fake
// EC. juno/selftest.sh extracts the code under test into flexicharger-extract.h.
//
// Fake EC (live/NVRAM = status, start, end):
//   0x46              live <- NVRAM
//   0x77              legacy read, 0x00EESSTT
//   0x76 0x06..       legacy thresholds -> live; 0x76 0x05.. status -> live, live -> NVRAM
//   0x04 0x1e         cc4 read: ACPI returns a buffer, WMI an integer 0xffffffff
//   pkgbuf 0x04 0x1f  cc4 write -> live; WMI returns -EOPNOTSUPP
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define IS_ERR_OR_NULL(p) (!(p) || (uintptr_t)(p) >= (uintptr_t)-4095)
#define EXPORT_SYMBOL(s)
#define pr_err(...) ((void)0)
#define pr_debug(...) ((void)0)
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(7, 2, 0)

#define ACPI_TYPE_INTEGER 1
#define ACPI_TYPE_BUFFER 3
union acpi_object {
	u32 type;
	struct { u32 type; u64 value; } integer;
	struct { u32 type; u32 length; u8 *pointer; } buffer;
};

struct clevo_interface_t {
	char *string_id;
	int (*method_call)(u8, u32, union acpi_object **);
	int (*method_call_pkgbuf)(u8, u8 *, u32, union acpi_object **);
};
#define CLEVO_CMD_SET_EVENTS_ENABLED 0x46

struct mutex { bool locked; };
static void mutex_lock(struct mutex *m) { assert(!m->locked); m->locked = true; }
static void mutex_unlock(struct mutex *m) { assert(m->locked); m->locked = false; }
static struct mutex clevo_flexicharger_lock;
static struct clevo_interface_t *active_clevo_interface;

static bool dmi_string_in(int field, const char *s) { return false; }
#define DMI_BOARD_NAME 0

struct attribute { const char *name; };
struct device_attribute { struct attribute attr; };
struct attribute_group { struct attribute **attrs; };
struct device { int unused; };
struct power_supply { struct device dev; };
struct acpi_battery_hook { int unused; };
#define DEVICE_ATTR_RW(n) struct device_attribute dev_attr_##n = { { #n } }
#define DEVICE_ATTR_RO(n) DEVICE_ATTR_RW(n)
#define ATTRIBUTE_GROUPS(n)                                                \
	static const struct attribute_group n##_group = { n##_attrs };    \
	static const struct attribute_group *n##_groups[] = { &n##_group, NULL }
static int device_add_groups(struct device *d, const struct attribute_group **g) { return 0; }
static void device_remove_groups(struct device *d, const struct attribute_group **g) { }
#define to_power_supply(d) ((struct power_supply *)(d))
static int supply_changed;
static void power_supply_changed(struct power_supply *p) { ++supply_changed; }
static bool sysfs_streq(const char *a, const char *b) { return !strcmp(a, b) || (!strncmp(a, b, strlen(b)) && !strcmp(a + strlen(b), "\n")); }
static int kstrtou8(const char *s, int base, u8 *v)
{
	char *e;
	unsigned long x = strtoul(s, &e, base);

	if (e == s || (*e && strcmp(e, "\n")) || x > 255)
		return -EINVAL;
	*v = x;
	return 0;
}

// Every object handed out is tracked, so a leak or a free of garbage fails.
static union acpi_object *live_objs[16];
static int n_objs;
static union acpi_object *new_obj(u32 type)
{
	union acpi_object *o = calloc(1, sizeof(*o));

	o->type = type;
	assert(n_objs < 16);
	live_objs[n_objs++] = o;
	return o;
}
static void ACPI_FREE(union acpi_object *o)
{
	for (int i = 0; i < n_objs; ++i) {
		if (live_objs[i] == o) {
			live_objs[i] = live_objs[--n_objs];
			if (o->type == ACPI_TYPE_BUFFER)
				free(o->buffer.pointer);
			free(o);
			return;
		}
	}
	assert(!"ACPI_FREE of an object never allocated");
}

static struct { u8 live[3], nvram[3]; } ec;
static int unlocked_ec_calls;

static int ec_call(bool wmi, u8 cmd, u32 arg, union acpi_object **out)
{
	union acpi_object *o;
	u32 v = 0;

	if (!clevo_flexicharger_lock.locked)
		++unlocked_ec_calls;
	if (cmd == 0x46) {
		memcpy(ec.live, ec.nvram, 3);
		v = 0x100;
	} else if (cmd == 0x77) {
		v = ec.live[2] << 16 | ec.live[1] << 8 | ec.live[0];
	} else if (cmd == 0x76) {
		if (arg >> 24 == 0x06) {
			ec.live[1] = arg & 0xff;
			ec.live[2] = (arg >> 8) & 0xff;
		} else if (arg >> 24 == 0x05) {
			ec.live[0] = arg & 1;
			memcpy(ec.nvram, ec.live, 3);
		}
		v = 0x76;
	} else if (cmd == 0x04 && !wmi) {
		o = new_obj(ACPI_TYPE_BUFFER);
		o->buffer.length = 256;
		o->buffer.pointer = calloc(256, 1);
		memcpy(o->buffer.pointer, ec.live, 3);
		*out = o;
		return 0;
	} else if (cmd == 0x04) {
		v = 0xffffffff;
	}
	o = new_obj(ACPI_TYPE_INTEGER);
	o->integer.value = v;
	*out = o;
	return 0;
}

static int acpi_call(u8 cmd, u32 arg, union acpi_object **out) { return ec_call(false, cmd, arg, out); }
static int wmi_call(u8 cmd, u32 arg, union acpi_object **out) { return ec_call(true, cmd, arg, out); }
static int acpi_pkgbuf(u8 cmd, u8 *arg, u32 len, union acpi_object **out)
{
	union acpi_object *o = new_obj(ACPI_TYPE_BUFFER);

	assert(cmd == 0x04 && len >= 4 && arg[0] == 0x1f);
	if (!clevo_flexicharger_lock.locked)
		++unlocked_ec_calls;
	memcpy(ec.live, arg + 1, 3);
	o->buffer.length = 4;
	o->buffer.pointer = calloc(4, 1);
	*out = o;
	return 0;
}
static int wmi_pkgbuf(u8 cmd, u8 *arg, u32 len, union acpi_object **out)
{
	return -EOPNOTSUPP;
}

static struct clevo_interface_t acpi_if = { "clevo_acpi", acpi_call, acpi_pkgbuf };
static struct clevo_interface_t wmi_if = { "clevo_wmi", wmi_call, wmi_pkgbuf };

#include "flexicharger-extract.h"

static int failures;
#define CHECK(c)                                                          \
	do {                                                              \
		if (!(c)) {                                               \
			printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
			++failures;                                       \
		}                                                         \
	} while (0)

static void reset(u8 s, u8 a, u8 b, u8 ns, u8 na, u8 nb)
{
	ec = (typeof(ec)){ { s, a, b }, { ns, na, nb } };
	active_clevo_interface = NULL;
	memset(&clevo_flexicharger_preserve, 0, sizeof(clevo_flexicharger_preserve));
	supply_changed = 0;
	unlocked_ec_calls = 0;
}

static bool live_is(u8 s, u8 a, u8 b) { return ec.live[0] == s && ec.live[1] == a && ec.live[2] == b; }

// clevo_keyboard_add_interface(): active interface assigned, then 0x46 through it.
static void add_interface(struct clevo_interface_t *i)
{
	active_clevo_interface = i;
	clevo_flexicharger_set_events_enabled();
}

int main(void)
{
	struct power_supply bat;
	char buf[64];
	int before = failures;

	setvbuf(stdout, NULL, _IONBF, 0); // keep output ahead of a sanitizer abort

#define CASE(name) printf("case %s\n", name); before = failures
#define END() printf("  %s\n", failures == before ? "ok" : "FAILED")

	CASE("acpi-first registration keeps live 95/100 across 0x46 (NVRAM 70/80)");
	reset(1, 95, 100, 1, 70, 80);
	add_interface(&acpi_if);
	add_interface(&acpi_if); // probe-time 0x46
	CHECK(live_is(1, 95, 100));
	CHECK(!clevo_flexicharger_preserve.held);
	END();

	CASE("wmi-first registration (RELOAD.sh order) keeps live 95/100 until acpi restores it");
	reset(1, 95, 100, 1, 70, 80);
	add_interface(&wmi_if);
	CHECK(clevo_flexicharger_preserve.held);
	add_interface(&wmi_if); // probe-time 0x46 still on wmi
	add_interface(&acpi_if);
	CHECK(live_is(1, 95, 100));
	CHECK(!clevo_flexicharger_preserve.held);
	END();

	CASE("0x46 with no active interface returns without dereferencing it");
	reset(1, 95, 100, 1, 70, 80);
	clevo_flexicharger_set_events_enabled();
	CHECK(live_is(1, 95, 100));
	END();

	CASE("cc4 store passes raw percents and notifies the supply");
	reset(1, 70, 80, 1, 70, 80);
	active_clevo_interface = &acpi_if;
	CHECK(charge_control_end_threshold_store(&bat.dev, NULL, "85\n", 3) == 3);
	CHECK(charge_control_start_threshold_store(&bat.dev, NULL, "75", 2) == 2);
	CHECK(charge_type_store(&bat.dev, NULL, "Standard\n", 9) == 9);
	CHECK(live_is(0, 75, 85));
	CHECK(supply_changed == 3);
	CHECK(charge_control_end_threshold_store(&bat.dev, NULL, "101", 3) == -EINVAL);
	CHECK(charge_control_start_threshold_store(&bat.dev, NULL, "0", 1) == -EINVAL);
	CHECK(charge_type_store(&bat.dev, NULL, "Fast", 4) == -EINVAL);
	CHECK(supply_changed == 3);
	memset(buf, 0, sizeof(buf));
	CHECK(charge_control_end_threshold_show(&bat.dev, NULL, buf) > 0 && !strcmp(buf, "85\n"));
	END();

	CASE("legacy path over wmi: reads work, stores snap to the legacy table");
	reset(1, 95, 100, 1, 95, 100);
	active_clevo_interface = &wmi_if;
	memset(buf, 0, sizeof(buf));
	CHECK(charge_control_start_threshold_show(&bat.dev, NULL, buf) > 0 && !strcmp(buf, "95\n"));
	CHECK(charge_control_end_threshold_store(&bat.dev, NULL, "85", 2) == 2);
	CHECK(live_is(1, 95, 80)); // 85 ties 80/90, the first wins
	memset(buf, 0, sizeof(buf));
	CHECK(charge_type_show(&bat.dev, NULL, buf) > 0 && !strcmp(buf, "Custom\n"));
	END();

	CASE("battery hook probes run under clevo_flexicharger_lock and register the attrs");
	reset(1, 95, 100, 1, 95, 100);
	active_clevo_interface = &acpi_if;
	CHECK(clevo_battery_add(&bat, NULL) == 0);
	CHECK(charge_control_registered);
	CHECK(unlocked_ec_calls == 0);
	CHECK(clevo_battery_remove(&bat, NULL) == 0);
	memset(buf, 0, sizeof(buf));
	CHECK(charge_control_start_available_thresholds_show(&bat.dev, NULL, buf) > 0 &&
	      !strcmp(buf, "40 50 60 70 80 95\n"));
	memset(buf, 0, sizeof(buf));
	CHECK(charge_control_end_available_thresholds_show(&bat.dev, NULL, buf) > 0 &&
	      !strcmp(buf, "60 70 80 90 100\n"));
	END();

	CASE("every EC object handed out was freed exactly once");
	CHECK(n_objs == 0);
	END();

	printf("flexicharger-test: %s (%d failed checks)\n", failures ? "FAIL" : "PASS", failures);
	return failures != 0;
}
