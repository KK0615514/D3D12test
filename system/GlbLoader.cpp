#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include "GlbLoader.h"

#include <Windows.h>
#include <stdexcept>
#include <string>
#include <array>
#include <cstring>

static const uint8_t* GetElement(const tinygltf::Model& model , const tinygltf::Accessor& accessor , size_t i , size_t elementSize){
    if (accessor.bufferView < 0 || i >= accessor.count)
        throw std::runtime_error("Invalid GLB accessor");

    const auto& view = model.bufferViews.at(accessor.bufferView);
    const auto& buffer = model.buffers.at(view.buffer);
    const int stride = accessor.ByteStride(view);

    if (stride < 0 || static_cast<size_t>(stride) < elementSize)
        throw std::runtime_error("Invalid GLB accessor stride");

    const size_t offset =
        view.byteOffset + accessor.byteOffset + i * static_cast<size_t>(stride);

    if (offset > buffer.data.size() ||
        elementSize > buffer.data.size() - offset)
        throw std::runtime_error("GLB accessor exceeds buffer");

    return buffer.data.data() + offset;
}

template <size_t N>
static std::array<float, N> ReadFloats(const tinygltf::Model& model , const tinygltf::Accessor& accessor , size_t i){
    const int expectedType =
        N == 3 ? TINYGLTF_TYPE_VEC3 : TINYGLTF_TYPE_VEC2;

    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        accessor.type != expectedType)
        throw std::runtime_error("Unsupported GLB vertex format");

    std::array<float, N> result{};
    std::memcpy(result.data(),
        GetElement(model, accessor, i, sizeof(result)),
        sizeof(result));
    return result;
}

static uint32_t ReadIndex(const tinygltf::Model& model , const tinygltf::Accessor& accessor , size_t i) {
    if (accessor.type != TINYGLTF_TYPE_SCALAR)
        throw std::runtime_error("Invalid GLB index format");

    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        uint8_t value;
        std::memcpy(&value, GetElement(model, accessor, i, sizeof(value)), sizeof(value));
        return value;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        uint16_t value;
        std::memcpy(&value, GetElement(model, accessor, i, sizeof(value)), sizeof(value));
        return value;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        uint32_t value;
        std::memcpy(&value, GetElement(model, accessor, i, sizeof(value)), sizeof(value));
        return value;
    }
    default:
        throw std::runtime_error("Unsupported GLB index format");
    }
}

MeshData LoadGlbMeshData(const char* path)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;

    std::string error;
    std::string warning;
    bool ok = loader.LoadBinaryFromFile(&model, &error, &warning, path);
    if (!warning.empty()) {
        OutputDebugStringA(warning.c_str());
    }
    if (!ok) {
        throw std::runtime_error(error);
    }

    MeshData result;

    for (const tinygltf::Mesh& mesh : model.meshes)
    {
        for (const tinygltf::Primitive& primitive : mesh.primitives)
        {
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt == primitive.attributes.end()) {
                continue;
            }

            const tinygltf::Accessor& posAccessor = model.accessors.at(posIt->second);

            const tinygltf::Accessor* normalAccessor = nullptr;
            if (auto it = primitive.attributes.find("NORMAL");
                it != primitive.attributes.end())
            {
                normalAccessor = &model.accessors.at(it->second);
            }

            const tinygltf::Accessor* uvAccessor = nullptr;
            if (auto it = primitive.attributes.find("TEXCOORD_0");
                it != primitive.attributes.end())
            {
                uvAccessor = &model.accessors.at(it->second);
            }

            const uint32_t baseVertex = static_cast<uint32_t>(result.vertices.size());

            for (size_t i = 0; i < posAccessor.count; ++i)
            {
                MeshVertex vertex{};

                const auto p = ReadFloats<3>(model, posAccessor, i);
                vertex.position = { p[0], p[1], p[2] };

                if (normalAccessor)
                {
                    const auto n = ReadFloats<3>(model, *normalAccessor, i);
                    vertex.normal = { n[0], n[1], n[2] };
                }

                if (uvAccessor)
                {
                    const auto uv = ReadFloats<2>(model, *uvAccessor, i);
                    vertex.uv = { uv[0], uv[1] };
                }

                result.vertices.push_back(vertex);
            }

            if (primitive.indices >= 0)
            {
                const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];

                for (size_t i = 0; i < indexAccessor.count; ++i)
                {
                    result.indices.push_back(
                        baseVertex + ReadIndex(model, indexAccessor, i)
                    );
                }
            }
        }
    }

    return result;
}