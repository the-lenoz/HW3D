module;

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "embedded_spirv.hpp"

module hw3d.gpu_pipeline;

namespace hw3d {
namespace {

constexpr std::uint32_t compute_local_size = 64;

struct alignas(16) Float4 {
    float x{};
    float y{};
    float z{};
    float w{};
};

struct alignas(16) UInt4 {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t z{};
    std::uint32_t w{};
};

struct alignas(16) PackedInitialTriangle {
    Float4 a;
    Float4 b;
    Float4 c;
    Float4 axis_origin;
    Float4 axis_direction_and_speed;
};

struct alignas(16) PackedRuntimeTriangle {
    UInt4 a_and_highlight;
    Float4 b;
    Float4 c;
    Float4 minimum;
    Float4 maximum;
};

struct alignas(16) PackedStaticEntry {
    std::array<std::int32_t, 4> voxel{};
};

static_assert(sizeof(Float4) == 16 && alignof(Float4) == 16);
static_assert(sizeof(UInt4) == 16 && alignof(UInt4) == 16);
static_assert(sizeof(PackedInitialTriangle) == 80);
static_assert(alignof(PackedInitialTriangle) == 16);
static_assert(offsetof(PackedInitialTriangle, a) == 0);
static_assert(offsetof(PackedInitialTriangle, b) == 16);
static_assert(offsetof(PackedInitialTriangle, c) == 32);
static_assert(offsetof(PackedInitialTriangle, axis_origin) == 48);
static_assert(
    offsetof(PackedInitialTriangle, axis_direction_and_speed) == 64);
static_assert(sizeof(PackedRuntimeTriangle) == 80);
static_assert(alignof(PackedRuntimeTriangle) == 16);
static_assert(offsetof(PackedRuntimeTriangle, a_and_highlight) == 0);
static_assert(offsetof(PackedRuntimeTriangle, b) == 16);
static_assert(offsetof(PackedRuntimeTriangle, c) == 32);
static_assert(offsetof(PackedRuntimeTriangle, minimum) == 48);
static_assert(offsetof(PackedRuntimeTriangle, maximum) == 64);
static_assert(sizeof(TrianglePair) == 8);
static_assert(sizeof(PackedStaticEntry) == 16);

Float4 pack_point(const Vec3 point, const float w = 0.0F) noexcept
{
    return {point.x, point.y, point.z, w};
}

}

PackedInitialTriangle pack(const InitialTriangle& triangle) noexcept;
std::int32_t gpu_voxel_coordinate(std::int64_t value) noexcept;
vk::DeviceSize align_up(vk::DeviceSize, vk::DeviceSize) noexcept;
vk::DeviceSize checked_size(std::size_t, std::size_t, const char*);
void check_elapsed(std::chrono::duration<float> elapsed);
VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT,
    vk::DebugUtilsMessageTypeFlagsEXT,
    const vk::DebugUtilsMessengerCallbackDataEXT*,
    void*) noexcept;

bool contains_name(
    const std::vector<vk::LayerProperties>& values,
    const char* name)
{
    return std::ranges::any_of(values, [&](const auto& value) {
        return std::strcmp(value.layerName, name) == 0;
    });
}

bool contains_name(
    const std::vector<vk::ExtensionProperties>& values,
    const char* name)
{
    return std::ranges::any_of(values, [&](const auto& value) {
        return std::strcmp(value.extensionName, name) == 0;
    });
}

using Matrix4 = std::array<float, 16>;
constexpr float field_of_view = 1.0471975512F;
constexpr float initial_yaw = -1.5707963268F;
constexpr float maximum_pitch = 1.5533430343F;
constexpr float mouse_sensitivity = 0.002F;
Vec3 add(Vec3, Vec3) noexcept;
Vec3 scale(Vec3, float) noexcept;
Vec3 cross(Vec3, Vec3) noexcept;
float length(Vec3) noexcept;
Vec3 normalize(Vec3) noexcept;
Matrix4 multiply(const Matrix4&, const Matrix4&) noexcept;
Matrix4 perspective(float, float, float) noexcept;
Matrix4 look_at(Vec3, Vec3) noexcept;

