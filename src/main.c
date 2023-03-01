#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vulkan/vulkan.h>

#ifdef USE_DXC
# define SHADER_NAME "sum.hlsl.spv"
#elif USE_GLSLANG
# define SHADER_NAME "sum.glsl.spv"
#elif USE_WGSL
# define SHADER_NAME "sum.wgsl.spv"
#else
# error "USE_DXC, USE_GLSLANG or USE_WGSL not set"
#endif

#define BUFFER_COUNT 2
#define SHADER_ENTRY_POINT "main"
#define REDHAT_VENDOR_ID 0x1af4
#define VIRTIOGPU_DEVICE_ID 0x1012
#define VIRTIO_VAR_NAME "USE_VIRTIOGPU"

struct vulkan_state {
    VkInstance              instance;
    VkPhysicalDevice        phys_device;
    VkDevice                device;
    VkQueue                 queue;
    uint32_t                queue_family_index;

    VkDescriptorPool        descriptor_pool;
    VkDescriptorSetLayout   descriptor_layout;
    VkDescriptorSet         descriptor_set;
};

static const char* vkresult_to_string(VkResult res)
{
    switch (res)
    {
#define VK2STR(Value) case Value: return #Value
        VK2STR(VK_SUCCESS);
        VK2STR(VK_NOT_READY);
        VK2STR(VK_TIMEOUT);
        VK2STR(VK_EVENT_SET);
        VK2STR(VK_EVENT_RESET);
        VK2STR(VK_INCOMPLETE);
        VK2STR(VK_ERROR_OUT_OF_HOST_MEMORY);
        VK2STR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        VK2STR(VK_ERROR_INITIALIZATION_FAILED);
        VK2STR(VK_ERROR_DEVICE_LOST);
        VK2STR(VK_ERROR_MEMORY_MAP_FAILED);
        VK2STR(VK_ERROR_LAYER_NOT_PRESENT);
        VK2STR(VK_ERROR_EXTENSION_NOT_PRESENT);
        VK2STR(VK_ERROR_FEATURE_NOT_PRESENT);
        VK2STR(VK_ERROR_INCOMPATIBLE_DRIVER);
        VK2STR(VK_ERROR_TOO_MANY_OBJECTS);
        VK2STR(VK_ERROR_FORMAT_NOT_SUPPORTED);
        VK2STR(VK_ERROR_FRAGMENTED_POOL);
        VK2STR(VK_ERROR_OUT_OF_POOL_MEMORY);
        VK2STR(VK_ERROR_INVALID_EXTERNAL_HANDLE);
        VK2STR(VK_ERROR_SURFACE_LOST_KHR);
        VK2STR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        VK2STR(VK_SUBOPTIMAL_KHR);
        VK2STR(VK_ERROR_OUT_OF_DATE_KHR);
        VK2STR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        VK2STR(VK_ERROR_VALIDATION_FAILED_EXT);
        VK2STR(VK_ERROR_INVALID_SHADER_NV);
        VK2STR(VK_ERROR_FRAGMENTATION_EXT);
        VK2STR(VK_ERROR_NOT_PERMITTED_EXT);
        VK2STR(VK_RESULT_MAX_ENUM);
#undef VK2STR
        default:
            return "VK_UNKNOWN_RETURN_VALUE";
    }
}

static void check_vkresult(const char* fname, VkResult res)
{
    if (res == VK_SUCCESS) {
        fprintf(stderr, "\033[32m%s\033[0m\n", fname);
        return;
    }

    fprintf(stderr, "\033[31m%s = %s\033[0m\n", fname, vkresult_to_string(res));
    assert(0);
}

#define CALL_VK(Func, Param) check_vkresult(#Func, Func Param)

static void dump_available_layers(void)
{
    uint32_t layer_count;
    CALL_VK(vkEnumerateInstanceLayerProperties, (&layer_count, NULL));

    if (layer_count == 0) {
        fprintf(stderr, "no layers available.\n");
        return;
    }

    VkLayerProperties *layers = malloc(sizeof(*layers) * layer_count);
    assert(layers);


    CALL_VK(vkEnumerateInstanceLayerProperties, (&layer_count, layers));

    fprintf(stderr, "layers:\n");
    for (uint32_t i = 0; i < layer_count; i++) {
        fprintf(stderr, "\t%s: %s\n", layers[i].layerName, layers[i].description);
    }

    free(layers);
}

static struct vulkan_state* create_state(void)
{
    struct vulkan_state *state = malloc(sizeof(*state));
    if (NULL == state) {
        abort();
    }
    memset(state, 0, sizeof(*state));

    struct VkApplicationInfo app_info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        NULL,
        "sample-compute",
        1,
        "sample-engine",
        1,
        VK_API_VERSION_1_2
    };

    dump_available_layers();

    const char* validation_layers[] = {
#ifdef DEBUG
        "VK_LAYER_KHRONOS_validation",
#endif
    };

    struct VkInstanceCreateInfo info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        NULL,
        0,
        &app_info,
        sizeof(validation_layers) / sizeof(*validation_layers),
        validation_layers,
        0,
        NULL
    };

    CALL_VK(vkCreateInstance, (&info, NULL, &state->instance));

    return state;
}

