#include <gli/gli.hpp>
#include "BaseTexture.h"

BaseTexture::BaseTexture() :
	m_TextureID(0),
	m_Target(GL_INVALID_ENUM),
    m_MinFilter(GL_LINEAR_MIPMAP_LINEAR),
    m_MagFilter(GL_LINEAR),
    m_WrapS(GL_CLAMP_TO_EDGE),
    m_WrapT(GL_CLAMP_TO_EDGE),
    m_WrapR(GL_CLAMP_TO_EDGE)
{
}

BaseTexture::~BaseTexture()
{
	destroy();
}

/// Filename can be KTX or DDS files
bool BaseTexture::create(const std::string& Filename)
{
	gli::texture Texture = gli::load(Filename);
	if(Texture.empty())
		return false;

	gli::gl GL(gli::gl::PROFILE_GL33);
	gli::gl::format const Format = GL.translate(Texture.format(), Texture.swizzles());
	GLenum Target = GL.translate(Texture.target());

	GLuint TextureName = 0;
	glGenTextures(1, &TextureName);
	glBindTexture(Target, TextureName);
	glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(Texture.levels() - 1));
	glTexParameteri(Target, GL_TEXTURE_SWIZZLE_R, Format.Swizzles[0]);
	glTexParameteri(Target, GL_TEXTURE_SWIZZLE_G, Format.Swizzles[1]);
	glTexParameteri(Target, GL_TEXTURE_SWIZZLE_B, Format.Swizzles[2]);
	glTexParameteri(Target, GL_TEXTURE_SWIZZLE_A, Format.Swizzles[3]);

	glTexParameteri(Target, GL_TEXTURE_WRAP_S, m_WrapS);
	glTexParameteri(Target, GL_TEXTURE_WRAP_T, m_WrapT);
	glTexParameteri(Target, GL_TEXTURE_WRAP_R, m_WrapR);
	glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, m_MinFilter);
	glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, m_MagFilter);

	glm::tvec3<GLsizei> const Extent(Texture.extent());
	GLsizei const FaceTotal = static_cast<GLsizei>(Texture.layers() * Texture.faces());

	switch(Texture.target())
	{
	case gli::TARGET_1D:
		glTexStorage1D(
			Target, static_cast<GLint>(Texture.levels()), Format.Internal, Extent.x);
		break;
	case gli::TARGET_1D_ARRAY:
	case gli::TARGET_2D:
	case gli::TARGET_CUBE:
		glTexStorage2D(
			Target, static_cast<GLint>(Texture.levels()), Format.Internal,
			Extent.x, Extent.y);
		break;
	case gli::TARGET_2D_ARRAY:
	case gli::TARGET_3D:
	case gli::TARGET_CUBE_ARRAY:
		glTexStorage3D(
			Target, static_cast<GLint>(Texture.levels()), Format.Internal,
			Extent.x, Extent.y,
			Texture.target() == gli::TARGET_3D ? Extent.z : FaceTotal);
		break;
	default:
		assert(0);
		break;
	}

	for(std::size_t Layer = 0; Layer < Texture.layers(); ++Layer)
	for(std::size_t Face = 0; Face < Texture.faces(); ++Face)
	for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
	{
		GLsizei const LayerGL = static_cast<GLsizei>(Layer);
		glm::tvec3<GLsizei> Extent(Texture.extent(Level));
		GLenum Target = gli::is_target_cube(Texture.target())
			? static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + Face)
			: Target;

		switch(Texture.target())
		{
		case gli::TARGET_1D:
			if(gli::is_compressed(Texture.format()))
				glCompressedTexSubImage1D(
					Target, static_cast<GLint>(Level), 0, Extent.x,
					Format.Internal, static_cast<GLsizei>(Texture.size(Level)),
					Texture.data(Layer, Face, Level));
			else
				glTexSubImage1D(
					Target, static_cast<GLint>(Level), 0, Extent.x,
					Format.External, Format.Type,
					Texture.data(Layer, Face, Level));
			break;
		case gli::TARGET_1D_ARRAY:
		case gli::TARGET_2D:
		case gli::TARGET_CUBE:
			if(gli::is_compressed(Texture.format()))
				glCompressedTexSubImage2D(
					Target, static_cast<GLint>(Level),
					0, 0,
					Extent.x,
					Texture.target() == gli::TARGET_1D_ARRAY ? LayerGL : Extent.y,
					Format.Internal, static_cast<GLsizei>(Texture.size(Level)),
					Texture.data(Layer, Face, Level));
			else
				glTexSubImage2D(
					Target, static_cast<GLint>(Level),
					0, 0,
					Extent.x,
					Texture.target() == gli::TARGET_1D_ARRAY ? LayerGL : Extent.y,
					Format.External, Format.Type,
					Texture.data(Layer, Face, Level));
			break;
		case gli::TARGET_2D_ARRAY:
		case gli::TARGET_3D:
		case gli::TARGET_CUBE_ARRAY:
			if(gli::is_compressed(Texture.format()))
				glCompressedTexSubImage3D(
					Target, static_cast<GLint>(Level),
					0, 0, 0,
					Extent.x, Extent.y,
					Texture.target() == gli::TARGET_3D ? Extent.z : LayerGL,
					Format.Internal, static_cast<GLsizei>(Texture.size(Level)),
					Texture.data(Layer, Face, Level));
			else
				glTexSubImage3D(
					Target, static_cast<GLint>(Level),
					0, 0, 0,
					Extent.x, Extent.y,
					Texture.target() == gli::TARGET_3D ? Extent.z : LayerGL,
					Format.External, Format.Type,
					Texture.data(Layer, Face, Level));
			break;
		default: 
			assert(0); 
			break;
		}
	}
	m_Target = Target;
	m_TextureID = TextureName;
	glBindTexture(Target, 0);
	return true;
}