class VulkanRuntime final {
public:
    VulkanRuntime(
        WindowContext& window,
        const vk::DeviceSize required_initial_size,
        const vk::DeviceSize required_runtime_size)
    {
        const std::uint32_t loader_version = context_.enumerateInstanceVersion();
        if (loader_version < VK_API_VERSION_1_4) {
            throw std::runtime_error(
                "Vulkan loader 1.4 or newer is required");
        }

        std::vector<const char*> instance_extensions;
        std::uint32_t count{};
        const char** const extensions =
            glfwGetRequiredInstanceExtensions(&count);
        if (extensions == nullptr || count == 0) {
            throw std::runtime_error(
                "GLFW did not provide Vulkan surface extensions");
        }
        instance_extensions.assign(extensions, extensions + count);

        std::vector<const char*> layers;
#if defined(HW3D_DEBUG_VALIDATION)
        const bool validation_disabled =
            std::getenv("HW3D_DISABLE_VALIDATION") != nullptr;
        const auto available_layers =
            context_.enumerateInstanceLayerProperties();
        const auto available_extensions =
            context_.enumerateInstanceExtensionProperties();
        if (!validation_disabled
            && contains_name(
                available_layers, "VK_LAYER_KHRONOS_validation")) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            if (contains_name(
                    available_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                instance_extensions.push_back(
                    VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                debug_enabled_ = true;
            }
        }
#endif

        const vk::ApplicationInfo application_info{
            .pApplicationName = "HW3D",
            .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .pEngineName = "HW3D",
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .apiVersion = VK_API_VERSION_1_4,
        };
        const vk::InstanceCreateInfo instance_info{
            .pApplicationInfo = &application_info,
            .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
            .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount =
                static_cast<std::uint32_t>(instance_extensions.size()),
            .ppEnabledExtensionNames = instance_extensions.data(),
        };
        instance_ = vk::raii::Instance(context_, instance_info);

        if (debug_enabled_) {
            const vk::DebugUtilsMessengerCreateInfoEXT debug_info{
                .messageSeverity =
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                    | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                .messageType =
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                    | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                    | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
                .pfnUserCallback = &debug_callback,
            };
            debug_messenger_ =
                vk::raii::DebugUtilsMessengerEXT(instance_, debug_info);
        }

        VkSurfaceKHR raw_surface{};
        const VkResult result = glfwCreateWindowSurface(
            static_cast<VkInstance>(*instance_),
            static_cast<GLFWwindow*>(window.native_handle()),
            nullptr,
            &raw_surface);
        if (result != VK_SUCCESS) {
            throw std::runtime_error(
                "failed to create the Vulkan window surface");
        }
        surface_ = vk::raii::SurfaceKHR(instance_, raw_surface);

        select_physical_device(required_initial_size, required_runtime_size);
        create_device();
    }

    [[nodiscard]] const vk::raii::Instance& instance() const noexcept
    {
        return instance_;
    }

    [[nodiscard]] const vk::raii::PhysicalDevice& physical() const noexcept
    {
        return physical_;
    }

    [[nodiscard]] const vk::raii::Device& device() const noexcept
    {
        return device_;
    }

    [[nodiscard]] const vk::raii::Queue& compute_queue() const noexcept
    {
        return compute_queue_;
    }

    [[nodiscard]] const vk::raii::Queue& present_queue() const noexcept
    {
        return present_queue_;
    }

    [[nodiscard]] const vk::raii::CommandPool& command_pool() const noexcept
    {
        return command_pool_;
    }

    [[nodiscard]] const vk::raii::SurfaceKHR& surface() const noexcept
    {
        return surface_;
    }

    [[nodiscard]] std::uint32_t compute_family() const noexcept
    {
        return compute_family_;
    }

    [[nodiscard]] std::uint32_t present_family() const noexcept
    {
        return present_family_;
    }

    [[nodiscard]] vk::PhysicalDeviceProperties properties() const
    {
        return physical_.getProperties();
    }

    [[nodiscard]] vk::PhysicalDeviceMemoryProperties memory_properties() const
    {
        return physical_.getMemoryProperties();
    }

private:
    struct Candidate {
        std::size_t index{};
        std::uint32_t compute_family{};
        std::uint32_t present_family{};
        int score{};
    };

    void select_physical_device(
        const vk::DeviceSize required_initial_size,
        const vk::DeviceSize required_runtime_size)
    {
        auto devices = instance_.enumeratePhysicalDevices();
        std::optional<Candidate> best;
        std::ostringstream rejected;
        for (std::size_t device_index = 0;
             device_index < devices.size();
             ++device_index) {
            auto& device = devices[device_index];
            const vk::PhysicalDeviceProperties properties =
                device.getProperties();
            std::string reason;
            if (properties.apiVersion < VK_API_VERSION_1_4) {
                reason = "API version is below 1.4";
            } else if (properties.limits.maxStorageBufferRange
                < std::max(required_initial_size, required_runtime_size)) {
                reason = "runtime or initial buffer exceeds maxStorageBufferRange";
            }

            VkPhysicalDeviceVulkan13Features features13{
                .sType =
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            VkPhysicalDeviceFeatures2 features2{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &features13};
            vkGetPhysicalDeviceFeatures2(
                static_cast<VkPhysicalDevice>(*device), &features2);
            if (reason.empty()
                && (!features13.dynamicRendering
                    || !features13.synchronization2)) {
                reason = "dynamic rendering or synchronization2 is unavailable";
            }

            const auto queue_families = device.getQueueFamilyProperties();
            std::optional<std::uint32_t> compute_family;
            std::optional<std::uint32_t> present_family;
            for (std::uint32_t index = 0;
                 index < queue_families.size();
                 ++index) {
                const auto flags = queue_families[index].queueFlags;
                const bool compute = static_cast<bool>(
                    flags & vk::QueueFlagBits::eCompute);
                const bool graphics = static_cast<bool>(
                    flags & vk::QueueFlagBits::eGraphics);
                if (!compute_family && compute && graphics) {
                    compute_family = index;
                }
                if (device.getSurfaceSupportKHR(index, *surface_)) {
                    present_family = index;
                }
            }
            if (reason.empty() && !compute_family) {
                reason = "no graphics+compute queue family";
            }

            if (reason.empty()) {
                const auto extensions =
                    device.enumerateDeviceExtensionProperties();
                if (!contains_name(
                        extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                    reason = "VK_KHR_swapchain is unavailable";
                } else if (!present_family) {
                    reason = "no presentation queue family";
                } else if (device.getSurfaceFormatsKHR(*surface_).empty()
                    || device.getSurfacePresentModesKHR(*surface_).empty()) {
                    reason = "surface has no formats or present modes";
                }
            }

            if (!reason.empty()) {
                rejected << properties.deviceName.data()
                    << ": " << reason << "; ";
                continue;
            }

            int score{};
            switch (properties.deviceType) {
            case vk::PhysicalDeviceType::eDiscreteGpu: score = 400; break;
            case vk::PhysicalDeviceType::eIntegratedGpu: score = 300; break;
            case vk::PhysicalDeviceType::eVirtualGpu: score = 200; break;
            case vk::PhysicalDeviceType::eCpu: score = 100; break;
            default: score = 0; break;
            }
            if (!best || score > best->score) {
                best = Candidate{
                    .index = device_index,
                    .compute_family = *compute_family,
                    .present_family = *present_family,
                    .score = score,
                };
            }
        }

        if (!best) {
            throw std::runtime_error(
                "no suitable Vulkan 1.4 device: " + rejected.str());
        }
        physical_ = std::move(devices[best->index]);
        compute_family_ = best->compute_family;
        present_family_ = best->present_family;
    }

    void create_device()
    {
        constexpr float priority = 1.0F;
        std::vector<vk::DeviceQueueCreateInfo> queues;
        queues.push_back({
            .queueFamilyIndex = compute_family_,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        });
        if (present_family_ != compute_family_) {
            queues.push_back({
                .queueFamilyIndex = present_family_,
                .queueCount = 1,
                .pQueuePriorities = &priority,
            });
        }

        const vk::PhysicalDeviceVulkan13Features features13{
            .synchronization2 = vk::True,
            .dynamicRendering = vk::True,
        };
        const std::array<const char*, 1> swapchain_extension{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        const vk::DeviceCreateInfo device_info{
            .pNext = &features13,
            .queueCreateInfoCount =
                static_cast<std::uint32_t>(queues.size()),
            .pQueueCreateInfos = queues.data(),
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = swapchain_extension.data(),
        };
        device_ = vk::raii::Device(physical_, device_info);
        compute_queue_ = device_.getQueue(compute_family_, 0);
        present_queue_ = device_.getQueue(present_family_, 0);
        command_pool_ = vk::raii::CommandPool(
            device_,
            vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = compute_family_,
            });
    }

    bool debug_enabled_{};
    std::uint32_t compute_family_{};
    std::uint32_t present_family_{};
    vk::raii::Context context_;
    vk::raii::Instance instance_{nullptr};
    vk::raii::DebugUtilsMessengerEXT debug_messenger_{nullptr};
    vk::raii::SurfaceKHR surface_{nullptr};
    vk::raii::PhysicalDevice physical_{nullptr};
    vk::raii::Device device_{nullptr};
    vk::raii::Queue compute_queue_{nullptr};
    vk::raii::Queue present_queue_{nullptr};
    vk::raii::CommandPool command_pool_{nullptr};
};

namespace {

class AllocatedBuffer final {
public:
    AllocatedBuffer() = default;

    AllocatedBuffer(
        const VulkanRuntime& runtime,
        const vk::DeviceSize size,
        const vk::BufferUsageFlags usage,
        const vk::MemoryPropertyFlags required,
        const vk::MemoryPropertyFlags preferred = {})
        : device_(&runtime.device()),
          size_(size)
    {
        buffer_ = vk::raii::Buffer(
            runtime.device(),
            vk::BufferCreateInfo{
                .size = size,
                .usage = usage,
                .sharingMode = vk::SharingMode::eExclusive,
            });
        const vk::MemoryRequirements requirements =
            buffer_.getMemoryRequirements();
        const auto properties = runtime.memory_properties();
        const auto choose = [&](const vk::MemoryPropertyFlags wanted)
            -> std::optional<std::uint32_t> {
            for (std::uint32_t index = 0;
                 index < properties.memoryTypeCount;
                 ++index) {
                if ((requirements.memoryTypeBits & (1U << index)) != 0
                    && (properties.memoryTypes[index].propertyFlags & wanted)
                        == wanted) {
                    return index;
                }
            }
            return std::nullopt;
        };
        const auto selected = choose(required | preferred)
            .or_else([&] { return choose(required); });
        if (!selected) {
            throw std::runtime_error(
                "no Vulkan memory type satisfies a buffer allocation");
        }
        memory_properties_ =
            properties.memoryTypes[*selected].propertyFlags;
        allocation_size_ = requirements.size;
        memory_ = vk::raii::DeviceMemory(
            runtime.device(),
            vk::MemoryAllocateInfo{
                .allocationSize = allocation_size_,
                .memoryTypeIndex = *selected,
            });
        buffer_.bindMemory(*memory_, 0);
    }

    [[nodiscard]] vk::Buffer handle() const noexcept
    {
        return *buffer_;
    }

    [[nodiscard]] vk::DeviceSize size() const noexcept
    {
        return size_;
    }

    [[nodiscard]] void* map()
    {
        if (!mapped_) {
            mapped_ = memory_.mapMemory(0, vk::WholeSize);
        }
        return mapped_;
    }

    void unmap() noexcept
    {
        if (mapped_) {
            memory_.unmapMemory();
            mapped_ = nullptr;
        }
    }

    void flush()
    {
        if (!(memory_properties_
                & vk::MemoryPropertyFlagBits::eHostCoherent)) {
            device_->flushMappedMemoryRanges(vk::MappedMemoryRange{
                .memory = *memory_,
                .offset = 0,
                .size = vk::WholeSize,
            });
        }
    }

    ~AllocatedBuffer()
    {
        unmap();
    }

    AllocatedBuffer(const AllocatedBuffer&) = delete;
    AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;
    AllocatedBuffer(AllocatedBuffer&&) = default;
    AllocatedBuffer& operator=(AllocatedBuffer&&) = default;

private:
    const vk::raii::Device* device_{};
    vk::DeviceSize size_{};
    vk::DeviceSize allocation_size_{};
    vk::MemoryPropertyFlags memory_properties_{};
    void* mapped_{};
    vk::raii::Buffer buffer_{nullptr};
    vk::raii::DeviceMemory memory_{nullptr};
};

class AllocatedImage final {
public:
    AllocatedImage() = default;

    AllocatedImage(
        const VulkanRuntime& runtime,
        const vk::Extent2D extent,
        const vk::Format format,
        const vk::ImageUsageFlags usage,
        const vk::ImageAspectFlags aspect)
        : format_(format),
          extent_(extent)
    {
        image_ = vk::raii::Image(
            runtime.device(),
            vk::ImageCreateInfo{
                .imageType = vk::ImageType::e2D,
                .format = format,
                .extent = {extent.width, extent.height, 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = usage,
                .sharingMode = vk::SharingMode::eExclusive,
                .initialLayout = vk::ImageLayout::eUndefined,
            });
        const auto requirements = image_.getMemoryRequirements();
        const auto properties = runtime.memory_properties();
        std::optional<std::uint32_t> memory_type;
        for (std::uint32_t index = 0;
             index < properties.memoryTypeCount;
             ++index) {
            if ((requirements.memoryTypeBits & (1U << index)) != 0
                && (properties.memoryTypes[index].propertyFlags
                    & vk::MemoryPropertyFlagBits::eDeviceLocal)) {
                memory_type = index;
                break;
            }
        }
        if (!memory_type) {
            throw std::runtime_error(
                "no device-local memory type for Vulkan image");
        }
        memory_ = vk::raii::DeviceMemory(
            runtime.device(),
            vk::MemoryAllocateInfo{
                .allocationSize = requirements.size,
                .memoryTypeIndex = *memory_type,
            });
        image_.bindMemory(*memory_, 0);
        view_ = vk::raii::ImageView(
            runtime.device(),
            vk::ImageViewCreateInfo{
                .image = *image_,
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .subresourceRange = {
                    .aspectMask = aspect,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            });
    }

    [[nodiscard]] vk::Image image() const noexcept { return *image_; }
    [[nodiscard]] vk::ImageView view() const noexcept { return *view_; }
    [[nodiscard]] vk::Format format() const noexcept { return format_; }

    AllocatedImage(const AllocatedImage&) = delete;
    AllocatedImage& operator=(const AllocatedImage&) = delete;
    AllocatedImage(AllocatedImage&&) = default;
    AllocatedImage& operator=(AllocatedImage&&) = default;

private:
    vk::Format format_{};
    vk::Extent2D extent_{};
    vk::raii::Image image_{nullptr};
    vk::raii::DeviceMemory memory_{nullptr};
    vk::raii::ImageView view_{nullptr};
};

vk::raii::ShaderModule make_shader_module(
    const vk::raii::Device& device,
    const std::span<const std::uint32_t> words)
{
    return vk::raii::ShaderModule(
        device,
        vk::ShaderModuleCreateInfo{
            .codeSize = words.size_bytes(),
            .pCode = words.data(),
        });
}

struct PairRange {
    vk::DeviceSize offset{};
    std::uint32_t pair_count{};
};

struct CandidateUpload {
    bool pair_mode{};
    std::vector<std::byte> pairs;
    std::vector<PackedStaticEntry> entries;
    std::vector<PairRange> pair_ranges;
};

CandidateUpload prepare_candidates(
    const MotionPairCandidates& candidates,
    const VulkanRuntime& runtime)
{
    CandidateUpload result;
    const auto limits = runtime.properties().limits;
    if (candidates.pairs) {
        result.pair_mode = true;
        const vk::DeviceSize range_alignment = std::max<vk::DeviceSize>(
            limits.minStorageBufferOffsetAlignment,
            alignof(TrianglePair));
        const std::size_t maximum_pairs_per_range = std::max<std::size_t>(
            1,
            static_cast<std::size_t>(
                limits.maxStorageBufferRange / sizeof(TrianglePair)));
        const auto& pairs = *candidates.pairs;
        for (std::size_t source_offset = 0;
             source_offset < pairs.size();) {
            const std::size_t count = std::min(
                maximum_pairs_per_range,
                pairs.size() - source_offset);
            const vk::DeviceSize destination_offset = align_up(
                result.pairs.size(), range_alignment);
            result.pairs.resize(
                static_cast<std::size_t>(destination_offset)
                    + count * sizeof(TrianglePair));
            std::memcpy(
                result.pairs.data() + destination_offset,
                pairs.data() + source_offset,
                count * sizeof(TrianglePair));
            result.pair_ranges.push_back({
                .offset = destination_offset,
                .pair_count = static_cast<std::uint32_t>(count),
            });
            source_offset += count;
        }
        if (result.pair_ranges.empty()) {
            result.pair_ranges.push_back({.offset = 0, .pair_count = 0});
        }
    } else {
        result.entries.reserve(candidates.voxels.size());
        for (std::size_t id = 0; id < candidates.voxels.size(); ++id) {
            const Voxel voxel = candidates.voxels[id];
            const PackedStaticEntry entry{.voxel = {
                gpu_voxel_coordinate(voxel.x),
                gpu_voxel_coordinate(voxel.y),
                gpu_voxel_coordinate(voxel.z),
                static_cast<std::int32_t>(id),
            }};
            result.entries.push_back(entry);
        }
        std::ranges::sort(
            result.entries,
            [](const PackedStaticEntry& left,
               const PackedStaticEntry& right) {
                return std::tie(
                    left.voxel[0], left.voxel[1], left.voxel[2])
                    < std::tie(
                        right.voxel[0], right.voxel[1], right.voxel[2]);
            });
    }
    return result;
}

class ComputeState final {
public:
    ComputeState(
        const VulkanRuntime& runtime,
        const std::span<const InitialTriangle> initial,
        const MotionPairCandidates& candidates)
        : runtime_(runtime),
          triangle_count_(initial.size()),
          candidate_upload_(prepare_candidates(candidates, runtime)),
          initial_buffer_(
              runtime,
              checked_size(
                  initial.size(),
                  sizeof(PackedInitialTriangle),
                  "initial triangle buffer"),
              vk::BufferUsageFlagBits::eStorageBuffer
                  | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal),
          runtime_buffer_(
              runtime,
              checked_size(
                  initial.size(),
                  sizeof(PackedRuntimeTriangle),
                  "runtime triangle buffer"),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal),
          pair_buffer_(
              runtime,
              std::max<vk::DeviceSize>(candidate_upload_.pairs.size(), 8),
              vk::BufferUsageFlagBits::eStorageBuffer
                  | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal),
          static_buffer_(
              runtime,
              std::max<vk::DeviceSize>(
                  candidate_upload_.entries.size()
                      * sizeof(PackedStaticEntry),
                  16),
              vk::BufferUsageFlagBits::eStorageBuffer
                  | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal),
          dummy_buffer_(
              runtime,
              16,
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal)
    {
        std::vector<PackedInitialTriangle> packed_initial;
        packed_initial.reserve(initial.size());
        for (const InitialTriangle& triangle : initial) {
            packed_initial.push_back(pack(triangle));
        }
        upload(
            std::as_bytes(std::span{packed_initial}),
            std::span<const std::byte>{candidate_upload_.pairs},
            std::as_bytes(std::span{candidate_upload_.entries}));
        create_descriptors();
        create_pipelines();
    }

    [[nodiscard]] vk::Buffer runtime_buffer() const noexcept
    {
        return runtime_buffer_.handle();
    }

    [[nodiscard]] vk::DeviceSize runtime_size() const noexcept
    {
        return runtime_buffer_.size();
    }

    [[nodiscard]] std::size_t triangle_count() const noexcept
    {
        return triangle_count_;
    }

    void record_rotation(
        const vk::raii::CommandBuffer& command,
        const float elapsed_seconds) const
    {
        command.bindPipeline(
            vk::PipelineBindPoint::eCompute, *rotation_pipeline_);
        command.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            *rotation_layout_,
            0,
            {*rotation_sets_->front()},
            {});
        command.pushConstants(
            *rotation_layout_,
            vk::ShaderStageFlagBits::eCompute,
            0,
            sizeof(elapsed_seconds),
            &elapsed_seconds);
        command.dispatch(group_count(triangle_count_), 1, 1);
    }

    void record_intersections(
        const vk::raii::CommandBuffer& command) const
    {
        command.bindPipeline(
            vk::PipelineBindPoint::eCompute, *intersection_pipeline_);
        const std::uint64_t maximum_invocations =
            static_cast<std::uint64_t>(
                runtime_.properties().limits.maxComputeWorkGroupCount[0])
            * compute_local_size;
        if (candidate_upload_.pair_mode) {
            for (std::size_t range_index = 0;
                 range_index < candidate_upload_.pair_ranges.size();
                 ++range_index) {
                const PairRange range =
                    candidate_upload_.pair_ranges[range_index];
                command.bindDescriptorSets(
                    vk::PipelineBindPoint::eCompute,
                    *intersection_layout_,
                    0,
                    {*(*intersection_sets_)[range_index]},
                    {});
                for (std::uint64_t offset = 0;
                     offset < range.pair_count;) {
                    const std::uint32_t batch = static_cast<std::uint32_t>(
                        std::min<std::uint64_t>(
                            range.pair_count - offset,
                            maximum_invocations));
                    const std::array<std::uint32_t, 2> push{
                        0U, static_cast<std::uint32_t>(offset)};
                    command.pushConstants(
                        *intersection_layout_,
                        vk::ShaderStageFlagBits::eCompute,
                        0,
                        sizeof(push),
                        push.data());
                    command.dispatch(group_count(batch), 1, 1);
                    offset += batch;
                }
            }
        } else {
            command.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                *intersection_layout_,
                0,
                {*intersection_sets_->front()},
                {});
            for (std::uint64_t offset = 0;
                 offset < candidate_upload_.entries.size();) {
                const std::uint32_t batch = static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(
                        candidate_upload_.entries.size() - offset,
                        maximum_invocations));
                const std::array<std::uint32_t, 2> push{
                    1U, static_cast<std::uint32_t>(offset)};
                command.pushConstants(
                    *intersection_layout_,
                    vk::ShaderStageFlagBits::eCompute,
                    0,
                    sizeof(push),
                    push.data());
                command.dispatch(group_count(batch), 1, 1);
                offset += batch;
            }
        }
    }

    void barrier_rotation_to_intersections(
        const vk::raii::CommandBuffer& command) const
    {
        const vk::BufferMemoryBarrier2 barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderRead
                | vk::AccessFlagBits2::eShaderWrite,
            .buffer = runtime_buffer_.handle(),
            .offset = 0,
            .size = runtime_buffer_.size(),
        };
        command.pipelineBarrier2(vk::DependencyInfo{
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &barrier,
        });
    }

    void barrier_intersections_to(
        const vk::raii::CommandBuffer& command,
        const vk::PipelineStageFlags2 destination_stage,
        const vk::AccessFlags2 destination_access) const
    {
        const vk::BufferMemoryBarrier2 barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
            .dstStageMask = destination_stage,
            .dstAccessMask = destination_access,
            .buffer = runtime_buffer_.handle(),
            .offset = 0,
            .size = runtime_buffer_.size(),
        };
        command.pipelineBarrier2(vk::DependencyInfo{
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &barrier,
        });
    }

private:
    static std::uint32_t group_count(const std::size_t invocations)
    {
        return static_cast<std::uint32_t>(
            (invocations + compute_local_size - 1) / compute_local_size);
    }

    void upload(
        const std::span<const std::byte> initial,
        const std::span<const std::byte> pairs,
        const std::span<const std::byte> entries)
    {
        const vk::DeviceSize pair_offset = align_up(initial.size(), 16);
        const vk::DeviceSize entry_offset = align_up(
            pair_offset + pairs.size(), 16);
        const vk::DeviceSize staging_size = std::max<vk::DeviceSize>(
            entry_offset + entries.size(), 1);
        AllocatedBuffer staging{
            runtime_,
            staging_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible,
            vk::MemoryPropertyFlagBits::eHostCoherent
                | vk::MemoryPropertyFlagBits::eHostCached};
        auto* const mapped = static_cast<std::byte*>(staging.map());
        std::memcpy(mapped, initial.data(), initial.size());
        if (!pairs.empty()) {
            std::memcpy(mapped + pair_offset, pairs.data(), pairs.size());
        }
        if (!entries.empty()) {
            std::memcpy(mapped + entry_offset, entries.data(), entries.size());
        }
        staging.flush();

        vk::raii::CommandBuffers commands(
            runtime_.device(),
            vk::CommandBufferAllocateInfo{
                .commandPool = *runtime_.command_pool(),
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            });
        auto& command = commands.front();
        command.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        command.copyBuffer(
            staging.handle(),
            initial_buffer_.handle(),
            vk::BufferCopy{.srcOffset = 0, .size = initial.size()});
        if (!pairs.empty()) {
            command.copyBuffer(
                staging.handle(),
                pair_buffer_.handle(),
                vk::BufferCopy{
                    .srcOffset = pair_offset,
                    .size = pairs.size(),
                });
        }
        if (!entries.empty()) {
            command.copyBuffer(
                staging.handle(),
                static_buffer_.handle(),
                vk::BufferCopy{
                    .srcOffset = entry_offset,
                    .size = entries.size(),
                });
        }
        command.end();
        vk::raii::Fence fence(
            runtime_.device(), vk::FenceCreateInfo{});
        const vk::CommandBuffer raw_command = *command;
        runtime_.compute_queue().submit(
            vk::SubmitInfo{
                .commandBufferCount = 1,
                .pCommandBuffers = &raw_command,
            },
            *fence);
        const auto result = runtime_.device().waitForFences(
            {*fence}, vk::True, std::numeric_limits<std::uint64_t>::max());
        if (result != vk::Result::eSuccess) {
            throw std::runtime_error(
                "Vulkan initialization upload did not complete");
        }
    }

    void create_descriptors()
    {
        const std::array<vk::DescriptorSetLayoutBinding, 2>
            rotation_bindings{{
                {
                    .binding = 0,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                },
                {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                },
            }};
        rotation_set_layout_ = vk::raii::DescriptorSetLayout(
            runtime_.device(),
            vk::DescriptorSetLayoutCreateInfo{
                .bindingCount =
                    static_cast<std::uint32_t>(rotation_bindings.size()),
                .pBindings = rotation_bindings.data(),
            });

        const std::array<vk::DescriptorSetLayoutBinding, 3>
            intersection_bindings{{
                {
                    .binding = 0,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                },
                {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                },
                {
                    .binding = 2,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
                },
            }};
        intersection_set_layout_ = vk::raii::DescriptorSetLayout(
            runtime_.device(),
            vk::DescriptorSetLayoutCreateInfo{
                .bindingCount =
                    static_cast<std::uint32_t>(intersection_bindings.size()),
                .pBindings = intersection_bindings.data(),
            });

        const std::uint32_t intersection_count = static_cast<std::uint32_t>(
            candidate_upload_.pair_mode
                ? candidate_upload_.pair_ranges.size() : 1);
        const std::array<vk::DescriptorPoolSize, 1> pool_sizes{{
            {
                .type = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 2 + 3 * intersection_count,
            },
        }};
        descriptor_pool_ = vk::raii::DescriptorPool(
            runtime_.device(),
            vk::DescriptorPoolCreateInfo{
                .maxSets = 1 + intersection_count,
                .poolSizeCount =
                    static_cast<std::uint32_t>(pool_sizes.size()),
                .pPoolSizes = pool_sizes.data(),
            });

        const std::array<vk::DescriptorSetLayout, 1> rotation_layouts{
            *rotation_set_layout_};
        rotation_sets_ = vk::raii::DescriptorSets(
            runtime_.device(),
            vk::DescriptorSetAllocateInfo{
                .descriptorPool = *descriptor_pool_,
                .descriptorSetCount = 1,
                .pSetLayouts = rotation_layouts.data(),
            });
        const vk::DescriptorBufferInfo initial_info{
            .buffer = initial_buffer_.handle(),
            .offset = 0,
            .range = initial_buffer_.size(),
        };
        const vk::DescriptorBufferInfo runtime_info{
            .buffer = runtime_buffer_.handle(),
            .offset = 0,
            .range = runtime_buffer_.size(),
        };
        const std::array<vk::WriteDescriptorSet, 2> rotation_writes{{
            {
                .dstSet = *rotation_sets_->front(),
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &initial_info,
            },
            {
                .dstSet = *rotation_sets_->front(),
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &runtime_info,
            },
        }};
        runtime_.device().updateDescriptorSets(rotation_writes, {});

        std::vector<vk::DescriptorSetLayout> layouts(
            intersection_count, *intersection_set_layout_);
        intersection_sets_ = vk::raii::DescriptorSets(
            runtime_.device(),
            vk::DescriptorSetAllocateInfo{
                .descriptorPool = *descriptor_pool_,
                .descriptorSetCount = intersection_count,
                .pSetLayouts = layouts.data(),
            });
        const vk::DescriptorBufferInfo dummy_pair{
            .buffer = dummy_buffer_.handle(), .offset = 0, .range = 8};
        const vk::DescriptorBufferInfo dummy_entry{
            .buffer = dummy_buffer_.handle(), .offset = 0, .range = 16};
        for (std::uint32_t index = 0;
             index < intersection_count;
             ++index) {
            const PairRange range = candidate_upload_.pair_mode
                ? candidate_upload_.pair_ranges[index]
                : PairRange{};
            const vk::DescriptorBufferInfo pair_info =
                candidate_upload_.pair_mode && range.pair_count != 0
                ? vk::DescriptorBufferInfo{
                    .buffer = pair_buffer_.handle(),
                    .offset = range.offset,
                    .range = static_cast<vk::DeviceSize>(
                        range.pair_count) * sizeof(TrianglePair),
                }
                : dummy_pair;
            const vk::DescriptorBufferInfo entry_info =
                !candidate_upload_.pair_mode
                    && !candidate_upload_.entries.empty()
                ? vk::DescriptorBufferInfo{
                    .buffer = static_buffer_.handle(),
                    .offset = 0,
                    .range = static_cast<vk::DeviceSize>(
                        candidate_upload_.entries.size())
                        * sizeof(PackedStaticEntry),
                }
                : dummy_entry;
            const std::array<vk::WriteDescriptorSet, 3> writes{{
                {
                    .dstSet = *(*intersection_sets_)[index],
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &runtime_info,
                },
                {
                    .dstSet = *(*intersection_sets_)[index],
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &pair_info,
                },
                {
                    .dstSet = *(*intersection_sets_)[index],
                    .dstBinding = 2,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &entry_info,
                },
            }};
            runtime_.device().updateDescriptorSets(writes, {});
        }
    }

    void create_pipelines()
    {
        const vk::PushConstantRange rotation_push{
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .offset = 0,
            .size = sizeof(float),
        };
        const vk::DescriptorSetLayout rotation_set_layout =
            *rotation_set_layout_;
        rotation_layout_ = vk::raii::PipelineLayout(
            runtime_.device(),
            vk::PipelineLayoutCreateInfo{
                .setLayoutCount = 1,
                .pSetLayouts = &rotation_set_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &rotation_push,
            });
        const vk::PushConstantRange intersection_push{
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .offset = 0,
            .size = sizeof(std::uint32_t) * 2,
        };
        const vk::DescriptorSetLayout intersection_set_layout =
            *intersection_set_layout_;
        intersection_layout_ = vk::raii::PipelineLayout(
            runtime_.device(),
            vk::PipelineLayoutCreateInfo{
                .setLayoutCount = 1,
                .pSetLayouts = &intersection_set_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &intersection_push,
            });

        auto rotation_shader = make_shader_module(
            runtime_.device(), shaders::update_vertices_comp_spv);
        auto intersection_shader = make_shader_module(
            runtime_.device(), shaders::intersections_comp_spv);
        rotation_pipeline_ = vk::raii::Pipeline(
            runtime_.device(),
            nullptr,
            vk::ComputePipelineCreateInfo{
                .stage = {
                    .stage = vk::ShaderStageFlagBits::eCompute,
                    .module = *rotation_shader,
                    .pName = "main",
                },
                .layout = *rotation_layout_,
            });
        intersection_pipeline_ = vk::raii::Pipeline(
            runtime_.device(),
            nullptr,
            vk::ComputePipelineCreateInfo{
                .stage = {
                    .stage = vk::ShaderStageFlagBits::eCompute,
                    .module = *intersection_shader,
                    .pName = "main",
                },
                .layout = *intersection_layout_,
            });
    }

    const VulkanRuntime& runtime_;
    std::size_t triangle_count_{};
    CandidateUpload candidate_upload_;
    AllocatedBuffer initial_buffer_;
    AllocatedBuffer runtime_buffer_;
    AllocatedBuffer pair_buffer_;
    AllocatedBuffer static_buffer_;
    AllocatedBuffer dummy_buffer_;
    vk::raii::DescriptorSetLayout rotation_set_layout_{nullptr};
    vk::raii::DescriptorSetLayout intersection_set_layout_{nullptr};
    vk::raii::DescriptorPool descriptor_pool_{nullptr};
    std::optional<vk::raii::DescriptorSets> rotation_sets_;
    std::optional<vk::raii::DescriptorSets> intersection_sets_;
    vk::raii::PipelineLayout rotation_layout_{nullptr};
    vk::raii::PipelineLayout intersection_layout_{nullptr};
    vk::raii::Pipeline rotation_pipeline_{nullptr};
    vk::raii::Pipeline intersection_pipeline_{nullptr};
};

class RendererState final {
public:
    enum class AcquireResult {
        acquired,
        skipped,
    };

