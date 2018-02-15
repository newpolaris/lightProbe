#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <GraphicsTypes.h>
#include <vector>

class ProgramShader
{
public:
    ProgramShader() : m_id(0u) {}
    virtual ~ProgramShader() {destroy();}
    
    /** Generate the program id */
    void initalize();
    
    /** Destroy the program id */
    void destroy();        
    
    /** Add a shader and compile it */
    void addShader(GLenum shaderType, const std::string &tag);
    
    //bool compile(); //static (with param)?
    
    bool link(); //static (with param)?
    
    void bind() const { glUseProgram( m_id ); }
    void unbind() const { glUseProgram( 0u ); }
    
    /** Return the program id */
    GLuint getId() const { return m_id; }
    
    bool setUniform(const std::string &name, GLint v) const;
    bool setUniform(const std::string &name, GLfloat v) const;
    bool setUniform(const std::string &name, const glm::vec3 &v) const;
    bool setUniform(const std::string &name, const glm::vec4 &v) const;
    bool setUniform(const std::string &name, const glm::mat3 &v) const;
    bool setUniform(const std::string &name, const glm::mat4 &v) const;

    bool bindTexture(const std::string &name, const BaseTexturePtr& texture, GLint unit);
    bool bindImage(const std::string &name, const BaseTexturePtr &texture, GLint unit, GLint level, GLboolean layered, GLint layer, GLenum access);
  
    static bool setIncludeFromFile(const std::string &includeName, const std::string &filename);
    static std::vector<char> readTextFile(const std::string &filename);

protected:

    static char* incPaths[];
    GLuint m_id;
};