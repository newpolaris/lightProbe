#pragma once

#include <GL/glew.h>
#include <string>

class BaseTexture
{
public:

	BaseTexture();
    virtual ~BaseTexture();

	bool create(GLint width, GLint height, GLenum target, GLenum format, GLuint levels);
	bool create(const std::string& filename);
	void destroy();
	void bind(GLuint unit) const;
	void unbind(GLuint unit) const;
	void generateMipmap();
	void parameter(GLenum pname, GLint param);

    bool createFromFileGLI(const std::string& filename);
    bool createFromFileSTB(const std::string& filename);

	GLuint m_TextureID;
	GLenum m_Target;
	GLenum m_Format;
	GLint m_Width;
	GLint m_Height;
	GLint m_Depth;
	GLint m_MipCount;
};