    RendererState(
        const VulkanRuntime& runtime,
        const ComputeState& compute,
        const std::span<const Aabb> motion_bounds)
        : runtime_(runtime),
          compute_(compute)
    {
        initialize_camera(motion_bounds);
        create_descriptor();
    }

    [[nodiscard]] AcquireResult acquire(
        const std::uint32_t width,
        const std::uint32_t height,
        const vk::Semaphore image_available)
    {
        if (width == 0 || height == 0) {
            return AcquireResult::skipped;
        }
        if (*swapchain_ == vk::SwapchainKHR{} || dirty_
            || requested_extent_.width != width
            || requested_extent_.height != height) {
            recreate(width, height);
        }

        std::uint32_t image_index{};
        const VkResult result = vkAcquireNextImageKHR(
            static_cast<VkDevice>(*runtime_.device()),
            static_cast<VkSwapchainKHR>(*swapchain_),
            std::numeric_limits<std::uint64_t>::max(),
            static_cast<VkSemaphore>(image_available),
            VK_NULL_HANDLE,
            &image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            dirty_ = true;
            return AcquireResult::skipped;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error(
                "failed to acquire a Vulkan swapchain image");
        }
        image_index_ = image_index;
        acquired_suboptimal_ = result == VK_SUBOPTIMAL_KHR;
        return AcquireResult::acquired;
    }

