#pragma once

#include <GL/glew.h>
#include <string>

class BaseTexture
{
public:
	BaseTexture();
    virtual ~BaseTexture();

	bool create(const std::string& Filename);
	void destroy();
	void bind(GLuint unit) const;
	void unbind(GLuint unit) const;

	GLuint m_TextureID;
	GLenum m_Target;
    GLenum m_MinFilter;
    GLenum m_MagFilter;
    GLenum m_WrapS;
    GLenum m_WrapT;
    GLenum m_WrapR;
};

