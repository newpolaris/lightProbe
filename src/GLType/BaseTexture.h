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
	void generateMipmap();
	void parameter(GLenum pname, GLint param);

private:

    bool createFromFileGLI(const std::string& Filename);
    bool createFromFileSTB(const std::string& Filename);

	GLuint m_TextureID;
	GLenum m_Target;
	GLenum m_Format;
	GLint m_Width;
	GLint m_Height;
	GLint m_Depth;
	GLint m_MipCount;
};

