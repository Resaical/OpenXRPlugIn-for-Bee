#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "core/ecs.hpp"
#include "core/fileio.hpp"
#include "platform/opengl/shader_gl.hpp"
#include "rendering/render_components.hpp"
#include "platform/opengl/render_gl.hpp"

struct XrFovf;

namespace bee
{

struct CameraUBO;
struct PointLightsUBO;
struct TransformsUBO;
struct DirectionalLightsUBO;
struct MeshRenderer;
struct Transform;


class RendererXR : public System
{
public:
    RendererXR();
    ~RendererXR() override;
    void Render() override;
    void BeguinRenderXR();
    void RenderFlat();
    void EndRenderXR();
    void LoadEnvironment(FileIO::Directory directory, const std::string& filename);
    void SetFog(glm::vec4 fogColor, float forNear, float fogFar);

    /// <summary>
    /// Enables or disables alpha blending for 2D games.
    /// If enabled, depth testing will be *disabled*, and objects will be pre-sorted by transform.Translation.z.
    /// This is sufficient for 2D games, but not for 3D with arbitrary camera settings.
    /// </summary>
    /// <param name="value">Whether or not to enable alpha blending.</param>
    void SetAlphaBlending2D(bool value);

    /// <summary>
    /// Sets the vignette intensity.
    /// </summary>
    /// <param name="value">The new vignette intensity to apply. Will be clamped to the range [0,1], where 0 = off and 1 =
    /// maximum intensity.</param>
    void SetVignette(float value);

    //XR
    float m_eyeOffset = 0.0f;

private:
    void ProcessObjectForRendering(const bee::MeshRenderer& renderer, bee::Transform& transform, int& instances);
    void RenderCurrentInstances(int instances);
    void CreateFrameBuffers();
    void DeleteFrameBuffers();
    void CreateShadowMaps();
    void DeleteShadowMaps();
    void RenderShadowMaps();
    void DeleteUBOs();
    void ApplyMaterial(const std::shared_ptr<bee::Material>& material);

public:
    int m_width = -1;                       //INTRUSION
    int m_height = -1;

private:
    int m_msaa = 4;
    static const int m_max_dir_lights = 4;                   // In sync with Uniformsl.glsl
    static const unsigned int c_invalid_index = 4294967295;  // Just -1 casted to unsigned int
    const int m_shadowResolution = 2048;

    unsigned int m_msaaFramebuffer = 0;
    unsigned int m_msaaColorbuffer = 0;
    unsigned int m_msaaDepthbuffer = 0;
    unsigned int m_resolvedFramebuffer = 0;
    unsigned int m_resolvedColorbuffer = 0;
    unsigned int m_resolvedDepthbuffer = 0;

public:
    unsigned int m_finalFramebuffer = 0;                          //INTRUSSION

private:
    unsigned int m_finalColorbuffer = 0;
    unsigned int m_shadowFBOs[m_max_dir_lights];
    unsigned int m_shadowMaps[m_max_dir_lights];

    std::shared_ptr<Shader> m_toneMappingPass = nullptr;
    std::shared_ptr<Shader> m_envDebugPass = nullptr;

    DirectionalLightsUBO* m_dirLightsData = nullptr;
    unsigned int m_dirLightsUBO = c_invalid_index;

    CameraUBO* m_cameraData = nullptr;
    unsigned int m_cameraUBO = c_invalid_index;

    PointLightsUBO* m_pointLightsData = nullptr;
    unsigned int m_pointLightsUBO = c_invalid_index;

    TransformsUBO* m_transformsData = nullptr;
    unsigned int m_transformsUBO = c_invalid_index;

    std::shared_ptr<Material> m_currentMaterial;
    std::shared_ptr<Mesh> m_currentMesh;

    std::shared_ptr<Shader> m_forwardPass = nullptr;
    std::shared_ptr<Shader> m_post = nullptr;
    std::shared_ptr<Shader> m_shadowPass = nullptr;

    unsigned int m_hdrTexture = 0;
    unsigned int m_envCubemap = 0;
    unsigned int m_diffuseIBL = 0;
    unsigned int m_specularIBL = 0;
    unsigned int m_lutIBL = 0;
    DebugData m_debugData;
    int m_iblSpecularMipCount = -1;

    float m_vignette = 0.0f;

    bool m_useAlphaBlending = false;

#ifdef BEE_INSPECTOR
    void Reload();
    void Inspect() override;
    void Inspect(bee::Entity e) override;
    bool m_reload = false;
    int m_drawCalls = 0;
    int m_drawInstances = 0;
#endif
};

}  // namespace bee

