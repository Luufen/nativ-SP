/* ANDROID-PERF-01: the platform's defaults, as the consumers and the settings table see them. Built twice, with and
 * without -D__ANDROID__ (run_ci_tests.sh); no SDL, no GPU.
 *   c++ -I ../core -I ../ui platform_defaults_test.cpp [-D__ANDROID__] -o /tmp/pd && /tmp/pd */
#include "setting_defaults.h"
#include <cstdio>

int main() {
#ifdef __ANDROID__
	const char *aniso = "1"; const int aniso_n = 1, mailbox = 0; const char *who = "android";
#else
	const char *aniso = "16"; const int aniso_n = 16, mailbox = 1; const char *who = "desktop";
#endif
	int fail = 0;
	const char *d = setting_default("anisotropy");
	if (!d || strcmp(d, aniso)) { printf("anisotropy table default %s, want %s\n", d ? d : "(none)", aniso); fail = 1; }
	if (DEFAULT_ANISOTROPY != aniso_n) { printf("DEFAULT_ANISOTROPY %d, want %d\n", DEFAULT_ANISOTROPY, aniso_n); fail = 1; }
	if (DEFAULT_VULKAN_MAILBOX != mailbox) { printf("DEFAULT_VULKAN_MAILBOX %d, want %d\n", DEFAULT_VULKAN_MAILBOX, mailbox); fail = 1; }
	if (!fail) printf("platform_defaults_test (%s): 3 checks passed\n", who);
	return fail;
}
