/* vk_caps_test: gfx/vk_caps.h with stub devices -- the feature sets public Arm Mali reports show (vulkan.gpuinfo.org:
 * Mali-G52 has neither depthClamp nor shaderClipDistance, G57 and G610 lack shaderClipDistance), a device that offers
 * everything, one that lacks what has no fallback, and a failed probe. No Vulkan, SDL or GPU. */
#include <stdio.h>
#include <string.h>
#include "vk_caps.h"

static int checks, failed;
#define CHECK(x) do { checks++; if (!(x)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

static VkCapsInfo device(void) {   /* everything on, Vulkan 1.1, 4 descriptor sets */
	VkCapsInfo c; memset(&c, 0, sizeof c);
	c.known = 1; c.api_version = (1u << 22) | (1u << 12); c.has_swapchain = c.has_maintenance1 = 1; c.max_bound_descriptor_sets = 4;
	for (int i = 0; i < VKCAPS_NFEATURES; i++) c.features[i] = 1;
	return c;
}

int main(void) {
	VkCapsInfo c = device(); VkCapsPlan p;
	vk_caps_plan(&c, &p);
	CHECK(p.depth_clamp == 1); CHECK(p.fragment_stores == 1); CHECK(!p.missing[0]);

	/* Mali-G52 (report 51062, Vulkan 1.1.97, 4 sets) */
	c = device(); c.features[VKCAPS_DEPTH_CLAMP] = 0; c.features[VKCAPS_SHADER_CLIP_DISTANCE] = 0;
	vk_caps_plan(&c, &p);
	CHECK(p.depth_clamp == 0); CHECK(p.fragment_stores == 1); CHECK(!p.missing[0]);

	/* Mali-G57 / G610: clip distance only -- not needed, depth clamp stays */
	c = device(); c.features[VKCAPS_SHADER_CLIP_DISTANCE] = 0;
	vk_caps_plan(&c, &p);
	CHECK(p.depth_clamp == 1); CHECK(!p.missing[0]);

	c = device(); c.features[VKCAPS_FRAGMENT_STORES_AND_ATOMICS] = 0;
	vk_caps_plan(&c, &p);
	CHECK(p.fragment_stores == 0); CHECK(!p.missing[0]);

	/* no fallback: every one named */
	c = device(); c.features[VKCAPS_INDEPENDENT_BLEND] = 0; c.features[VKCAPS_IMAGE_CUBE_ARRAY] = 0; c.max_bound_descriptor_sets = 3;
	vk_caps_plan(&c, &p);
	CHECK(!strcmp(p.missing, "independentBlend, imageCubeArray, maxBoundDescriptorSets 3 < 4"));
	c = device(); c.api_version = 1u << 22; c.has_maintenance1 = 0; c.features[VKCAPS_SAMPLER_ANISOTROPY] = 0;
	vk_caps_plan(&c, &p);
	CHECK(strstr(p.missing, "Vulkan 1.1") != 0); CHECK(strstr(p.missing, "VK_KHR_maintenance1") != 0); CHECK(strstr(p.missing, "samplerAnisotropy") != 0);
	/* all of them together still fit the message */
	memset(&c, 0, sizeof c); c.known = 1;
	vk_caps_plan(&c, &p);
	CHECK(strlen(p.missing) < sizeof p.missing - 1); CHECK(strstr(p.missing, "maxBoundDescriptorSets 0 < 4") != 0);

	/* a failed probe: ask as before, name nothing */
	c.known = 0; vk_caps_plan(&c, &p);
	CHECK(p.depth_clamp == 1); CHECK(p.fragment_stores == 1); CHECK(!p.missing[0]);

	printf("vk_caps_test: %d checks, %d failed\n", checks, failed);
	return failed != 0;
}