    void record(
        const vk::raii::CommandBuffer& command,
        const FrameInput& input)
    {
        update_camera(input);

        const vk::ImageMemoryBarrier2 color_to_attachment{
            .srcStageMask = vk::PipelineStageFlagBits2::eNone,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask =
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask =
                vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .image = images_[image_index_],
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        const vk::ImageMemoryBarrier2 depth_to_attachment{
            .srcStageMask = vk::PipelineStageFlagBits2::eNone,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask =
                vk::PipelineStageFlagBits2::eEarlyFragmentTests
                | vk::PipelineStageFlagBits2::eLateFragmentTests,
            .dstAccessMask =
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout =
                vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .image = depth_.image(),
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eDepth,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        const std::array barriers{color_to_attachment, depth_to_attachment};
        command.pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount =
                static_cast<std::uint32_t>(barriers.size()),
            .pImageMemoryBarriers = barriers.data(),
        });

        vk::ClearValue color_clear;
        color_clear.color.float32[0] = 0.04F;
        color_clear.color.float32[1] = 0.05F;
        color_clear.color.float32[2] = 0.08F;
        color_clear.color.float32[3] = 1.0F;
        vk::ClearValue depth_clear;
        depth_clear.depthStencil.depth = 1.0F;
        depth_clear.depthStencil.stencil = 0;
        const vk::RenderingAttachmentInfo color_attachment{
            .imageView = *views_[image_index_],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = color_clear,
        };
        const vk::RenderingAttachmentInfo depth_attachment{
            .imageView = depth_.view(),
            .imageLayout =
                vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue = depth_clear,
        };
        command.beginRendering(vk::RenderingInfo{
            .renderArea = {.offset = {0, 0}, .extent = extent_},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment,
            .pDepthAttachment = &depth_attachment,
        });
        command.bindPipeline(
            vk::PipelineBindPoint::eGraphics, *pipeline_);
        command.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *pipeline_layout_,
            0,
            {*descriptor_sets_->front()},
            {});
        const Matrix4 matrix = view_projection();
        command.pushConstants(
            *pipeline_layout_,
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(matrix),
            matrix.data());
        command.setViewport(
            0,
            vk::Viewport{
                .x = 0.0F,
                .y = 0.0F,
                .width = static_cast<float>(extent_.width),
                .height = static_cast<float>(extent_.height),
                .minDepth = 0.0F,
                .maxDepth = 1.0F,
            });
        command.setScissor(
            0,
            vk::Rect2D{.offset = {0, 0}, .extent = extent_});
        command.draw(
            static_cast<std::uint32_t>(compute_.triangle_count() * 3),
            1,
            0,
            0);
        command.endRendering();

