/*
 * vk_caps.h: what an Android Vulkan device offers against what SDL_GPU's Vulkan backend (SDL 3.4.16) and Nativ ask of
 * it, decided before the device is made (gfx_gpu.c create_gpu_device). SDL turns a device down without saying which
 * feature it lacked ("Failed to determine a suitable physical device"), so a feature Nativ can do without is switched off
 * here and one it can't is named. Public Arm Mali reports (vulkan.gpuinfo.org): no shaderClipDistance on Mali-G52,
 * G57 and G610, no depthClamp on Mali-G52 -- both on SDL's default list.
 *
 * Pure: no Vulkan or SDL headers (tests/vk_caps_test.c feeds it a stub device). The probe that fills VkCapsInfo is
 * gfx_gpu.c's.
 */
#ifndef NATIV_VK_CAPS_H
#define NATIV_VK_CAPS_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* VkPhysicalDeviceFeatures: 55 VkBool32 in the specification's order */
enum {
	VKCAPS_ROBUST_BUFFER_ACCESS = 0, VKCAPS_IMAGE_CUBE_ARRAY = 2, VKCAPS_INDEPENDENT_BLEND = 3, VKCAPS_SAMPLE_RATE_SHADING = 6,
	VKCAPS_DRAW_INDIRECT_FIRST_INSTANCE = 10, VKCAPS_DEPTH_CLAMP = 11, VKCAPS_SAMPLER_ANISOTROPY = 19,
	VKCAPS_FRAGMENT_STORES_AND_ATOMICS = 26, VKCAPS_SHADER_CLIP_DISTANCE = 37, VKCAPS_NFEATURES = 55
};
#define VKCAPS_MIN_BOUND_SETS 4u   /* SDL_GPU's graphics pipeline layout: 4 descriptor sets, no push constants */

typedef struct {
	int known;                 /* 0: the probe failed, nothing below is meaningful */
	uint32_t api_version;      /* VkPhysicalDeviceProperties.apiVersion */
	uint32_t vendor_id;        /* VkPhysicalDeviceProperties.vendorID (tex_caps.h) */
	uint32_t features[VKCAPS_NFEATURES];
	int has_swapchain, has_maintenance1;
	uint32_t max_bound_descriptor_sets;
} VkCapsInfo;

typedef struct {
	int depth_clamp;           /* 0: SDL's depth-clamping requirement off, every pipeline clips in depth instead */
	int fragment_stores;       /* 0: fragmentStoresAndAtomics not asked for (the visibility tests stay off) */
	char missing[400];         /* what the device lacks and Nativ has no fallback for ("" none): creation will fail */
} VkCapsPlan;

static inline void vk_caps_need(VkCapsPlan *p, int have, const char *name) {
	if (have) return;
	size_t n = strlen(p->missing);
	snprintf(p->missing + n, sizeof p->missing - n, "%s%s", n ? ", " : "", name);
}

static inline void vk_caps_plan(const VkCapsInfo *c, VkCapsPlan *p) {
	memset(p, 0, sizeof *p);
	p->depth_clamp = 1; p->fragment_stores = 1;   /* (an unknown device: ask as before) */
	if (!c || !c->known) return;
	p->depth_clamp = c->features[VKCAPS_DEPTH_CLAMP] != 0;
	p->fragment_stores = c->features[VKCAPS_FRAGMENT_STORES_AND_ATOMICS] != 0;
	/* (version numbers: major in bits 22-28, minor in 12-21) */
	vk_caps_need(p, c->api_version >= ((1u << 22) | (1u << 12)), "Vulkan 1.1 (the shaders are SPIR-V 1.3)");
	vk_caps_need(p, c->has_swapchain, "VK_KHR_swapchain");
	vk_caps_need(p, c->has_maintenance1, "VK_KHR_maintenance1");
	vk_caps_need(p, c->features[VKCAPS_ROBUST_BUFFER_ACCESS] != 0, "robustBufferAccess");
	/* SDL requires these and has no property that turns them off */
	vk_caps_need(p, c->features[VKCAPS_INDEPENDENT_BLEND] != 0, "independentBlend");
	vk_caps_need(p, c->features[VKCAPS_SAMPLE_RATE_SHADING] != 0, "sampleRateShading");
	vk_caps_need(p, c->features[VKCAPS_IMAGE_CUBE_ARRAY] != 0, "imageCubeArray");
	/* SDL requires these two by default; Nativ keeps them on (every sampled Mali report has them) */
	vk_caps_need(p, c->features[VKCAPS_SAMPLER_ANISOTROPY] != 0, "samplerAnisotropy");
	vk_caps_need(p, c->features[VKCAPS_DRAW_INDIRECT_FIRST_INSTANCE] != 0, "drawIndirectFirstInstance");
	if (c->max_bound_descriptor_sets < VKCAPS_MIN_BOUND_SETS) {
		char t[80]; snprintf(t, sizeof t, "maxBoundDescriptorSets %u < %u", (unsigned)c->max_bound_descriptor_sets, VKCAPS_MIN_BOUND_SETS);
		vk_caps_need(p, 0, t);
	}
}
#endif
