#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vulkan/vulkan.h>

#define BUFFER_COUNT 2
#define SHADER_PATH "./sum.spv"
#define SHADER_ENTRY_POINT "main"

struct vulkan_state {
    VkInstance              instance;
    VkPhysicalDevice        phys_device;
    VkDevice                device;
    VkQueue                 queue;
    uint32_t                queue_family_index;

    VkDescriptorPool        descriptor_pool;
    VkCommandPool           command_pool;

    VkDescriptorSetLayout   descriptor_layout;
    VkDescriptorSet         descriptor_set;
    VkPipelineLayout        pipeline_layout;
    VkPipeline              pipeline;
    VkShaderModule          shader_module;
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

    state->queue_family_index = compute_queue_index;
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

static void descriptor_set_layouts_create(struct vulkan_state *state, uint32_t count)
{
    VkDescriptorSetLayoutBinding *bindings = malloc(sizeof(*bindings) * count);
    assert(bindings);

    for (uint32_t i = 0; i < count; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].pImmutableSamplers = NULL;
    }

    VkDescriptorSetLayoutCreateInfo info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL,
        0,
        count,
        bindings
    };

    CALL_VK(vkCreateDescriptorSetLayout,
            (state->device, &info, NULL, &state->descriptor_layout));
}

static void descriptor_pool_create(struct vulkan_state *state, uint32_t size)
{
    VkDescriptorPoolSize pool_size = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        size
    };

    VkDescriptorPoolCreateInfo info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        NULL,
        0 /* no flags */,
        size,
        1,
        &pool_size
    };

    CALL_VK(vkCreateDescriptorPool,
            (state->device, &info, NULL, &state->descriptor_pool));
}

static void command_pool_create(struct vulkan_state *state)
{
    VkCommandPoolCreateInfo pool_info = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        NULL,
        0,
        state->queue_family_index
    };

    CALL_VK(vkCreateCommandPool,
            (state->device, &pool_info, NULL, &state->command_pool));
}

static void descriptor_set_create(struct vulkan_state *state)
{
    VkDescriptorSetAllocateInfo alloc_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        NULL,
        state->descriptor_pool,
        1,
        &state->descriptor_layout
    };

    CALL_VK(vkAllocateDescriptorSets,
            (state->device, &alloc_info, &state->descriptor_set));
}

static void descriptor_set_bind(struct vulkan_state *state,
                              struct gpu_memory *buffer,
                              uint32_t binding)
{
    VkDescriptorBufferInfo buffer_info = {
        buffer->vk_buffer,
        0,
        buffer->vk_size,
    };

    VkWriteDescriptorSet write_info = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        NULL,
        state->descriptor_set,
        binding,
        0,
        1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        NULL,
        &buffer_info,
        NULL
    };

    vkUpdateDescriptorSets(state->device, 1, &write_info, 0, NULL);
}

static uint32_t* load_shader(const char *path, size_t *file_length)
{
    assert(file_length);

    int fd = open(path, O_RDONLY);
    assert(fd >= 0);

    size_t size = lseek(fd, 0, SEEK_END);
    assert(size != (size_t)-1);
    lseek(fd, 0, SEEK_SET);

    uint32_t *content = malloc(size);
    assert(content);
    assert(read(fd, content, size) >= 0);
    close(fd);

    *file_length = size;
    return content;
}

static void create_pipeline(struct vulkan_state *state,
                            const uint32_t *shader,
                            uint32_t shader_len)
{
    VkShaderModuleCreateInfo shader_info = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        NULL,
        0,
        shader_len,
        shader
    };

    CALL_VK(vkCreateShaderModule,
            (state->device, &shader_info, NULL, &state->shader_module));

    VkPipelineShaderStageCreateInfo shader_stage_creation_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL,
        0,
        VK_SHADER_STAGE_COMPUTE_BIT,
        state->shader_module,
        SHADER_ENTRY_POINT,
        NULL
    };

    VkPipelineLayoutCreateInfo layout_info = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        NULL,
        0,
        1,
        &state->descriptor_layout,
        0,
        NULL
    };

    CALL_VK(vkCreatePipelineLayout,
            (state->device, &layout_info, NULL, &state->pipeline_layout));

    VkComputePipelineCreateInfo pipeline_info = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        NULL,
        0,
        shader_stage_creation_info,
        state->pipeline_layout,
        VK_NULL_HANDLE,
        0
    };

    CALL_VK(vkCreateComputePipelines,
            (state->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &state->pipeline));
}

static void execute_sum_kernel(struct vulkan_state *state)
{
    VkCommandBuffer command_buffer;

    VkCommandBufferAllocateInfo alloc_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        NULL,
        state->command_pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1
    };

    CALL_VK(vkAllocateCommandBuffers, (state->device, &alloc_info, &command_buffer));

    VkCommandBufferBeginInfo begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        NULL,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        NULL
    };

    CALL_VK(vkBeginCommandBuffer, (command_buffer, &begin_info));

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, state->pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            state->pipeline_layout,
                            0,
                            1,
                            &state->descriptor_set,
                            0,
                            NULL);

    vkCmdDispatch(command_buffer, ELT_COUNT / WORKGROUP_SIZE, 1, 1);

    CALL_VK(vkEndCommandBuffer, (command_buffer));

    VkSubmitInfo submit_info = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        NULL,
        0,
        NULL,
        NULL,
        1,
        &command_buffer,
        0,
        NULL
    };

    VkFence fence;
    VkFenceCreateInfo fence_info = {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        NULL,
        0
    };

    CALL_VK(vkCreateFence, (state->device, &fence_info, NULL, &fence));

    CALL_VK(vkQueueSubmit, (state->queue, 1, &submit_info, fence));
    CALL_VK(vkWaitForFences, (state->device, 1, &fence, VK_TRUE, 1e9 * 5));

    vkDestroyFence(state->device, fence, NULL);
}

int main()
{
    struct vulkan_state *state = NULL;
    struct gpu_memory buffers[BUFFER_COUNT];

    state = initialize_vulkan();
    initialize_device(state);

    descriptor_pool_create(state, BUFFER_COUNT);
    command_pool_create(state);

    descriptor_set_layouts_create(state, BUFFER_COUNT);
    descriptor_set_create(state);

    for (uint32_t i = 0; i < BUFFER_COUNT; i++) {
        buffers[i] = allocate_buffer(state, sizeof(buffers[i]) * ELT_COUNT);
        descriptor_set_bind(state, &buffers[i], i);
    }

    size_t shader_length;
    uint32_t *shader = load_shader(SHADER_PATH, &shader_length);
    printf("shader loaded (%zu bytes)\n", shader_length);

    create_pipeline(state, shader, shader_length);

    execute_sum_kernel(state);

    /* Cleanup */
    for (uint32_t i = 0; i < BUFFER_COUNT; i++) {
        free_buffer(state, &buffers[i]);
    }

    free(shader);
    vkDestroyShaderModule(state->device, state->shader_module, NULL);
    vkDestroyDevice(state->device, NULL);
    vkDestroyInstance(state->instance, NULL);

    return 0;
}
