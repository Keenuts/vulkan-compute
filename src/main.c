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

#define BUFFER_COUNT 2
#define SHADER_NAME "sum.spv"
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

    dump_available_layers();

    const char* validation_layers[] = {
        "VK_LAYER_LUNARG_standard_validation",
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
    free(bindings);
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
                                VkBuffer buffer,
                                VkDeviceSize size,
                                uint32_t binding)
{
    VkDescriptorBufferInfo buffer_info = {
        buffer,
        0,
        size,
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

static void initialize_device(struct vulkan_state *state)
{
    select_physical_device(state);
    create_logical_device(state);
    descriptor_pool_create(state, BUFFER_COUNT);
    command_pool_create(state);
    descriptor_set_layouts_create(state, BUFFER_COUNT);
    descriptor_set_create(state);
}

static VkDeviceMemory allocate_gpu_memory(struct vulkan_state *state, VkDeviceSize size)
{
    uint32_t memory_index = UINT32_MAX;
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
        size,
        memory_index
    };

    VkDeviceMemory vk_memory;
    CALL_VK(vkAllocateMemory, (state->device, &alloc_info, NULL, &vk_memory));

    return vk_memory;
}

static VkBuffer create_gpu_buffer(struct vulkan_state *state, VkDeviceSize size)
{
    VkBufferCreateInfo buffer_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        NULL /* ignored since marked as exclusive */
    };

    VkBuffer vk_buffer;
    CALL_VK(vkCreateBuffer, (state->device, &buffer_info, NULL, &vk_buffer));

    return vk_buffer;
}

static struct gpu_memory allocate_buffer(struct vulkan_state *state, uint64_t size)
{
    VkDeviceSize vk_size = size;
    VkDeviceMemory vk_memory = allocate_gpu_memory(state, vk_size);
    VkBuffer vk_buffer = create_gpu_buffer(state, vk_size);

    CALL_VK(vkBindBufferMemory, (state->device, vk_buffer, vk_memory, 0));