        const vk::ImageMemoryBarrier2 attachment_to_present{
            .srcStageMask =
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask =
                vk::AccessFlagBits2::eColorAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eNone,
            .dstAccessMask = vk::AccessFlagBits2::eNone,
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .image = images_[image_index_],
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        command.pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &attachment_to_present,
        });
    }

    [[nodiscard]] bool present(const vk::Semaphore render_finished)
    {
        const VkSemaphore semaphore =
            static_cast<VkSemaphore>(render_finished);
        const VkSwapchainKHR swapchain =
            static_cast<VkSwapchainKHR>(*swapchain_);
        const VkPresentInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &semaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &image_index_,
        };
        const VkResult result = vkQueuePresentKHR(
            static_cast<VkQueue>(*runtime_.present_queue()), &info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            dirty_ = true;
            return false;
        }
        if (result == VK_SUBOPTIMAL_KHR || acquired_suboptimal_) {
            dirty_ = true;
            return true;
        }
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present a Vulkan frame");
        }
        return true;
    }

private:
    void create_descriptor()
    {
        const vk::DescriptorSetLayoutBinding binding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
        };
        descriptor_layout_ = vk::raii::DescriptorSetLayout(
            runtime_.device(),
            vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = 1,
                .pBindings = &binding,
            });
        const vk::DescriptorPoolSize pool_size{
            .type = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
        };
        descriptor_pool_ = vk::raii::DescriptorPool(
            runtime_.device(),
            vk::DescriptorPoolCreateInfo{
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &pool_size,
            });
        const vk::DescriptorSetLayout layout = *descriptor_layout_;
        descriptor_sets_ = vk::raii::DescriptorSets(
            runtime_.device(),
            vk::DescriptorSetAllocateInfo{
                .descriptorPool = *descriptor_pool_,
                .descriptorSetCount = 1,
                .pSetLayouts = &layout,
            });
        const vk::DescriptorBufferInfo buffer_info{
            .buffer = compute_.runtime_buffer(),
            .offset = 0,
            .range = compute_.runtime_size(),
        };
        runtime_.device().updateDescriptorSets(
            vk::WriteDescriptorSet{
                .dstSet = *descriptor_sets_->front(),
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &buffer_info,
            },
            {});
    }

    void recreate(
        const std::uint32_t requested_width,
        const std::uint32_t requested_height)
    {
        // The frame fence has already completed graphics/compute work.
        // Only presentation can still reference the old swapchain.
        runtime_.present_queue().waitIdle();
        const auto capabilities =
            runtime_.physical().getSurfaceCapabilitiesKHR(
                *runtime_.surface());
        requested_extent_ = vk::Extent2D{
            .width = requested_width, .height = requested_height};
        if (capabilities.currentExtent.width
            != std::numeric_limits<std::uint32_t>::max()) {
            extent_ = capabilities.currentExtent;
        } else {
            extent_ = vk::Extent2D{
                .width = std::clamp(
                    requested_width,
                    capabilities.minImageExtent.width,
                    capabilities.maxImageExtent.width),
                .height = std::clamp(
                    requested_height,
                    capabilities.minImageExtent.height,
                    capabilities.maxImageExtent.height),
            };
        }

        const auto formats = runtime_.physical().getSurfaceFormatsKHR(
            *runtime_.surface());
        auto selected_format = formats.front();
        for (const auto& candidate : formats) {
            const bool srgb_format =
                candidate.format == vk::Format::eB8G8R8A8Srgb
                || candidate.format == vk::Format::eR8G8B8A8Srgb;
            if (srgb_format
                && candidate.colorSpace
                    == vk::ColorSpaceKHR::eSrgbNonlinear) {
                selected_format = candidate;
                break;
            }
        }
        color_format_ = selected_format.format;
        std::uint32_t image_count = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount != 0) {
            image_count = std::min(image_count, capabilities.maxImageCount);
        }
        const std::array queue_families{
            runtime_.compute_family(), runtime_.present_family()};
        const bool concurrent =
            runtime_.compute_family() != runtime_.present_family();
        vk::raii::SwapchainKHR new_swapchain(
            runtime_.device(),
            vk::SwapchainCreateInfoKHR{
                .surface = *runtime_.surface(),
                .minImageCount = image_count,
                .imageFormat = selected_format.format,
                .imageColorSpace = selected_format.colorSpace,
                .imageExtent = extent_,
                .imageArrayLayers = 1,
                .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
                .imageSharingMode = concurrent
                    ? vk::SharingMode::eConcurrent
                    : vk::SharingMode::eExclusive,
                .queueFamilyIndexCount = concurrent ? 2U : 0U,
                .pQueueFamilyIndices =
                    concurrent ? queue_families.data() : nullptr,
                .preTransform = capabilities.currentTransform,
                .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                .presentMode = vk::PresentModeKHR::eFifo,
                .clipped = vk::True,
                .oldSwapchain = *swapchain_,
            });

        views_.clear();
        images_.clear();
        swapchain_ = std::move(new_swapchain);
        images_ = swapchain_.getImages();
        views_.reserve(images_.size());
        for (const vk::Image image : images_) {
            views_.emplace_back(
                runtime_.device(),
                vk::ImageViewCreateInfo{
                    .image = image,
                    .viewType = vk::ImageViewType::e2D,
                    .format = color_format_,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                });
        }
        depth_ = AllocatedImage(
            runtime_,
            extent_,
            choose_depth_format(),
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::ImageAspectFlagBits::eDepth);
        create_pipeline();
        dirty_ = false;
    }

    [[nodiscard]] vk::Format choose_depth_format() const
    {
        for (const vk::Format format : {
                 vk::Format::eD32Sfloat,
                 vk::Format::eD24UnormS8Uint,
                 vk::Format::eD16Unorm}) {
            const auto properties =
                runtime_.physical().getFormatProperties(format);
            if (properties.optimalTilingFeatures
                & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
                return format;
            }
        }
        throw std::runtime_error(
            "Vulkan device has no supported depth format");
    }

    void create_pipeline()
    {
        const vk::PushConstantRange push_constant{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(Matrix4),
        };
        const vk::DescriptorSetLayout descriptor_layout =
            *descriptor_layout_;
        pipeline_layout_ = vk::raii::PipelineLayout(
            runtime_.device(),
            vk::PipelineLayoutCreateInfo{
                .setLayoutCount = 1,
                .pSetLayouts = &descriptor_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_constant,
            });
        auto vertex_shader = make_shader_module(
            runtime_.device(), shaders::triangle_vert_spv);
        auto fragment_shader = make_shader_module(
            runtime_.device(), shaders::triangle_frag_spv);
        const std::array stages{
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = *vertex_shader,
                .pName = "main",
            },
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = *fragment_shader,
                .pName = "main",
            },
        };
        const vk::PipelineVertexInputStateCreateInfo vertex_input{};
        const vk::PipelineInputAssemblyStateCreateInfo input_assembly{
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = vk::False,
        };
        const vk::PipelineViewportStateCreateInfo viewport_state{
            .viewportCount = 1,
            .scissorCount = 1,
        };
        const vk::PipelineRasterizationStateCreateInfo rasterization{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth = 1.0F,
        };
        const vk::PipelineMultisampleStateCreateInfo multisample{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
        };
        const vk::PipelineDepthStencilStateCreateInfo depth_stencil{
            .depthTestEnable = vk::True,
            .depthWriteEnable = vk::True,
            .depthCompareOp = vk::CompareOp::eLess,
            .depthBoundsTestEnable = vk::False,
            .stencilTestEnable = vk::False,
        };
        const vk::PipelineColorBlendAttachmentState blend_attachment{
            .blendEnable = vk::False,
            .colorWriteMask =
                vk::ColorComponentFlagBits::eR
                | vk::ColorComponentFlagBits::eG
                | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA,
        };
        const vk::PipelineColorBlendStateCreateInfo color_blend{
            .logicOpEnable = vk::False,
            .attachmentCount = 1,
            .pAttachments = &blend_attachment,
        };
        const std::array dynamic_states{
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor};
        const vk::PipelineDynamicStateCreateInfo dynamic_state{
            .dynamicStateCount =
                static_cast<std::uint32_t>(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data(),
        };
        const vk::PipelineRenderingCreateInfo rendering_info{
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &color_format_,
            .depthAttachmentFormat = depth_.format(),
        };
        pipeline_ = vk::raii::Pipeline(
            runtime_.device(),
            nullptr,
            vk::GraphicsPipelineCreateInfo{
                .pNext = &rendering_info,
                .stageCount = static_cast<std::uint32_t>(stages.size()),
                .pStages = stages.data(),
                .pVertexInputState = &vertex_input,
                .pInputAssemblyState = &input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &rasterization,
                .pMultisampleState = &multisample,
                .pDepthStencilState = &depth_stencil,
                .pColorBlendState = &color_blend,
                .pDynamicState = &dynamic_state,
                .layout = *pipeline_layout_,
            });
    }

    void initialize_camera(const std::span<const Aabb> bounds)
    {
        Vec3 minimum{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
        Vec3 maximum{
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()};
        for (const Aabb& value : bounds) {
            minimum.x = std::min(minimum.x, value.minimum.x);
            minimum.y = std::min(minimum.y, value.minimum.y);
            minimum.z = std::min(minimum.z, value.minimum.z);
            maximum.x = std::max(maximum.x, value.maximum.x);
            maximum.y = std::max(maximum.y, value.maximum.y);
            maximum.z = std::max(maximum.z, value.maximum.z);
        }
        scene_center_ = {
            std::midpoint(minimum.x, maximum.x),
            std::midpoint(minimum.y, maximum.y),
            std::midpoint(minimum.z, maximum.z)};
        const Vec3 half_extent{
            (maximum.x - minimum.x) * 0.5F,
            (maximum.y - minimum.y) * 0.5F,
            (maximum.z - minimum.z) * 0.5F};
        scene_radius_ = std::max(length(half_extent), 0.5F);
        const float distance =
            scene_radius_ / std::tan(field_of_view * 0.5F)
            + scene_radius_;
        camera_position_ = {
            scene_center_.x,
            scene_center_.y,
            scene_center_.z + distance};
        movement_speed_ = scene_radius_ * 1.5F;
    }

    [[nodiscard]] Vec3 direction() const noexcept
    {
        const float pitch_cosine = std::cos(pitch_);
        return normalize({
            std::cos(yaw_) * pitch_cosine,
            std::sin(pitch_),
            std::sin(yaw_) * pitch_cosine,
        });
    }

    void update_camera(const FrameInput& input) noexcept
    {
        yaw_ += input.mouse_x_offset * mouse_sensitivity;
        pitch_ = std::clamp(
            pitch_ + input.mouse_y_offset * mouse_sensitivity,
            -maximum_pitch,
            maximum_pitch);
        const Vec3 forward = direction();
        const Vec3 right = normalize(cross(
            forward, Vec3{0.0F, 1.0F, 0.0F}));
        if (input.up) {
            camera_position_ = add(
                camera_position_,
                scale(forward, movement_speed_ * input.delta_seconds));
        }
        if (input.down) {
            camera_position_ = add(
                camera_position_,
                scale(forward, -movement_speed_ * input.delta_seconds));
        }
        if (input.left) {
            camera_position_ = add(
                camera_position_,
                scale(right, -movement_speed_ * input.delta_seconds));
        }
        if (input.right) {
            camera_position_ = add(
                camera_position_,
                scale(right, movement_speed_ * input.delta_seconds));
        }
    }

    [[nodiscard]] Matrix4 view_projection() const noexcept
    {
        const float distance = length({
            scene_center_.x - camera_position_.x,
            scene_center_.y - camera_position_.y,
            scene_center_.z - camera_position_.z});
        const float padded_radius = scene_radius_ * 1.1F;
        const float near_plane = std::max(
            std::max(scene_radius_ * 0.001F, 0.0001F),
            distance - padded_radius);
        const float far_plane = std::max(
            distance + padded_radius,
            std::nextafter(
                near_plane, std::numeric_limits<float>::max()));
        return multiply(
            perspective(
                static_cast<float>(extent_.width)
                    / static_cast<float>(extent_.height),
                near_plane,
                far_plane),
            look_at(camera_position_, direction()));
    }

    const VulkanRuntime& runtime_;
    const ComputeState& compute_;
    bool dirty_ = true;
    bool acquired_suboptimal_{};
    std::uint32_t image_index_{};
    vk::Extent2D requested_extent_{};
    vk::Extent2D extent_{};
    vk::Format color_format_{};
    vk::raii::SwapchainKHR swapchain_{nullptr};
    std::vector<vk::Image> images_;
    std::vector<vk::raii::ImageView> views_;
    AllocatedImage depth_;
    vk::raii::DescriptorSetLayout descriptor_layout_{nullptr};
    vk::raii::DescriptorPool descriptor_pool_{nullptr};
    std::optional<vk::raii::DescriptorSets> descriptor_sets_;
    vk::raii::PipelineLayout pipeline_layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
    Vec3 camera_position_{};
    Vec3 scene_center_{};
    float scene_radius_{};
    float movement_speed_{};
    float yaw_ = initial_yaw;
    float pitch_{};
};

}