static void select_physical_device(struct vulkan_state *state)
{
    uint32_t device_count;
    VkPhysicalDevice *devices = NULL;

    CALL_VK(vkEnumeratePhysicalDevices, (state->instance, &device_count, NULL));
    if (device_count <= 0) {
        abort();
    }

    devices = malloc(sizeof(*devices) * device_count);
    if (devices == NULL)
        exit(1);

    CALL_VK(vkEnumeratePhysicalDevices, (state->instance, &device_count, devices));

    uint32_t device_index = UINT_MAX;

    printf("%d available devices\n", device_count);
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);

        printf("\t[%u] - %s (v:0x%x, d:0x%x)\n",
            i, props.deviceName, props.vendorID, props.deviceID);

        if (props.vendorID != REDHAT_VENDOR_ID) {
            continue;
        }

        if (props.deviceID != VIRTIOGPU_DEVICE_ID) {
            continue;
        }

        device_index = i;
    }

    if (!getenv(VIRTIO_VAR_NAME)) {
        fprintf(stderr, "the application will allow non-virtiogpu devices.\n");
        device_index = 0;
    }

    if (device_index == UINT_MAX) {
        fprintf(stderr, "Unable to find any virtio-gpu device. Aborting now.\n");
        abort();
    }

    printf("loading device id=%u\n", device_index);
    state->phys_device = devices[device_index];
    free(devices);
}

static VkDeviceQueueCreateInfo find_queue(struct vulkan_state *state)
{
    uint32_t count;
    VkQueueFamilyProperties *properties;

    vkGetPhysicalDeviceQueueFamilyProperties(state->phys_device, &count, NULL);
    if (count <= 0) {
        abort();
    }

    properties = malloc(sizeof(*properties) * count);
    if (NULL == properties) {
        abort();
    }

    vkGetPhysicalDeviceQueueFamilyProperties(state->phys_device, &count, properties);


    uint32_t compute_queue_index = UINT32_MAX;

    for (uint32_t i = 0; i < count; i++) {
        if (properties[i].queueFlags | VK_QUEUE_COMPUTE_BIT) {
            compute_queue_index = i;
            break;
        }
    }
    assert(compute_queue_index < UINT32_MAX);

    const float priorities[] = { 1.f };

    VkDeviceQueueCreateInfo queue_info = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        NULL,
        0,
        compute_queue_index,
        1,
        priorities
    };

    state->queue_family_index = compute_queue_index;
    free(properties);
    return queue_info;
}

static void create_logical_device(struct vulkan_state *state)
{
    VkDeviceQueueCreateInfo queue_info = find_queue(state);

    struct VkDeviceCreateInfo info = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        NULL,
        0,
        1,
        &queue_info,
        0,
        NULL,
        0,
        NULL,
        NULL
    };

    CALL_VK(vkCreateDevice, (state->phys_device, &info, NULL, &state->device));
    vkGetDeviceQueue(state->device, queue_info.queueFamilyIndex, 0, &state->queue);
}

static void initialize_device(struct vulkan_state *state)
{
    select_physical_device(state);
    create_logical_device(state);

    // Create pool
    VkDescriptorPoolSize pool_size[5];
    pool_size[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size[0].descriptorCount = 512;
    pool_size[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    pool_size[1].descriptorCount = 512;
    pool_size[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    pool_size[2].descriptorCount = 512;
    // The additional descriptors.
    pool_size[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size[3].descriptorCount = 512;
    pool_size[4].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    pool_size[4].descriptorCount = 512;

#if 1
    const size_t DESC_TYPE_COUNT = 3;
#else
    const size_t DESC_TYPE_COUNT = 5;
#endif

    size_t max_sets = 0;
    for (size_t i = 0; i < DESC_TYPE_COUNT; i++) {
        max_sets += pool_size[i].descriptorCount;
    }


    VkDescriptorPoolCreateInfo desc_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        NULL,
        0 /* no flags */,
        max_sets,
        DESC_TYPE_COUNT,
        pool_size
    };

    CALL_VK(vkCreateDescriptorPool,
            (state->device, &desc_info, NULL, &state->descriptor_pool));


    // Create layout
    VkDescriptorSetLayoutBinding bindings[7];
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = NULL;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = NULL;
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = NULL;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].pImmutableSamplers = NULL;
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[4].pImmutableSamplers = NULL;

    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[5].pImmutableSamplers = NULL;
    bindings[6].binding = 6;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[6].pImmutableSamplers = NULL;

    VkDescriptorSetLayoutCreateInfo layout_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        7,
        bindings
    };

    CALL_VK(vkCreateDescriptorSetLayout,
            (state->device, &layout_info, NULL, &state->descriptor_layout));

    size_t allocated_descriptors = 0;
    // Allocate descriptors
    for (size_t i = 0; i < 207; i++) {
        VkDescriptorSetAllocateInfo alloc_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            NULL,
            state->descriptor_pool,
            1,
            &state->descriptor_layout
        };

        printf("Allocated descriptors so far: %zu/%zu\n", allocated_descriptors, max_sets);
        CALL_VK(vkAllocateDescriptorSets,
                (state->device, &alloc_info, &state->descriptor_set));
        allocated_descriptors += 7;
    }
}

static void destroy_state(struct vulkan_state **state)
{
    assert(state && *state);
    struct vulkan_state *st = *state;

#define FREE_VK(Field, Function)                \
    if (st->Field != VK_NULL_HANDLE)            \
        Function(st->device, st->Field, NULL)

    FREE_VK(descriptor_pool, vkDestroyDescriptorPool);
    FREE_VK(descriptor_layout, vkDestroyDescriptorSetLayout);

    if (st->device != VK_NULL_HANDLE)
        vkDestroyDevice(st->device, NULL);
    if (st->instance != VK_NULL_HANDLE)
        vkDestroyInstance(st->instance, NULL);

    free(st);
    *state = NULL;
}

int main(int argc, char **argv)
{
    if (argc <= 0)
        return 1;

    struct vulkan_state *state = NULL;

    state = create_state();
    if (state == NULL)
        return 1;

    initialize_device(state);
    destroy_state(&state);

    puts("bye bye");
    return 0;
}