    struct gpu_memory info = {
        NULL,
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


static uint32_t* load_shader(const char *path, size_t *file_length)
{
    assert(file_length);
    uint32_t *content = NULL;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;

    do {
        size_t size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        if (size == (size_t)-1) {
            break;
        }

        content = malloc(size);
        if (content == NULL) {
            break;
        }

        if (read(fd, content, size) < 0) {
            free(content);
            content = NULL;
            break;
        }

        *file_length = size;
    } while (0);

    close(fd);
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

static void destroy_state(struct vulkan_state **state)
{
    assert(state && *state);
    struct vulkan_state *st = *state;

#define FREE_VK(Field, Function)                \
    if (st->Field != VK_NULL_HANDLE)            \
        Function(st->device, st->Field, NULL)

    FREE_VK(shader_module, vkDestroyShaderModule);
    FREE_VK(descriptor_pool, vkDestroyDescriptorPool);
    FREE_VK(descriptor_layout, vkDestroyDescriptorSetLayout);
    FREE_VK(pipeline_layout, vkDestroyPipelineLayout);
    FREE_VK(pipeline, vkDestroyPipeline);
    FREE_VK(command_pool, vkDestroyCommandPool);

    if (st->device != VK_NULL_HANDLE)
        vkDestroyDevice(st->device, NULL);
    if (st->instance != VK_NULL_HANDLE)
        vkDestroyInstance(st->instance, NULL);

    free(st);
    *state = NULL;
}


/* Application logic */

static void generate_payload(int *buffer)
{
    for (int i = 0; i < ELT_COUNT; i++) {
        buffer[i] = i;
    }
}

static void check_payload(int *buffer)
{
    for (int i = 0; i < ELT_COUNT; i++) {
        if (buffer[i] != i + i) {
            fprintf(stderr, "invalid value for [%d]. got %d, expected %d\n",
                    i, buffer[i], i + i);
        }
        assert(buffer[i] == i + i);
    }
}

static void do_sum_one_buffer_one_memory(struct vulkan_state *state)
{
    struct gpu_memory a;
    void *ptr;

    a = allocate_buffer(state, sizeof(int) * ELT_COUNT);
    descriptor_set_bind(state, a.vk_buffer, a.vk_size, 0);
    descriptor_set_bind(state, a.vk_buffer, a.vk_size, 1);

    CALL_VK(vkMapMemory, (state->device, a.vk_memory, 0, a.vk_size, 0, &ptr));
    generate_payload(ptr);
    vkUnmapMemory(state->device, a.vk_memory);

    execute_sum_kernel(state);

    CALL_VK(vkMapMemory, (state->device, a.vk_memory, 0, a.vk_size, 0, &ptr));
    check_payload(ptr);
    vkUnmapMemory(state->device, a.vk_memory);

    free_buffer(state, &a);
}

static void do_sum_two_buffer_one_memory(struct vulkan_state *state)
{
    VkDeviceMemory memory;
    VkBuffer buffer_a, buffer_b;
    void *ptr_a, *ptr_b;
    VkDeviceSize size = ELT_COUNT * sizeof(int);

    memory = allocate_gpu_memory(state, size * 2);
    buffer_a = create_gpu_buffer(state, size);
    buffer_b = create_gpu_buffer(state, size);

    CALL_VK(vkBindBufferMemory, (state->device, buffer_a, memory, 0));
    CALL_VK(vkBindBufferMemory, (state->device, buffer_b, memory, size));

    descriptor_set_bind(state, buffer_a, size, 0);
    descriptor_set_bind(state, buffer_b, size, 1);

    CALL_VK(vkMapMemory, (state->device, memory,    0, size, 0, &ptr_a));
    generate_payload(ptr_a);
    vkUnmapMemory(state->device, memory);

    execute_sum_kernel(state);

    CALL_VK(vkMapMemory, (state->device, memory, size, size, 0, &ptr_b));
    check_payload(ptr_b);
    vkUnmapMemory(state->device, memory);


    vkDestroyBuffer(state->device, buffer_a, NULL);
    vkDestroyBuffer(state->device, buffer_b, NULL);
    vkFreeMemory(state->device, memory, NULL);
}

static void do_sum_two_buffer_two_memory(struct vulkan_state *state)
{
    struct gpu_memory a, b;
    void *ptr_a, *ptr_b;

    a = allocate_buffer(state, sizeof(int) * ELT_COUNT);
    descriptor_set_bind(state, a.vk_buffer, a.vk_size, 0);

    b = allocate_buffer(state, sizeof(int) * ELT_COUNT);
    descriptor_set_bind(state, b.vk_buffer, b.vk_size, 1);

    CALL_VK(vkMapMemory, (state->device, a.vk_memory, 0, a.vk_size, 0, &ptr_a));
    CALL_VK(vkMapMemory, (state->device, b.vk_memory, 0, b.vk_size, 0, &ptr_b));

    generate_payload(ptr_a);
    execute_sum_kernel(state);
    check_payload(ptr_b);

    vkUnmapMemory(state->device, a.vk_memory);
    vkUnmapMemory(state->device, b.vk_memory);

    free_buffer(state, &a);
    free_buffer(state, &b);
}

int main(int argc, char **argv)
{
    if (argc <= 0)
        return 1;

    struct vulkan_state *state = NULL;
    uint32_t *shader_code = NULL;
    size_t shader_length;

    state = create_state();
    if (state == NULL)
        return 1;

    initialize_device(state);

    char *path = dirname(strdup(argv[0]));
    size_t pathlen = strlen(path) + strlen(SHADER_NAME) + 2;
    path = realloc(path, pathlen);
    strcat(path, "/" SHADER_NAME);

    shader_code = load_shader(path, &shader_length);
    printf("path: %s\n", path);
    free(path);
    path = NULL;


    if (shader_code == NULL) {
        fprintf(stderr, "unable to load the shader.\n");
        destroy_state(&state);
        return 2;
    }

    create_pipeline(state, shader_code, shader_length);

    do_sum_one_buffer_one_memory(state);
    do_sum_two_buffer_one_memory(state);
    do_sum_two_buffer_two_memory(state);

    free(shader_code);
    destroy_state(&state);

    return 0;
}