void BaseTexture::destroy()
{
	if (!m_TextureID)
	{
		glDeleteTextures( 1, &m_TextureID);
		m_TextureID = 0;
	}
	m_Target = GL_INVALID_ENUM;
}

void BaseTexture::bind(GLuint unit) const
{
	assert( 0u != m_TextureID );  
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(m_Target, m_TextureID);
}

void BaseTexture::unbind(GLuint unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(m_Target, 0u);
}

#if 0
bool TextureGL::create(gli::texture &&cpuTexture) {
    mCpuTexture = std::move(cpuTexture);

    GLformat format;
    gl::GLenum target;

    std::tie(format, target) = getFormatAndTarget(mCpuTexture);

    if(mCpuTexture.format() == gli::FORMAT_D32_SFLOAT_PACK32) {
        format.internal = gl::GL_R32F;
        format.external = gl::GL_RED;
        format.type = gl::GL_FLOAT;
        format.swizzleRed = gl::GL_RED;
        format.swizzleGreen = gl::GL_RED;
        format.swizzleBlue = gl::GL_RED;
        format.swizzleAlpha = gl::GL_ONE;
    }

    TextureDimension dimension = getTextureDimension(mCpuTexture.target());
    gl::GLenum minFilterGL = glConvert::textureMinFilter(mParams.minFilter);
    gl::GLenum magFilterGL = glConvert::textureMagFilter(mParams.magFilter);
    gl::GLenum wrapS = glConvert::textureWrapMode(mParams.wrapU);
    gl::GLenum wrapT = glConvert::textureWrapMode(mParams.wrapV);
    gl::GLenum wrapR = glConvert::textureWrapMode(mParams.wrapW);

    if(mId != 0) {
        gl::glDeleteTextures(1, &mId);
    }

    gl::glGenTextures(1, &mId);

    gl::glBindTexture(target, mId);
    gl::glTexParameteri(target, gl::GL_TEXTURE_MIN_FILTER, minFilterGL);
    gl::glTexParameteri(target, gl::GL_TEXTURE_MAG_FILTER, magFilterGL);
    gl::glTexParameteri(target, gl::GL_TEXTURE_BASE_LEVEL, (gl::GLint)mCpuTexture.base_level());
    gl::glTexParameteri(target, gl::GL_TEXTURE_MAX_LEVEL, (gl::GLint)mCpuTexture.max_level());
    gl::glTexParameteri(target, gl::GL_TEXTURE_WRAP_S, wrapS);

    gl::glTexParameteri(target, gl::GL_TEXTURE_SWIZZLE_R, format.swizzleRed);
    gl::glTexParameteri(target, gl::GL_TEXTURE_SWIZZLE_G, format.swizzleGreen);
    gl::glTexParameteri(target, gl::GL_TEXTURE_SWIZZLE_B, format.swizzleBlue);
    gl::glTexParameteri(target, gl::GL_TEXTURE_SWIZZLE_A, format.swizzleAlpha);

    if(dimension == TextureDimension::Texture2D || dimension == TextureDimension::Texture3D) {
        gl::glTexParameteri(target, gl::GL_TEXTURE_WRAP_T, wrapT);
    }
    if(dimension == TextureDimension::Texture3D) {
        gl::glTexParameteri(target, gl::GL_TEXTURE_WRAP_R, wrapR);
    }

    {
        const gli::extent3d &extent = mCpuTexture.extent();
        gl::GLsizei faceTotal = static_cast<gl::GLsizei>(mCpuTexture.layers() * mCpuTexture.faces());

        switch(mCpuTexture.target())
        {
        case gli::TARGET_1D:
            gl::glTexStorage1D(target, (gl::GLint)mCpuTexture.levels(), format.internal, extent.x);
            break;
        case gli::TARGET_2D:
            gl::glTexStorage2D(target, (gl::GLint)mCpuTexture.levels(), format.internal, extent.x, extent.y);
            break;
        case gli::TARGET_1D_ARRAY:
        case gli::TARGET_CUBE:
            gl::glTexStorage2D(target, (gl::GLint)mCpuTexture.levels(), format.internal, extent.x, extent.y);
            break;
        case gli::TARGET_2D_ARRAY:
        case gli::TARGET_CUBE_ARRAY:
            gl::glTexStorage3D(target, (gl::GLint)mCpuTexture.levels(), format.internal, extent.x, extent.y, faceTotal);
            break;
        case gli::TARGET_3D:
            gl::glTexStorage3D(target, (gl::GLint)mCpuTexture.levels(), format.internal, extent.x, extent.y, extent.z);
            break;
        default:
            Log()->error("Trying to create texture from unknown type");
            gl::glBindTexture(target, 0);
            return false;
        }
    }

    size_t rowPitch = gli::block_size(mCpuTexture.format()) * mCpuTexture.extent().x;
    bool restoreUnpackAlignment = false;
    if(!math::isAlignedTo(rowPitch, size_t(4))) {
        if(math::isAlignedTo(rowPitch, size_t(2))) {
            gl::glPixelStorei(gl::GL_UNPACK_ALIGNMENT, 2);
        } else {
            gl::glPixelStorei(gl::GL_UNPACK_ALIGNMENT, 1);
        }

        restoreUnpackAlignment = true;
    }

    gl::GLenum originalTarget = target;

    for(std::size_t layer = 0; layer < mCpuTexture.layers(); ++layer)
    {
        for(std::size_t face = 0; face < mCpuTexture.faces(); ++face)
        {
            for(std::size_t level = 0; level < mCpuTexture.levels(); ++level)
            {
                const gl::GLsizei layerGL = static_cast<gl::GLsizei>(layer);
                const gli::extent3d &extent = mCpuTexture.extent(level);
                target = (mCpuTexture.target() == gli::TARGET_CUBE) ? static_cast<gl::GLenum>(gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X + (gl::GLint)face) : target;

                switch(mCpuTexture.target())
                {
                case gli::TARGET_1D:
                    if(gli::is_compressed(mCpuTexture.format()))
                        gl::glCompressedTexSubImage1D(
                            target, static_cast<gl::GLint>(level), 0, extent.x,
                            format.internal, static_cast<gl::GLsizei>(mCpuTexture.size(level)),
                            mCpuTexture.data(layer, face, level));
                    else
                        glTexSubImage1D(
                            target, static_cast<gl::GLint>(level), 0, extent.x,
                            format.external, format.type,
                            mCpuTexture.data(layer, face, level));
                    break;
                case gli::TARGET_1D_ARRAY:
                case gli::TARGET_2D:
                case gli::TARGET_CUBE:
                    if(gli::is_compressed(mCpuTexture.format()))
                        glCompressedTexSubImage2D(
                            target, static_cast<gl::GLint>(level),
                            0, 0,
                            extent.x,
                            mCpuTexture.target() == gli::TARGET_1D_ARRAY ? layerGL : extent.y,
                            format.internal, static_cast<gl::GLsizei>(mCpuTexture.size(level)),
                            mCpuTexture.data(layer, face, level));
                    else
                        glTexSubImage2D(
                            target, static_cast<gl::GLint>(level),
                            0, 0,
                            extent.x,
                            mCpuTexture.target() == gli::TARGET_1D_ARRAY ? layerGL : extent.y,
                            format.external, format.type,
                            mCpuTexture.data(layer, face, level));
                    break;
                case gli::TARGET_2D_ARRAY:
                case gli::TARGET_3D:
                case gli::TARGET_CUBE_ARRAY:
                    if(gli::is_compressed(mCpuTexture.format()))
                        glCompressedTexSubImage3D(
                            target, static_cast<gl::GLint>(level),
                            0, 0, mCpuTexture.target() == gli::TARGET_3D ? 0 : layerGL,
                            extent.x, extent.y,
                            mCpuTexture.target() == gli::TARGET_3D ? extent.z : 1,
                            format.internal, static_cast<gl::GLsizei>(mCpuTexture.size(level)),
                            mCpuTexture.data(layer, face, level));
                    else
                        glTexSubImage3D(
                            target, static_cast<gl::GLint>(level),
                            0, 0, mCpuTexture.target() == gli::TARGET_3D ? 0 : (gl::GLint)(layerGL * mCpuTexture.faces() + face),
                            extent.x, extent.y,
                            mCpuTexture.target() == gli::TARGET_3D ? extent.z : 1,
                            format.external, format.type,
                            mCpuTexture.data(layer, face, level));
                    break;
                default:
                    assert(0);
                    break;
                }
            }
        }
    }

    if(restoreUnpackAlignment) {
        gl::glPixelStorei(gl::GL_UNPACK_ALIGNMENT, 4);
    }

    gl::glBindTexture(originalTarget, 0);
}
#endif
