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
    const uint32_t s_brdfSize = 512;

    BaseTexture s_newportTex;
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
    glCreateFramebuffers(1, &s_captureFBO);
    glCreateRenderbuffers(1, &s_captureRBO);

    s_brdfTexture = createBrdfLutTexture();

    // load the HDR environment map
    s_newportTex.create("resource/newport_loft.hdr");
    s_newportTex.parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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
    // Generate a 2D LUT from the BRDF quation used.
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
    glNamedFramebufferTexture(s_captureFBO, GL_COLOR_ATTACHMENT0, tex->m_TextureID, 0);
    glNamedRenderbufferStorage(s_captureRBO, GL_DEPTH_COMPONENT24, s_brdfSize, s_brdfSize);
    glNamedFramebufferRenderbuffer(s_captureFBO, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_captureRBO);
    GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);
    s_programBrdf.bind();

    glBindFramebuffer(GL_FRAMEBUFFER, s_captureFBO);
    glViewport(0, 0, s_brdfSize, s_brdfSize);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    s_triangle.draw();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return tex;
}

bool LightProbe::initialize()
{
    m_envCubemap = BaseTexture::Create(m_envMapSize, m_envMapSize, GL_TEXTURE_CUBE_MAP, GL_RGB16F, m_MipmapLevels);
    if (!m_envCubemap) return false;
    m_envCubemap->parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // create an irradiance cubemap
    m_irradianceCubemap = BaseTexture::Create(m_irradianceSize, m_irradianceSize, GL_TEXTURE_CUBE_MAP, GL_RGBA16F, 1);
    if (!m_irradianceCubemap) return false;
    m_irradianceCubemap->parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // create a prefilter cubemap and allocate mips
    m_prefilterCubemap = BaseTexture::Create(m_prefilterSize, m_prefilterSize, GL_TEXTURE_CUBE_MAP, GL_RGBA16F, 8);
    if (!m_prefilterCubemap) return false;
    m_prefilterCubemap->parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glCreateFramebuffers(1, &m_captureFBO);
    glCreateRenderbuffers(1, &m_captureRBO);

    glNamedFramebufferTexture(s_captureFBO, GL_COLOR_ATTACHMENT0, m_envCubemap->m_TextureID, 0);
    glNamedRenderbufferStorage(m_captureRBO, GL_DEPTH_COMPONENT24, s_brdfSize, s_brdfSize);
    glNamedFramebufferRenderbuffer(s_captureFBO, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_captureRBO);
    GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);

    return true;
}

bool LightProbe::update()
{
    createEnvCube();
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    createIrradiance(m_envCubemap);
    createPrefilter(m_envCubemap);
    glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    return true;
}

void LightProbe::destroy()
{
    glCreateRenderbuffers(1, &m_captureRBO);
    glCreateFramebuffers(1, &m_captureFBO);
    m_captureRBO = 0;
    m_captureFBO = 0;
}

LightProbe::~LightProbe()
{
    destroy();
}

void LightProbe::createEnvCube()
{
    // PROFILEGL("Env cubemap");

    // set up projection and view matrices for capturing data onto the 6 cubemap face directions
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

    // convert HDR equirectangular environment map to cubemap equivalent
    s_newportTex.bind(0);

    s_equirectangularToCubemapShader.bind();
    s_equirectangularToCubemapShader.setUniform("equirectangularMap", 0);
    s_equirectangularToCubemapShader.setUniform("projection", captureProjection);

    assert(m_captureFBO != 0);
    assert(m_captureRBO != 0);
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);

    // don't forget to configure the viewport to the capture dimensions.
    glViewport(0, 0, m_envMapSize, m_envMapSize);

    for (unsigned int i = 0; i < 6; ++i)
    {
        s_equirectangularToCubemapShader.setUniform("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_envCubemap->m_TextureID, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        s_cube.draw();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // then let OpenGL generate mipmaps from first mip face (combatting visible dots artifact)
    m_envCubemap->parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    m_envCubemap->generateMipmap();
}

void LightProbe::createIrradiance(const BaseTexturePtr& envMap)
{
    // solve diffuse integral by convolution to create an irradiance cbuemap
    const int localSize = 16;
    envMap->bind(0);
    s_programIrradiance.bind();
    s_programIrradiance.setUniform("uEnvMap", 0);

    // Set layered true to use whole cube face
    s_programIrradiance.bindImage("uCube", m_irradianceCubemap, 0, 0, GL_TRUE, 0, GL_WRITE_ONLY);
    glDispatchCompute((m_irradianceSize - 1) / localSize + 1, (m_irradianceSize - 1) / localSize + 1, 6);
}

void LightProbe::createPrefilter(const BaseTexturePtr& envMap)
{
    // run a quasi monte-carlo simulation on the environment lighting to create a prefilter cubemap
    glCopyImageSubData(
        envMap->m_TextureID, GL_TEXTURE_CUBE_MAP, 1, 0, 0, 0,
        m_prefilterCubemap->m_TextureID, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
        m_prefilterSize, m_prefilterSize, 6);

    const int localSize = 16;
    // Skip mipLevel 0
    auto size = m_prefilterSize / 2;
    auto mipLevel = 1;
    auto maxLevel = int(glm::floor(glm::log2(float(size))));
    envMap->bind(0);
    s_programPrefilter.bind();
    s_programPrefilter.setUniform("uEnvMap", 0);
    for (auto tsize = size; tsize > 0; tsize /= 2)
    {
        s_programPrefilter.setUniform("uRoughness", float(mipLevel) / maxLevel);
        // Set layered true to use whole cube face
        s_programPrefilter.bindImage("uCube", m_prefilterCubemap, 0, mipLevel, GL_TRUE, 0, GL_WRITE_ONLY);
        glDispatchCompute((tsize - 1) / localSize + 1, (tsize - 1) / localSize + 1, 6);
        mipLevel++;
    }
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

