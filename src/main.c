#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

struct vulkan_state {
    VkInstance instance;
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

static struct vulkan_state* initialize_vulkan(void)
{
    struct vulkan_state *state = NULL;
    VkResult res;

    state = malloc(sizeof(*state));
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

    res = vkCreateInstance(&info, NULL, &state->instance);
    check_vkresult(res);

    return state;
}

int main()
{
    struct vulkan_state *state = initialize_vulkan();
    (void)state;

    return 0;
}
