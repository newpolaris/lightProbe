/**
 * 
 *    \file Texture.cpp  
 * 
 */


#include <GL/glew.h>
#include <cassert>
#include <tools/ImageLoader.hpp>
#include <tools/Timer.hpp>
#include <gli/gli.hpp>
#include <tools/gltools.hpp>
#include "irradianceEnvMap.h"
#include "Texture.h"

#define ENABLE_TEXTURE_MIPMAP 1

/** TEXTURE ----------------------------------------- */

void Texture::initialize()
{
	if (!m_id) {
		glGenTextures( 1, &m_id);
	}
}

void Texture::destroy()
{
	if (m_id) {
		glDeleteTextures( 1, &m_id);
	}
	m_id = 0;
}

void Texture::bind(GLuint unit) const
{
	assert( 0u != m_id );  
	glActiveTexture( GL_TEXTURE0 + unit );
	glBindTexture( getTarget(), m_id);

	CHECKGLERROR();
}

void Texture::unbind(GLuint unit) const
{
	glActiveTexture( GL_TEXTURE0 + unit );
	glBindTexture( getTarget(), 0u);
}



/** TEXTURE 2D -------------------------------------- */

bool Texture2D::load(const std::string &name)
{
	assert( 0u != m_id );  
  
	bind();
	{  
#if ENABLE_TEXTURE_MIPMAP  
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
#else  
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif  
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  

#if ENABLE_TEXTURE_ANISOTROPICFILTERING
		int maxAnisoLevel;        
		glGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisoLevel);
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisoLevel);
#endif

		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);  

        Image_t image;

        assert( true == image.load(name.c_str()) );
        assert( GL_TEXTURE_2D == image.target );  

        glTexImage2D( GL_TEXTURE_2D, 0, image.internalFormat, 
                image.width, image.height, 0, 
                image.format, image.type, 
                image.data);
#if ENABLE_TEXTURE_MIPMAP
		glGenerateMipmap( GL_TEXTURE_2D );  
#endif  
	}
	unbind();

	CHECKGLERROR();

	return true;
}

bool Texture2D::load( GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * data )
{
	assert( 0u != m_id );  

	bind();
	{  
#if ENABLE_TEXTURE_MIPMAP  
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
#else  
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif  
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  

#if ENABLE_TEXTURE_ANISOTROPICFILTERING
		int maxAnisoLevel;        
		glGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisoLevel);
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisoLevel);
#endif

		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);  

		assert( data != nullptr );  

		glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, data); 

#if ENABLE_TEXTURE_MIPMAP
		glGenerateMipmap( GL_TEXTURE_2D );  
#endif  
	}
	unbind();

	return true;
}

/** TEXTURE CUBEMAP --------------------------------- */

bool TextureCubemap::load(const std::string &name)
{
	/**
 * 
 */
 	
  assert( 0u != m_id );
  
  size_t dot_idx = name.find_last_of('.', name.size());
  assert( dot_idx != name.npos );

  const auto ext_length = name.size()-(dot_idx);
  std::string extension = name.substr(dot_idx, ext_length);
    
  bind();
  {  
    #if ENABLE_TEXTURE_MIPMAP
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    #else
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    #endif  
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
    
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    /// TODO : rewrite the loader-----------------------------------------------
    Image_t image[6];

    size_t wildcard_idx = name.find_last_of( '*', name.size());
    assert( wildcard_idx != name.npos );

    std::string begin_name = name.substr(0, wildcard_idx);
    std::string end_name = name.substr( wildcard_idx+1, name.size()-(wildcard_idx+1));
    static const std::string wildname[] = { "posx", "negx", "posy", "negy", "posz", "negz"};

    for (int i=0; i<6; ++i)
    {
        std::string texname = begin_name + wildname[i] + end_name;      

        assert( true == image[i].load(texname.c_str()) );      
        assert( GL_TEXTURE_2D == image[i].target ); //
        fprintf( stderr, "%s loaded\n", texname.c_str());

        glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 
                image[i].internalFormat, 
                image[i].width, image[i].height, 0, 
                image[i].format, image[i].type, 
                image[i].data);
    }
    fprintf(stderr, "Computing the irradiance matrices : "); fflush(stderr);
    float tStart = Timer::getInstance().getRelativeTime();

    IrradianceEnvMap::prefilter(image, m_shMatrix);

    fprintf(stderr, "%.3f seconds.\n", 0.001f*(Timer::getInstance().getRelativeTime() - tStart));

    m_bIrradiancePrecomputed = true;
    /// TODO : rewrite the loader-----------------------------------------------
#if ENABLE_TEXTURE_MIPMAP
	glGenerateMipmap( GL_TEXTURE_CUBE_MAP );
#endif   
  }
  unbind();

  return true;
}

