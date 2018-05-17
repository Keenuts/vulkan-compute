#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

struct vulkan_state {
    VkInstance instance;
    VkPhysicalDevice phys_device;
    VkDevice device;
    VkDescriptorSetLayout layout_desc;
};

struct gpu_memory {
    void           *buffer;
    VkDeviceSize    vk_size;
    VkDeviceMemory  vk_memory;
    VkBuffer        vk_buffer;
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

static void check_vkresult(VkResult res)
{
    if (res == VK_SUCCESS) {
        return;
    }

    fprintf(stderr, "error: %s\n", vkresult_to_string(res));
    assert(0);
}

#define CALL_VK(Func, Param) check_vkresult(Func Param)

static struct vulkan_state* initialize_vulkan(void)
{
    struct vulkan_state *state = malloc(sizeof(*state));
    assert(state);

    struct VkApplicationInfo app_info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        NULL,
        "sample-compute",
        1,
        "sample-engine",
        1,
        VK_MAKE_VERSION(1, 0, 0)
    };

    struct VkInstanceCreateInfo info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        NULL,
        0,
        &app_info,
        0,
        NULL,
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
    assert(device_count > 0);

    devices = malloc(sizeof(*devices) * device_count);
    assert(devices);

    CALL_VK(vkEnumeratePhysicalDevices, (state->instance, &device_count, devices));

    state->phys_device = devices[0];
}

static VkDeviceQueueCreateInfo find_queue(struct vulkan_state *state)
{
    uint32_t count;
    VkQueueFamilyProperties *properties;

    vkGetPhysicalDeviceQueueFamilyProperties(state->phys_device, &count, NULL);
    assert(count > 0);

    properties = malloc(sizeof(*properties) * count);
    assert(properties);

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
}

static void initialize_device(struct vulkan_state *state)
{
    select_physical_device(state);
    create_logical_device(state);
}

static struct gpu_memory allocate_buffer(struct vulkan_state *state, uint64_t size)
{
    uint32_t memory_index = UINT32_MAX;
    VkDeviceSize vk_size = size;
    VkPhysicalDeviceMemoryProperties props;

    vkGetPhysicalDeviceMemoryProperties(state->phys_device, &props);

    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        VkMemoryType type = props.memoryTypes[i];

        if (type.propertyFlags | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT &&
            type.propertyFlags | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
            memory_index = i;
            break;
        }
    }
    assert(memory_index < UINT32_MAX);

    VkMemoryAllocateInfo alloc_info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        vk_size,
        memory_index
    };

    VkDeviceMemory vk_memory;
    CALL_VK(vkAllocateMemory, (state->device, &alloc_info, NULL, &vk_memory));

    VkBufferCreateInfo buffer_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        vk_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        NULL /* ignored since marked as exclusive */
    };

    VkBuffer vk_buffer;
    CALL_VK(vkCreateBuffer, (state->device, &buffer_info, NULL, &vk_buffer));

    CALL_VK(vkBindBufferMemory, (state->device, vk_buffer, vk_memory, vk_size));


    void *data;
    CALL_VK(vkMapMemory, (state->device, vk_memory, 0, vk_size, 0, &data));

    struct gpu_memory info = {
        data,
        vk_size,
        vk_memory,
        vk_buffer,
    };

    return info;
}

static void free_buffer(struct vulkan_state *state, struct gpu_memory *mem)
{
    if (mem->buffer) {
        vkUnmapMemory(state->device, mem->vk_memory);
        mem->buffer = NULL;
    }

    vkFreeMemory(state->device, mem->vk_memory, NULL);
    vkDestroyBuffer(state->device, mem->vk_buffer, NULL);
}

static void create_descriptor_sets(struct vulkan_state *state)
{
    VkDescriptorSetLayoutBinding binding_input = {
        0,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        NULL
    };

    VkDescriptorSetLayoutBinding binding_output = {
        0,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        1,
        VK_SHADER_STAGE_COMPUTE_BIT,
        NULL
    };
    VkDescriptorSetLayoutBinding bindings[2] = { binding_input, binding_output };

    VkDescriptorSetLayoutCreateInfo info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        1,
        bindings
    };

    CALL_VK(vkCreateDescriptorSetLayout,
            (state->device, &info, NULL, &state->layout_desc));
}

int main()
{
    const int ELT_COUNT = 1000;

    struct vulkan_state *state = NULL;
    int *input_data = NULL;
    struct gpu_memory input_memory, output_memory;

    state = initialize_vulkan();
    initialize_device(state);

    input_memory  = allocate_buffer(state, sizeof(*input_data) * ELT_COUNT);
    output_memory = allocate_buffer(state, sizeof(*input_data) * ELT_COUNT);

    free_buffer(state, &input_memory);
    free_buffer(state, &output_memory);

    create_descriptor_sets(state);

    vkDestroyDevice(state->device, NULL);
    vkDestroyInstance(state->instance, NULL);

    return 0;
}