class GpuPipeline::Impl final {
public:
    Impl(
        WindowContext& window,
        const Configuration& configuration,
        const GpuPipelineOptions options)
    {
        validate_configuration(configuration);
        if (options.maximum_cached_pair_count
            > maximum_cached_triangle_pair_count) {
            throw std::invalid_argument(
                "intersection pair budget exceeds the supported maximum");
        }

        motion_bounds_.resize(configuration.triangles.size());
        calculate_motion_bounds(
            configuration.triangles, motion_bounds_);
        MotionPairCandidates candidates = make_motion_pair_candidates(
            motion_bounds_, options.maximum_cached_pair_count);

        const vk::DeviceSize initial_size = checked_size(
            configuration.triangles.size(),
            sizeof(PackedInitialTriangle),
            "initial triangle buffer");
        const vk::DeviceSize runtime_size = checked_size(
            configuration.triangles.size(),
            sizeof(PackedRuntimeTriangle),
            "runtime triangle buffer");
        runtime_ = std::make_unique<VulkanRuntime>(
            window, initial_size, runtime_size);
        compute_ = std::make_unique<ComputeState>(
            *runtime_, configuration.triangles, candidates);

        command_buffers_ = vk::raii::CommandBuffers(
            runtime_->device(),
            vk::CommandBufferAllocateInfo{
                .commandPool = *runtime_->command_pool(),
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            });
        fence_ = vk::raii::Fence(
            runtime_->device(),
            vk::FenceCreateInfo{
                .flags = vk::FenceCreateFlagBits::eSignaled,
            });

        image_available_ =
            vk::raii::Semaphore(runtime_->device(), vk::SemaphoreCreateInfo{});
        render_finished_ =
            vk::raii::Semaphore(runtime_->device(), vk::SemaphoreCreateInfo{});
        renderer_ = std::make_unique<RendererState>(
            *runtime_, *compute_, motion_bounds_);
    }

