#include <memory>
#include <GraphicsTypes.h>

namespace light_probe
{
    void initialize();
    void shutdown();

    BaseTexturePtr getBrdfLut();
}

class LightProbe
{
public:

    bool initialize();
    bool update();
    void draw();
    void destroy();
    ~LightProbe();

    BaseTexturePtr getEnvCube();
    BaseTexturePtr getIrradiance();
    BaseTexturePtr getPrefilter();

private:

    void createEnvCube();
    void createIrradiance(const BaseTexturePtr& envMap);
    void createPrefilter(const BaseTexturePtr& envMap);

    const uint32_t m_MipmapLevels = 8;
    const uint32_t m_envMapSize = 512;
    const uint32_t m_irradianceSize = 16;
    const uint32_t m_prefilterSize = 256;

    FramebufferPtr m_captureFBO;

    BaseTexturePtr m_envCubemap;
    BaseTexturePtr m_irradianceCubemap;
    BaseTexturePtr m_prefilterCubemap;
};