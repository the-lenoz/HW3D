module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

export module hw3d.gpu_triangles;

import hw3d.configuration;
import hw3d.geometry;

export namespace hw3d {

class GpuTriangleBuffer final {
public:
    explicit GpuTriangleBuffer(std::size_t triangle_count);
    ~GpuTriangleBuffer();

    GpuTriangleBuffer(const GpuTriangleBuffer&) = delete;
    GpuTriangleBuffer& operator=(const GpuTriangleBuffer&) = delete;
    GpuTriangleBuffer(GpuTriangleBuffer&&) = delete;
    GpuTriangleBuffer& operator=(GpuTriangleBuffer&&) = delete;

    [[nodiscard]] std::size_t size() const noexcept;
    void bind_for_shader_storage(std::uint32_t binding) const;
    void upload(std::span<const Triangle> triangles);
    void download(std::span<Triangle> triangles) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class GpuVertexUpdater final {
public:
    GpuVertexUpdater(
        std::span<const InitialTriangle> initial,
        GpuTriangleBuffer& current);
    ~GpuVertexUpdater();

    GpuVertexUpdater(const GpuVertexUpdater&) = delete;
    GpuVertexUpdater& operator=(const GpuVertexUpdater&) = delete;
    GpuVertexUpdater(GpuVertexUpdater&&) = delete;
    GpuVertexUpdater& operator=(GpuVertexUpdater&&) = delete;

    void update(std::chrono::duration<double> elapsed);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
