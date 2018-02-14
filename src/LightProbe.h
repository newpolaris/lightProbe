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

    bool create();

    BaseTexturePtr getEnvCube();
    BaseTexturePtr getIrradiance();
    BaseTexturePtr getPrefilter();

private:

    BaseTexturePtr createEnvCube();
    BaseTexturePtr createIrradiance(const BaseTexturePtr& envMap);
    BaseTexturePtr createPrefilter(const BaseTexturePtr& envMap);

    BaseTexturePtr m_envCubemap;
    BaseTexturePtr m_irradianceCubemap;
    BaseTexturePtr m_prefilterCubemap;
};