#include "LightProbe.h"
// GLM for matrix transformation
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> 

#include <Mesh.h>
#include <GLType/BaseTexture.h>
#include <GLType/ProgramShader.h>
#include <tools/SimpleProfile.h>

using namespace light_probe;

namespace light_probe
{
    unsigned int s_captureFBO = 0;
    unsigned int s_captureRBO = 0;
    const uint32_t s_envMapSize = 512;
    const uint32_t s_brdfSize = 512;

    BaseTexturePtr s_brdfTexture;

    ProgramShader s_equirectangularToCubemapShader;
    ProgramShader s_programIrradiance;
    ProgramShader s_programPrefilter;
    ProgramShader s_programBrdf;

    FullscreenTriangleMesh s_triangle;
    CubeMesh s_cube;

    BaseTexturePtr createBrdfLutTexture();
}

void light_probe::initialize()
{
    s_equirectangularToCubemapShader.initalize();
    s_equirectangularToCubemapShader.addShader(GL_VERTEX_SHADER, "Cubemap.Vertex");
    s_equirectangularToCubemapShader.addShader(GL_FRAGMENT_SHADER, "EquirectangularToCubemap.Fragment");
    s_equirectangularToCubemapShader.link();

    s_programIrradiance.initalize();
    s_programIrradiance.addShader(GL_COMPUTE_SHADER, "Irradiance.Compute");
    s_programIrradiance.link();

    s_programPrefilter.initalize();
    s_programPrefilter.addShader(GL_COMPUTE_SHADER, "Radiance.Compute");
    s_programPrefilter.link();

    s_programBrdf.initalize();
    s_programBrdf.addShader(GL_VERTEX_SHADER, "Brdf.Vertex");
    s_programBrdf.addShader(GL_FRAGMENT_SHADER, "Brdf.Fragment");
    s_programBrdf.link();

    s_triangle.init();
    s_cube.init();

    // generate frame buffers
    glGenFramebuffers(1, &s_captureFBO);
    glGenRenderbuffers(1, &s_captureRBO);

    s_brdfTexture = createBrdfLutTexture();
}

void light_probe::shutdown()
{
    glDeleteFramebuffers(1, &s_captureFBO);
    glDeleteRenderbuffers(1, &s_captureRBO);
    s_captureFBO = 0;
    s_captureRBO = 0;

    s_cube.destroy();
    s_triangle.destroy();
}

BaseTexturePtr light_probe::getBrdfLut()
{
    assert(s_brdfTexture != nullptr);
    return s_brdfTexture;
}

BaseTexturePtr light_probe::createBrdfLutTexture()
{
    // 10. Generate a 2D LUT from the BRDF quation used.
    auto tex = BaseTexture::Create(s_brdfSize, s_brdfSize, GL_TEXTURE_2D, GL_RG16F, 1);
    if (!tex) return nullptr;

    // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
    tex->parameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    tex->parameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    tex->parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    tex->parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // rescale capture framebuffer to brdf texture
    assert(s_captureFBO != 0);
    assert(s_captureRBO != 0);
    glBindFramebuffer(GL_FRAMEBUFFER, s_captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, s_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, s_brdfSize, s_brdfSize);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->m_TextureID, 0);
    s_programBrdf.bind();
    glViewport(0, 0, s_brdfSize, s_brdfSize);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    s_triangle.draw();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return tex;
}

bool LightProbe::create()
{
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    m_envCubemap = createEnvCube();
    m_irradianceCubemap = createIrradiance(m_envCubemap);
    m_prefilterCubemap = createPrefilter(m_envCubemap);
    glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    return true;
}

BaseTexturePtr LightProbe::getEnvCube()
{
    return m_envCubemap;
}

BaseTexturePtr LightProbe::getIrradiance()
{
    return m_irradianceCubemap;
}

BaseTexturePtr LightProbe::getPrefilter()
{
    return m_prefilterCubemap;
}

