#include <gli/gli.hpp>
#include <tools/stb_image.h>
#include "BaseTexture.h"

namespace {
    std::string GetFileExtension(const std::string& Filename)
    {
        return Filename.substr(Filename.find_last_of(".") + 1);
    }

    bool Stricompare(const std::string& str1, const std::string& str2) {
        std::string str1Cpy(str1);
        std::string str2Cpy(str2);
        std::transform(str1Cpy.begin(), str1Cpy.end(), str1Cpy.begin(), ::tolower);
        std::transform(str2Cpy.begin(), str2Cpy.end(), str2Cpy.begin(), ::tolower);
        return (str1Cpy == str2Cpy);
    }

    GLenum GetComponent(int Components)
    {
        switch (Components)
        {
        case 1u:
            return GL_RED;
        case 2u:
            return GL_RG;
        case 3u:
            return GL_RGB;
        case 4u:
            return GL_RGBA;
        default:
            assert(false);
        };
        return 0;
    }

    GLenum GetInternalComponent(int Components, bool bFloat)
    {
        GLenum Base = GetComponent(Components);
        if (bFloat)
        {
            switch (Base)
            {
            case GL_RED:
                return GL_R16F;
            case GL_RG:
                return GL_RG16F;
            case GL_RGB:
                return GL_RGB16F;
            case GL_RGBA:
                return GL_RGBA16F;
            }
        }
        else
        {
            switch (Base)
            {
            case GL_RED:
                return GL_R8;
            case GL_RG:
                return GL_RG8;
            case GL_RGB:
                return GL_RGB8;
            case GL_RGBA:
                return GL_RGBA8;
            }
        }
        return Base;
    }
}

BaseTexture::BaseTexture() :
	m_TextureID(0),
	m_Target(GL_INVALID_ENUM),
    m_MinFilter(GL_LINEAR_MIPMAP_LINEAR),
    m_MagFilter(GL_LINEAR),
    m_WrapS(GL_CLAMP_TO_EDGE),
    m_WrapT(GL_CLAMP_TO_EDGE),
    m_WrapR(GL_CLAMP_TO_EDGE),
	m_Width(0),
	m_Height(0),
	m_Depth(0),
	m_MipCount(0)
{
}

BaseTexture::~BaseTexture()
{
	destroy();
}

bool BaseTexture::create(const std::string& Filename)
{
    if (Filename.empty()) return false;
    std::string ext = GetFileExtension(Filename);
    if (Stricompare(ext, "DDX") || Stricompare(ext, "DDS"))
        return createFromFileGLI(Filename);
    return createFromFileSTB(Filename);
} 

// Filename can be KTX or DDS files
bool BaseTexture::createFromFileGLI(const std::string& Filename)
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
	m_MipCount = static_cast<GLint>(Texture.levels());
	m_Width = Extent.x;
	m_Height = Extent.y;
	m_Depth = Texture.target() == gli::TARGET_3D ? Extent.z : FaceTotal;
	glBindTexture(Target, 0);
	return true;
}

// Filename can be JPG, PNG, TGA, BMP, PSD, GIF, HDR, PIC files
bool BaseTexture::createFromFileSTB(const std::string& Filename)
{
    stbi_set_flip_vertically_on_load(true);

	GLenum Target = GL_TEXTURE_2D;
    GLenum Type = GL_UNSIGNED_BYTE;
    int Width = 0, Height = 0, nrComponents = 0;
    void* Data = nullptr;
    std::string Ext = GetFileExtension(Filename);
    if (Stricompare(Ext, "HDR"))
    {
        Type = GL_FLOAT;
        Data = stbi_loadf(Filename.c_str(), &Width, &Height, &nrComponents, 0);
    }
    else
    {
        Data = stbi_load(Filename.c_str(), &Width, &Height, &nrComponents, 0);
    }
    if (!Data) return false;

    GLenum Format = GetComponent(nrComponents);
    GLenum InternalFormat = GetInternalComponent(nrComponents, Type == GL_FLOAT);

	GLuint TextureName = 0;
	glGenTextures(1, &TextureName);
	glBindTexture(Target, TextureName);
	// To simpfy mipmap generating use TexImage rather than glTexStorage2D
	glTexImage2D(Target, 0, InternalFormat, Width, Height, 0, Format, Type, Data);

	glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    stbi_image_free(Data);


	m_Target = Target;
	m_TextureID = TextureName;
	m_Width = Width;
	m_Height = Height;
	m_Depth = 1;
	m_MipCount = 1;
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

void BaseTexture::generateMipmap()
{
	assert(m_Target != GL_INVALID_ENUM);
	assert(m_TextureID != 0);

	glBindTexture(m_Target, m_TextureID);
	if (true)
	{
		GLuint MipCount = GLuint(std::floor(std::log2(std::max(m_Width, m_Height))));
		glTexParameteri(m_Target, GL_TEXTURE_WRAP_S, m_WrapS);
		glTexParameteri(m_Target, GL_TEXTURE_WRAP_T, m_WrapT);
		glTexParameteri(m_Target, GL_TEXTURE_WRAP_R, m_WrapR);
		glTexParameteri(m_Target, GL_TEXTURE_MIN_FILTER, m_MinFilter);
		glTexParameteri(m_Target, GL_TEXTURE_MAG_FILTER, m_MagFilter);
		glTexParameteri(m_Target, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(m_Target, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(m_Target, GL_TEXTURE_MAX_LEVEL, MipCount);
		m_MipCount = MipCount;
	}
	glGenerateMipmap(m_Target);
	glBindTexture(m_Target, 0u);
}
