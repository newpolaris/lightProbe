#pragma once

#include <GL/glew.h>
#include <string>

class BaseTexture
{
private:
    bool createFromFileGLI(const std::string& Filename);
    bool createFromFileSTB(const std::string& Filename);

public:
	BaseTexture();
    virtual ~BaseTexture();

	bool create(const std::string& Filename);
	void destroy();
	void bind(GLuint unit) const;
	void unbind(GLuint unit) const;
    void setGenerateMipmap(bool bGenerate = true);

    bool m_bGenerateMipmap; // Support STB loader only
	GLuint m_TextureID;
	GLenum m_Target;
    GLenum m_MinFilter;
    GLenum m_MagFilter;
    GLenum m_WrapS;
    GLenum m_WrapT;
    GLenum m_WrapR;
};