    ~Impl()
    {
        if (runtime_) {
            runtime_->device().waitIdle();
        }
    }

    [[nodiscard]] bool render_frame(
        const std::chrono::duration<float> elapsed,
        const FrameInput& input)
    {
        check_elapsed(elapsed);
        if (input.framebuffer_width == 0
            || input.framebuffer_height == 0) {
            return false;
        }
        wait_for_frame();
        const auto acquire = renderer_->acquire(
            input.framebuffer_width,
            input.framebuffer_height,
            *image_available_);
        if (acquire == RendererState::AcquireResult::skipped) {
            return false;
        }

        runtime_->device().resetFences({*fence_});
        auto& command = command_buffers_->front();
        command.reset();
        command.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        compute_->record_rotation(command, elapsed.count());
        compute_->barrier_rotation_to_intersections(command);
        compute_->record_intersections(command);
        compute_->barrier_intersections_to(
            command,
            vk::PipelineStageFlagBits2::eVertexShader,
            vk::AccessFlagBits2::eShaderRead);
        renderer_->record(command, input);
        command.end();
        submit(command, *image_available_, *render_finished_);
        return renderer_->present(*render_finished_);
    }

private:
    void wait_for_frame() const
    {
        const vk::Result result = runtime_->device().waitForFences(
            {*fence_},
            vk::True,
            std::numeric_limits<std::uint64_t>::max());
        if (result != vk::Result::eSuccess) {
            throw std::runtime_error("Vulkan frame fence wait failed");
        }
    }