BaseTexturePtr LightProbe::createEnvCube()
{
    const uint32_t MipmapLevels = 8;

    // 2. load the HDR environment map
    BaseTexture newportTex;
    if(!newportTex.create("resource/newport_loft.hdr"))
        return nullptr;
    newportTex.parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // 3. setup cubemap to render to and attach to framebuffer
    auto tex = BaseTexture::Create(s_envMapSize, s_envMapSize, GL_TEXTURE_CUBE_MAP, GL_RGB16F, MipmapLevels);
    if (!tex) return nullptr;
    tex->parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // 4. set up projection and view matrices for capturing data onto the 6 cubemap face directions
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] =
    {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
    };

    // 5. convert HDR equirectangular environment map to cubemap equivalent
    newportTex.bind(0);

    assert(s_captureFBO != 0);
    assert(s_captureRBO != 0);
    glBindFramebuffer(GL_FRAMEBUFFER, s_captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, s_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, s_envMapSize, s_envMapSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_captureRBO);

    s_equirectangularToCubemapShader.bind();
    s_equirectangularToCubemapShader.setUniform("equirectangularMap", 0);
    s_equirectangularToCubemapShader.setUniform("projection", captureProjection);

    // don't forget to configure the viewport to the capture dimensions.
    glViewport(0, 0, s_envMapSize, s_envMapSize);
    for(unsigned int i = 0; i < 6; ++i)
    {
        s_equirectangularToCubemapShader.setUniform("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, tex->m_TextureID, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        s_cube.draw();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // then let OpenGL generate mipmaps from first mip face (combatting visible dots artifact)
    tex->parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    tex->generateMipmap();

    return tex;
}

BaseTexturePtr LightProbe::createIrradiance(const BaseTexturePtr& envMap)
{
    PROFILEGL("Irradiance cubemap");

    // 6. create an irradiance cubemap
    uint32_t irradianceSize = 16;
    auto tex = BaseTexture::Create(irradianceSize, irradianceSize, GL_TEXTURE_CUBE_MAP, GL_RGBA16F, 1);
    if (!tex) return nullptr;
    tex->parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // 7. solve diffuse integral by convolution to create an irradiance cbuemap
    const int localSize = 16;
    envMap->bind(0);
    s_programIrradiance.bind();
    s_programIrradiance.setUniform("uEnvMap", 0);

    // Set layered true to use whole cube face
    glBindImageTexture(0, tex->m_TextureID,
        0, GL_TRUE, 0, GL_WRITE_ONLY, tex->m_Format);
    glDispatchCompute((irradianceSize - 1) / localSize + 1, (irradianceSize - 1) / localSize + 1, 6);

    return tex;
}

BaseTexturePtr LightProbe::createPrefilter(const BaseTexturePtr& envMap)
{
    PROFILEGL("Prefilter cubemap");

    // 8. create a prefilter cubemap and allocate mips
    GLsizei prefilterSize = 256;
    auto tex = BaseTexture::Create(prefilterSize, prefilterSize, GL_TEXTURE_CUBE_MAP, GL_RGBA16F, 8);
    if (!tex) return nullptr;
    tex->parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // 9. run a quasi monte-carlo simulation on the environment lighting to create a prefilter cubemap
    glCopyImageSubData(
        envMap->m_TextureID, GL_TEXTURE_CUBE_MAP, 1, 0, 0, 0,
        tex->m_TextureID, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
        prefilterSize, prefilterSize, 6);

    const int localSize = 16;
    // Skip mipLevel 0
    auto size = prefilterSize / 2;
    auto mipLevel = 1;
    auto maxLevel = int(glm::floor(glm::log2(float(size))));
    envMap->bind(0);
    s_programPrefilter.bind();
    s_programPrefilter.setUniform("uEnvMap", 0);
    for (auto tsize = size; tsize > 0; tsize /= 2)
    {
        s_programPrefilter.setUniform("uRoughness", float(mipLevel) / maxLevel);
        // Set layered true to use whole cube face
        glBindImageTexture(0, tex->m_TextureID,
            mipLevel, GL_TRUE, 0, GL_WRITE_ONLY, tex->m_Format);
        glDispatchCompute((tsize - 1) / localSize + 1, (tsize - 1) / localSize + 1, 6);
        mipLevel++;
    }

    return tex;
}