    void submit(
        const vk::raii::CommandBuffer& command,
        const vk::Semaphore wait,
        const vk::Semaphore signal)
    {
        const vk::CommandBufferSubmitInfo command_info{
            .commandBuffer = *command,
        };
        const vk::SemaphoreSubmitInfo wait_info{
            .semaphore = wait,
            .stageMask =
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        };
        const vk::SemaphoreSubmitInfo signal_info{
            .semaphore = signal,
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        };
        runtime_->compute_queue().submit2(
            vk::SubmitInfo2{
                .waitSemaphoreInfoCount = wait ? 1U : 0U,
                .pWaitSemaphoreInfos = wait ? &wait_info : nullptr,
                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = &command_info,
                .signalSemaphoreInfoCount = signal ? 1U : 0U,
                .pSignalSemaphoreInfos = signal ? &signal_info : nullptr,
            },
            *fence_);
    }

    std::vector<Aabb> motion_bounds_;
    std::unique_ptr<VulkanRuntime> runtime_;
    std::unique_ptr<ComputeState> compute_;
    std::unique_ptr<RendererState> renderer_;
    std::optional<vk::raii::CommandBuffers> command_buffers_;
    vk::raii::Fence fence_{nullptr};
    vk::raii::Semaphore image_available_{nullptr};
    vk::raii::Semaphore render_finished_{nullptr};
};

GpuPipeline::GpuPipeline(
    WindowContext& window,
    const Configuration& configuration,
    const GpuPipelineOptions options)
    : impl_(std::make_unique<Impl>(window, configuration, options))
{
}

GpuPipeline::~GpuPipeline() = default;

bool GpuPipeline::render_frame(
    const std::chrono::duration<float> elapsed,
    const FrameInput& input)
{
    return impl_->render_frame(elapsed, input);
}

PackedInitialTriangle pack(const InitialTriangle& triangle) noexcept
{
    return {
        .a = pack_point(triangle.a),
        .b = pack_point(triangle.b),
        .c = pack_point(triangle.c),
        .axis_origin = pack_point(triangle.rotation_axis_origin),
        .axis_direction_and_speed = pack_point(
            triangle.rotation_axis_direction,
            triangle.angular_velocity_radians_per_second),
    };
}

std::int32_t gpu_voxel_coordinate(const std::int64_t value) noexcept
{
    return static_cast<std::int32_t>(std::clamp(
        value,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min())
            + 1,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())
            - 1));
}

vk::DeviceSize align_up(
    const vk::DeviceSize value,
    const vk::DeviceSize alignment) noexcept
{
    return alignment == 0
        ? value
        : (value + alignment - 1) / alignment * alignment;
}

vk::DeviceSize checked_size(
    const std::size_t count,
    const std::size_t element_size,
    const char* const label)
{
    if (count > std::numeric_limits<std::size_t>::max() / element_size) {
        throw std::length_error(std::string(label) + " is too large");
    }
    return static_cast<vk::DeviceSize>(
        std::max<std::size_t>(count * element_size, 1));
}

void check_elapsed(const std::chrono::duration<float> elapsed)
{
    if (!std::isfinite(elapsed.count()) || elapsed.count() < 0.0F) {
        throw std::invalid_argument(
            "elapsed simulation time must be finite and non-negative");
    }
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
    const vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT,
    const vk::DebugUtilsMessengerCallbackDataEXT* const data,
    void*) noexcept
{
    if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
        && data != nullptr && data->pMessage != nullptr) {
        std::fputs("Vulkan validation: ", stderr);
        std::fputs(data->pMessage, stderr);
        std::fputc('\n', stderr);
    }
    return vk::False;
}

Vec3 add(const Vec3 a, const Vec3 b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 scale(const Vec3 value, const float factor) noexcept
{
    return {value.x * factor, value.y * factor, value.z * factor};
}

float dot(const Vec3 a, const Vec3 b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3 a, const Vec3 b) noexcept
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float length(const Vec3 value) noexcept
{
    return std::sqrt(dot(value, value));
}

Vec3 normalize(const Vec3 value) noexcept
{
    return scale(value, 1.0F / length(value));
}

Matrix4 multiply(const Matrix4& left, const Matrix4& right) noexcept
{
    Matrix4 result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t index = 0; index < 4; ++index) {
                result[column * 4 + row] += left[index * 4 + row]
                    * right[column * 4 + index];
            }
        }
    }
    return result;
}

Matrix4 perspective(
    const float aspect,
    const float near_plane,
    const float far_plane) noexcept
{
    const float focal = 1.0F / std::tan(field_of_view * 0.5F);
    return {
        focal / aspect, 0.0F, 0.0F, 0.0F,
        0.0F, -focal, 0.0F, 0.0F,
        0.0F, 0.0F, far_plane / (near_plane - far_plane), -1.0F,
        0.0F, 0.0F,
        (far_plane * near_plane) / (near_plane - far_plane), 0.0F,
    };
}

Matrix4 look_at(const Vec3 position, const Vec3 direction) noexcept
{
    constexpr Vec3 world_up{0.0F, 1.0F, 0.0F};
    const Vec3 forward = normalize(direction);
    const Vec3 right = normalize(cross(forward, world_up));
    const Vec3 up = cross(right, forward);
    return {
        right.x, up.x, -forward.x, 0.0F,
        right.y, up.y, -forward.y, 0.0F,
        right.z, up.z, -forward.z, 0.0F,
        -dot(right, position), -dot(up, position), dot(forward, position), 1.0F,
    };
}

}
