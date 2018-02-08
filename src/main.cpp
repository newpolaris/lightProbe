#ifdef _WIN32
  #include <windows.h>
#endif

// OpenGL & GLEW (GL Extension Wrangler)
#include <GL/glew.h>

// GLFW
#include <glfw3.h>

// GLM for matrix transformation
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> 

// GLSL Wrangler
#include <glsw/glsw.h>

// Standard libraries
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>

// imgui
#include <tools/imgui.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw_gl3.h>

#include <tools/stb_image.h>
#include <tools/SimpleProfile.h>

#include <tools/TCamera.hpp>
#include <tools/Timer.hpp>
#include <tools/Logger.hpp>
#include <tools/gltools.hpp>

#include <GLType/ProgramShader.h>
#include <GLType/BaseTexture.h>
#include <SkyBox.h>
#include <Mesh.h>
#include <ModelAssImp.h>

#include <fstream>
#include <memory>
#include <vector>
#include <algorithm>

namespace {
    // lights
    // ------
    glm::vec3 lightPositions[] = {
        glm::vec3(-10.0f,  10.0f, 15.0f),
        glm::vec3( 10.0f,  10.0f, 15.0f),
        glm::vec3(-10.0f, -10.0f, 15.0f),
        glm::vec3( 10.0f, -10.0f, 15.0f),
    };
    glm::vec3 lightColors[] = {
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f)
    };

    int nrRows    = 7;
    int nrColumns = 7;
    float spacing = 2.5;

	BaseTexture envCubemap;
    BaseTexture irradianceCubemap;
    BaseTexture prefilterCubemap;
    BaseTexture brdfTexture;
}

struct LightProbe
{
	enum Enum
	{
		Bolonga,
		Kyoto,

		Count
	};

	void load(const std::string& name)
	{
		char filePath[512];
		std::snprintf(filePath, _countof(filePath), "resource/%s_lod.dds", name.c_str());
		m_Tex.create(filePath);

		std::snprintf(filePath, _countof(filePath), "resource/%s_irr.dds", name.c_str());
		m_TexIrr.create(filePath);
	}

	BaseTexture m_Tex;
	BaseTexture m_TexIrr;
};

struct Settings
{
	Settings()
	{
		m_envRotCurr = 0.0f;
		m_envRotDest = 0.0f;
		m_lightDir = glm::vec3(0.8f, 0.2f, 0.5f);
		m_lightCol = glm::vec3(0.5f, 0.f, 0.f);
		m_glossiness = 0.7f;
		m_exposure = 0.0f;
		m_bgType = 3.0f;
		m_radianceSlider = 2.0f;
		m_reflectivity = 0.85f;
		m_rgbDiff = glm::vec3(0.5f, 0.f, 0.f);
		m_rgbSpec = glm::vec3(1.f);
		m_lod = 0.0f;
		m_doDiffuse = true;
		m_doSpecular = true;
		m_doDiffuseIbl = true;
		m_doSpecularIbl = true;
		m_showLightColorWheel = true;
		m_showDiffColorWheel = true;
		m_showSpecColorWheel = true;
		m_metalOrSpec = 0;
		m_meshSelection = 1;
	}

	float m_envRotCurr;
	float m_envRotDest;
	glm::vec3 m_lightDir;
	glm::vec3 m_lightCol;
	float m_glossiness;
	float m_exposure;
	float m_radianceSlider;
	float m_bgType;
	float m_reflectivity;
	glm::vec3 m_rgbDiff;
	glm::vec3 m_rgbSpec;
	float m_lod;
	bool  m_doDiffuse;
	bool  m_doSpecular;
	bool  m_doDiffuseIbl;
	bool  m_doSpecularIbl;
	bool  m_showLightColorWheel;
	bool  m_showDiffColorWheel;
	bool  m_showSpecColorWheel;
	int32_t m_metalOrSpec;
	int32_t m_meshSelection;
};


namespace
{
	struct fRGB {
		float r, g, b; 
	};
    const unsigned int WINDOW_WIDTH = 1280;
    const unsigned int WINDOW_HEIGHT = 720;
	const char* WINDOW_NAME = "Light probe";

	bool bCloseApp = false;
	GLFWwindow* window = nullptr;  

    ProgramShader m_programMesh;
    ProgramShader m_programSky;
    BaseTexture m_pistolTex[4];
	BaseTexture m_pbrTex[5][4];
    SphereMesh m_sphere( 48, 5.0f );
    FullscreenTriangleMesh m_triangle;
    CubeMesh m_cube;
	Settings m_settings;
	ModelPtr m_pistol;
	ModelPtr m_orb;

	LightProbe m_lightProbes[LightProbe::Count];
	LightProbe::Enum m_currentLightProbe;

    //?

    TCamera camera;

    GLuint m_EmptyVAO = 0;
    bool bWireframe = false;

    //?

	void initialize(int argc, char** argv);
	void initExtension();
	void initGL();
	void initWindow(int argc, char** argv);
    void finalizeApp();
	void mainLoopApp();
    void moveCamera( int key, bool isPressed );
	void prepareRender();
    void render();
	void renderHUD();
	void update();
	void updateHUD();

    void glfw_keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void glfw_reshape_callback(GLFWwindow* window, int width, int height);
    void glfw_mouse_callback(GLFWwindow* window, int button, int action, int mods);
    void glfw_motion_callback(GLFWwindow* window, double xpos, double ypos);
	void glfw_char_callback(GLFWwindow* windows, unsigned int c);
	void glfw_scroll_callback(GLFWwindow* windows, double xoffset, double yoffset);
}

// Breakpoints that should ALWAYS trigger (EVEN IN RELEASE BUILDS) [x86]!
#ifdef _MSC_VER
#define DEBUG_BREAK __debugbreak()
#else 
#include <signal.h>
#define DEBUG_BREAK raise(SIGTRAP)
// __builtin_trap() 
#endif

void APIENTRY OpenglCallbackFunction(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    using namespace std;

    // ignore these non-significant error codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) 
        return;

    cout << "---------------------opengl-callback-start------------" << endl;
    cout << "message: " << message << endl;
    cout << "type: ";
    switch(type) {
    case GL_DEBUG_TYPE_ERROR:
        cout << "ERROR";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        cout << "DEPRECATED_BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        cout << "UNDEFINED_BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        cout << "PORTABILITY";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        cout << "PERFORMANCE";
        break;
    case GL_DEBUG_TYPE_OTHER:
        cout << "OTHER";
        break;
    }
    cout << endl;

    cout << "id: " << id << endl;
    cout << "severity: ";
    switch(severity){
    case GL_DEBUG_SEVERITY_LOW:
        cout << "LOW";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        cout << "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        cout << "HIGH";
        break;
    }
    cout << endl;
    cout << "---------------------opengl-callback-end--------------" << endl;
    if (type == GL_DEBUG_TYPE_ERROR)
        DEBUG_BREAK;
}

namespace {
	using namespace std;


	typedef std::vector<std::istream::char_type> ByteArray;
	ByteArray ReadFileSync( const std::string& name )
	{
		std::ifstream inputFile;
		inputFile.open( name, std::ios::binary | std::ios::ate );
		if (!inputFile.is_open())
			return ByteArray();
		auto filesize = static_cast<size_t>(inputFile.tellg());
		ByteArray buf = ByteArray(filesize * sizeof( char ) );
		inputFile.ignore( std::numeric_limits<std::streamsize>::max() );
		inputFile.seekg( std::ios::beg );
		inputFile.read( reinterpret_cast<char*>(buf.data()), filesize );
		inputFile.close();
		return buf;
	}

    glm::mat4 envViewMtx()
    {
        glm::vec3 toTargetNorm = camera.getDirection();
        glm::vec3 fakeUp { 0, 1, 0 };
        glm::vec3 right = glm::cross(fakeUp, toTargetNorm);
        glm::vec3 up = glm::cross(toTargetNorm, right);
        return glm::mat4( 
            glm::vec4(right, 0), 
            glm::vec4(up, 0),
            glm::vec4(toTargetNorm, 0),
            glm::vec4(0, 0, 0, 1));
    }

	void initialize(int argc, char** argv)
	{
		// window maanger
		initWindow(argc, argv);

		// OpenGL extensions
		initExtension();

		// OpenGL
		initGL();

		// ImGui initialize without callbacks
		ImGui_ImplGlfwGL3_Init(window, false);

		// Load FontsR
		// (there is a default font, this is only if you want to change it. see extra_fonts/README.txt for more details)
		// ImGuiIO& io = ImGui::GetIO();
		// io.Fonts->AddFontDefault();
		// io.Fonts->AddFontFromFileTTF("../../extra_fonts/Cousine-Regular.ttf", 15.0f);
		// io.Fonts->AddFontFromFileTTF("../../extra_fonts/DroidSans.ttf", 16.0f);
		// io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyClean.ttf", 13.0f);
		// io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyTiny.ttf", 10.0f);

		// GLSW : shader file manager
		glswInit();
		glswSetPath("./shaders/", ".glsl");
		glswAddDirectiveToken("*", "#version 330 core");

        // App Objects
        camera.setViewParams( glm::vec3( 5.0f, 5.0f, 20.0f), glm::vec3( 5.0f, 5.0f, 0.0f) );
        camera.setMoveCoefficient(0.35f);

        Timer::getInstance().start();

        GLuint m_VertexArrayID;
        GL_ASSERT(glGenVertexArrays(1, &m_VertexArrayID));
        GL_ASSERT(glBindVertexArray(m_VertexArrayID));

        m_sphere.init();
		m_cube.init();
		m_triangle.init();

		m_pistol = std::make_shared<ModelAssImp>();
		m_pistol->create();
		m_pistol->loadFromFile( "resource/pistol/pistol.obj" );

		m_orb = std::make_shared<ModelAssImp>();
		m_orb->create();

		m_lightProbes[LightProbe::Bolonga].load("bolonga");
		m_lightProbes[LightProbe::Kyoto  ].load("kyoto");
        m_currentLightProbe = LightProbe::Bolonga;

        std::string type[] = {
            "rusted_iron", 
            "gold", 
            "grass",
            "plastic",
            "wall"
        };
        std::string textureTypename[4] = {
            "albedo.png",
            "normal.png",
            "metallic.png",
            "roughness.png",
        };

        for (int k = 0; k < 5; k++)
            for(int i = 0; i < 4; i++) 
            {
                std::string path = "resource/" + type[k] + "/" + textureTypename[i];
                m_pbrTex[k][i].create(path);
            }

        for (int i = 0; i < 4; i++)
		{
            m_pistolTex[i].create("resource/pistol/" + textureTypename[i]);
		}

        m_programMesh.initalize();
        m_programMesh.addShader(GL_VERTEX_SHADER, "IblMesh.Vertex");
        m_programMesh.addShader(GL_FRAGMENT_SHADER, "IblMesh.Fragment");
        m_programMesh.link();  

        m_programSky.initalize();
        m_programSky.addShader(GL_VERTEX_SHADER, "IblSkyBox.Vertex");
        m_programSky.addShader(GL_FRAGMENT_SHADER, "IblSkyBox.Fragment");
        m_programSky.link();  

		// to prevent osx input bug
		fflush(stdout);

		prepareRender();
	}

	void initExtension()
	{
        glewExperimental = GL_TRUE;

        GLenum result = glewInit(); 
        if (result != GLEW_OK)
        {
            fprintf( stderr, "Error: %s\n", glewGetErrorString(result));
            exit( EXIT_FAILURE );
        }

        fprintf( stderr, "GLEW version : %s\n", glewGetString(GLEW_VERSION));
	}

	void initGL()
	{
        // Clear the error buffer (it starts with an error).
		GLenum err;
		while ((err = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL error: " << err << std::endl;
		}

        std::printf("%s\n%s\n", 
			glGetString(GL_RENDERER),  // e.g. Intel HD Graphics 3000 OpenGL Engine
			glGetString(GL_VERSION)    // e.g. 3.2 INTEL-8.0.61
        );

    #if _DEBUG
        glEnable(GL_DEBUG_OUTPUT);
        if (glDebugMessageCallback) {
            cout << "Register OpenGL debug callback " << endl;
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
            glDebugMessageCallback(OpenglCallbackFunction, nullptr);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        }
        else
            cout << "glDebugMessageCallback not available" << endl;
    #endif

        glClearColor( 0.15f, 0.15f, 0.15f, 0.0f);

        glEnable( GL_DEPTH_TEST );
        glDepthFunc( GL_LEQUAL );

        glDisable( GL_STENCIL_TEST );
        glClearStencil( 0 );

        glCullFace( GL_BACK );    
        glFrontFace(GL_CCW);

        glDisable( GL_MULTISAMPLE );

        glGenVertexArrays(1, &m_EmptyVAO);
	}

	void initWindow(int argc, char** argv)
	{
		// Initialise GLFW
		if( !glfwInit() )
		{
			fprintf( stderr, "Failed to initialize GLFW\n" );
			exit( EXIT_FAILURE );
		}
		glfwWindowHint(GLFW_SAMPLES, 4);
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		
		window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_NAME, NULL, NULL );
		if ( window == NULL ) {
			fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
			glfwTerminate();
			exit( EXIT_FAILURE );
		}
		glfwMakeContextCurrent(window);
		glfw_reshape_callback( window, WINDOW_WIDTH, WINDOW_HEIGHT );

		// GLFW Events' Callback
		glfwSetWindowSizeCallback( window, glfw_reshape_callback );
		glfwSetKeyCallback( window, glfw_keyboard_callback );
		glfwSetMouseButtonCallback( window, glfw_mouse_callback );
		glfwSetCursorPosCallback( window, glfw_motion_callback );
		glfwSetCharCallback( window, glfw_char_callback );
		glfwSetScrollCallback( window, glfw_scroll_callback );
	}

	void finalizeApp()
	{
		m_pistol->destroy();
		m_orb->destroy();
        glswShutdown();  
        m_programMesh.destroy();
        m_programSky.destroy();
        m_sphere.destroy();
		m_cube.destroy();
		m_triangle.destroy();

        for (int k = 0; k < 5; k++)
            for(int i = 0; i < 4; i++) 
                m_pbrTex[k][i].destroy();
        for(int i = 0; i < 4; i++)
            m_pistolTex[i].destroy();

        Logger::getInstance().close();
		ImGui_ImplGlfwGL3_Shutdown();
		glfwTerminate();
	}

	void mainLoopApp()
	{
		do {
			update();
			render();

			/* Swap front and back buffers */
			glfwSwapBuffers(window);

			/* Poll for and process events */
			glfwPollEvents();
		}
		while (!bCloseApp && glfwWindowShouldClose(window) == 0);
	}

	float sinc(float x) {               /* Supporting sinc function */
		if (fabs(x) < 1.0e-4) return 1.0 ;
		else return(sin(x)/x) ;
	}

    // GLFW Callbacks_________________________________________________  


    void glfw_reshape_callback(GLFWwindow* window, int width, int height)
    {
        glViewport(0, 0, width, height);

        float aspectRatio = ((float)width) / ((float)height);
        camera.setProjectionParams( 45.0f, aspectRatio, 0.1f, 250.0f);
    }

	void update()
	{
        Timer::getInstance().update();
        camera.update();
        // app.update();
		updateHUD();
	}

	void updateHUD()
	{
		ImGui_ImplGlfwGL3_NewFrame();

		auto width = WINDOW_WIDTH, height = WINDOW_HEIGHT;

		// GUI : right menu bar
		ImGui::SetNextWindowPos(
				ImVec2(width - width / 5.0f - 10.0f, 10.f),
				ImGuiSetCond_FirstUseEver);

		ImGui::Begin("Settings",
				NULL,
				ImVec2(width / 5.0f, height - 20.0f),
				ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::PushItemWidth(180.0f);

		ImGui::Text("Environment light:");
		ImGui::Indent();
		ImGui::Checkbox("IBL Diffuse",  &m_settings.m_doDiffuseIbl);
		ImGui::Checkbox("IBL Specular", &m_settings.m_doSpecularIbl);
		{
			float tabWidth = ImGui::GetContentRegionAvailWidth() / 2.0f;
			if (ImGui::TabButton("Bolonga", tabWidth, m_currentLightProbe == LightProbe::Bolonga) )
			{
				m_currentLightProbe = LightProbe::Bolonga;
			}

			ImGui::SameLine(0.0f,0.0f);

			if (ImGui::TabButton("Kyoto", tabWidth, m_currentLightProbe == LightProbe::Kyoto) )
			{
				m_currentLightProbe = LightProbe::Kyoto;
			}
		}
		ImGui::SliderFloat("Texture LOD", &m_settings.m_lod, 0.0f, 10.1f);
		ImGui::Unindent();

		ImGui::Separator();
		ImGui::Text("Directional light:");
		ImGui::Indent();
		ImGui::Checkbox("Diffuse",  &m_settings.m_doDiffuse);
		ImGui::Checkbox("Specular", &m_settings.m_doSpecular);
		const bool doDirectLighting = m_settings.m_doDiffuse || m_settings.m_doSpecular;
		if (doDirectLighting)
		{
			ImGui::SliderFloat("Light direction X", &m_settings.m_lightDir[0], -1.0f, 1.0f);
			ImGui::SliderFloat("Light direction Y", &m_settings.m_lightDir[1], -1.0f, 1.0f);
			ImGui::SliderFloat("Light direction Z", &m_settings.m_lightDir[2], -1.0f, 1.0f);
			ImGui::ColorWheel("Color:", glm::value_ptr(m_settings.m_lightCol), 0.6f);
		}
		ImGui::Unindent();

		ImGui::Separator();
		ImGui::Text("Background:");
		ImGui::Indent();
		{
			int32_t selection;
			if (0.0f == m_settings.m_bgType)
			{
				selection = UINT8_C(0);
			}
			else if (7.0f == m_settings.m_bgType)
			{
				selection = UINT8_C(2);
			}
			else
			{
				selection = UINT8_C(1);
			}

			float tabWidth = ImGui::GetContentRegionAvailWidth() / 3.0f;
			if (ImGui::TabButton("Skybox", tabWidth, selection == 0) )
			{
				selection = 0;
			}

			ImGui::SameLine(0.0f,0.0f);
			if (ImGui::TabButton("Radiance", tabWidth, selection == 1) )
			{
				selection = 1;
			}

			ImGui::SameLine(0.0f,0.0f);
			if (ImGui::TabButton("Irradiance", tabWidth, selection == 2) )
			{
				selection = 2;
			}

			if (0 == selection)
			{
				m_settings.m_bgType = 0.0f;
			}
			else if (2 == selection)
			{
				m_settings.m_bgType = 7.0f;
			}
			else
			{
				m_settings.m_bgType = m_settings.m_radianceSlider;
			}

			const bool isRadiance = (selection == 1);
			if (isRadiance)
			{
				ImGui::SliderFloat("Mip level", &m_settings.m_radianceSlider, 1.0f, 6.0f);
			}
		}
		ImGui::Unindent();

		ImGui::Separator();
		ImGui::Text("Post processing:");
		ImGui::Indent();
		ImGui::SliderFloat("Exposure",& m_settings.m_exposure, -4.0f, 4.0f);
		ImGui::Unindent();

		ImGui::PopItemWidth();
		ImGui::End();

		// GUI : left menu bar
		ImGui::SetNextWindowPos(
				ImVec2(10.0f, 260.0f),
				ImGuiSetCond_FirstUseEver);

		ImGui::Begin("Mesh", 
				NULL, 
				ImVec2(width / 5.0f, 450.f),
				ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::Text("Mesh:");
		ImGui::Indent();
		ImGui::RadioButton("Bunny", &m_settings.m_meshSelection, 0);
		ImGui::RadioButton("Orbs",  &m_settings.m_meshSelection, 1);
		ImGui::Unindent();

		const bool isBunny = (0 == m_settings.m_meshSelection);
		if (!isBunny)
		{
			m_settings.m_metalOrSpec = 0;
		}
		else
		{
			ImGui::Separator();
			ImGui::Text("Workflow:");
			ImGui::Indent();
			ImGui::RadioButton("Metalness", &m_settings.m_metalOrSpec, 0);
			ImGui::RadioButton("Specular", &m_settings.m_metalOrSpec, 1);
			ImGui::Unindent();

			ImGui::Separator();
			ImGui::Text("Material:");
			ImGui::Indent();
			ImGui::PushItemWidth(130.0f);
			ImGui::SliderFloat("Glossiness", &m_settings.m_glossiness, 0.0f, 1.0f);
			ImGui::SliderFloat(0 == m_settings.m_metalOrSpec ? "Metalness" : "Diffuse - Specular", &m_settings.m_reflectivity, 0.0f, 1.0f);
			ImGui::PopItemWidth();
			ImGui::Unindent();
		}

		ImGui::ColorWheel("Diffuse:", glm::value_ptr(m_settings.m_rgbDiff), 0.7f);
		ImGui::Separator();
		if ( (1 == m_settings.m_metalOrSpec) && isBunny )
		{
			ImGui::ColorWheel("Specular:", glm::value_ptr(m_settings.m_rgbSpec), 0.7f);
		}
		ImGui::End();
	}

	void prepareRender()
	{
		// 1. setup framebuffer
		unsigned int captureFBO;
		unsigned int captureRBO;

		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);

		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

		// 2. load the HDR environment map
        BaseTexture newportTex;
        if (!newportTex.create("resource/newport_loft.hdr"))
        {
            fprintf(stderr, "fail to load texture");
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
		newportTex.parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		// 3. setup cubemap to render to and attach to framebuffer
		envCubemap.create(512, 512, GL_TEXTURE_CUBE_MAP, GL_RGB16F, 7);
		envCubemap.parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		// 4. set up projection and view matrices for capturing data onto the 6 cubemap face directions
		glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		glm::mat4 captureViews[] =
		{
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};

		// 5. convert HDR equirectangular environment map to cubemap equivalent
		ProgramShader equirectangularToCubemapShader;
		equirectangularToCubemapShader.initalize();
        equirectangularToCubemapShader.addShader(GL_VERTEX_SHADER, "Cubemap.Vertex");
        equirectangularToCubemapShader.addShader(GL_FRAGMENT_SHADER, "EquirectangularToCubemap.Fragment");
        equirectangularToCubemapShader.link();  
		equirectangularToCubemapShader.bind();
		equirectangularToCubemapShader.setUniform("equirectangularMap", 0);
		equirectangularToCubemapShader.setUniform("projection", captureProjection);

        newportTex.bind(0);

		glViewport(0, 0, 512, 512); // don't forget to configure the viewport to the capture dimensions.
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		for (unsigned int i = 0; i < 6; ++i)
		{
			equirectangularToCubemapShader.setUniform("view", captureViews[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap.m_TextureID, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			m_cube.draw();
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // then let OpenGL generate mipmaps from first mip face (combatting visible dots artifact)
		envCubemap.parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		envCubemap.generateMipmap();

        // 6. create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
        uint32_t irradianceWidth = 32, irradianceHeight = 32;
		irradianceCubemap.create(irradianceWidth, irradianceHeight, GL_TEXTURE_CUBE_MAP, GL_RGB16F, 1);
		irradianceCubemap.parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR); 

		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, irradianceWidth, irradianceHeight);

        // 7. solve diffuse integral by convolution to create an irradiance cbuemap
        ProgramShader programIrradiance;
        programIrradiance.initalize();
        programIrradiance.addShader(GL_VERTEX_SHADER, "Cubemap.Vertex");
        programIrradiance.addShader(GL_FRAGMENT_SHADER, "Irradiance.Fragment");
        programIrradiance.link();  
		programIrradiance.bind();
		programIrradiance.setUniform("environmentCube", 0);
		programIrradiance.setUniform("projection", captureProjection);

		envCubemap.bind(0);

        glViewport(0, 0, irradianceWidth, irradianceHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        {
            PROFILEGL("Irradiance cubemap");
            for (int i = 0; i < 6; i++)
            {
                programIrradiance.setUniform("view", captureViews[i]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceCubemap.m_TextureID, 0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                m_cube.draw();
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 8. create a prefilter cubemap and allocate mips
        GLsizei prefilterSize = 128;
		prefilterCubemap.create(prefilterSize, prefilterSize, GL_TEXTURE_CUBE_MAP, GL_RGB16F, 7);
		prefilterCubemap.parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        // 9. run a quasi monte-carlo simulation on the environment lighting to create a prefilter cubemap
        ProgramShader programPrefilter;
        programPrefilter.initalize();
        programPrefilter.addShader(GL_VERTEX_SHADER, "Cubemap.Vertex");
        programPrefilter.addShader(GL_FRAGMENT_SHADER, "Prefilter.Fragment");
        programPrefilter.link();  
		programPrefilter.bind();
		programPrefilter.setUniform("environmentCube", 0);
		programPrefilter.setUniform("projection", captureProjection);

		envCubemap.bind(0);

        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        int maxMipLevels = 5;
        for (int mip = 0; mip < maxMipLevels; mip++)
        {
            PROFILEGL("Prefilter cubemap");
            // resize render buffer to prefilter scale
            GLsizei width = prefilterSize * std::pow(0.5, mip);
            GLsizei height = prefilterSize * std::pow(0.5, mip);

            glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
            glViewport(0, 0, width, height);

            float roughness = float(mip) / float(maxMipLevels - 1);
            programPrefilter.setUniform("uRoughness", roughness);
            // solve quasi monte-carlo integral for each face and a given roughness
            for (int i = 0; i < 6; i++)
            {
                programPrefilter.setUniform("view", captureViews[i]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterCubemap.m_TextureID, mip);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                m_cube.draw();
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 10. Generate a 2D LUT from the BRDF quation used.
    	brdfTexture.create(512, 512, GL_TEXTURE_2D, GL_RG16F, 1);
        // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
		brdfTexture.parameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        brdfTexture.parameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        brdfTexture.parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        brdfTexture.parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // rescale capture framebuffer to brdf texture
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfTexture.m_TextureID, 0);

        ProgramShader programBrdf;
        programBrdf.initalize();
        programBrdf.addShader(GL_VERTEX_SHADER, "Brdf.Vertex");
        programBrdf.addShader(GL_FRAGMENT_SHADER, "Brdf.Fragment");
        programBrdf.link();  
		programBrdf.bind();
        glViewport(0, 0, 512, 512); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        m_triangle.draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

    void render()
    {    
        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0, 0, 0, 0);
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );    
        glPolygonMode(GL_FRONT_AND_BACK, (bWireframe)? GL_LINE : GL_FILL);

		// Submit view 0.
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );  
		glDisable( GL_CULL_FACE );  
		glEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );  
		glm::mat4 followCamera = glm::translate( glm::mat4(1.0f), camera.getPosition() );
		glm::mat4 skyboxMtx = camera.getViewProjMatrix() * followCamera;
		m_programSky.bind();

		// Texture binding
		envCubemap.bind(0);
		irradianceCubemap.bind(1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterCubemap.m_TextureID);

		m_programSky.setUniform( "uEnvmap", 0 );
		m_programSky.setUniform( "uEnvmapIrr", 1 );
		m_programSky.setUniform( "uEnvmapPrefilter", 2 );

		// Uniform binding
        m_programSky.setUniform( "uViewMatrix", camera.getViewMatrix() );
        m_programSky.setUniform( "uProjMatrix", camera.getProjectionMatrix() );
		m_programSky.setUniform( "uBgType", m_settings.m_bgType );
		m_programSky.setUniform( "uExposure", m_settings.m_exposure );
		m_cube.draw();
		m_programSky.unbind();
		glDisable( GL_TEXTURE_CUBE_MAP_SEAMLESS );
		glEnable( GL_CULL_FACE );
		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST ); 

		// Sumbit view 1.
		irradianceCubemap.bind(4);
		prefilterCubemap.bind(5);
		brdfTexture.bind(6);

        m_programMesh.bind();
		// Uniform binding
        m_programMesh.setUniform( "uModelViewProjMatrix", camera.getViewProjMatrix() );
		m_programMesh.setUniform( "uEyePosWS", camera.getPosition());
		m_programMesh.setUniform( "uExposure", m_settings.m_exposure );
		m_programMesh.setUniform( "ubDiffuse", float(m_settings.m_doDiffuse) );
		m_programMesh.setUniform( "ubSpecular", float(m_settings.m_doSpecular) );
		m_programMesh.setUniform( "ubDiffuseIbl", float(m_settings.m_doDiffuseIbl) );
		m_programMesh.setUniform( "ubSpecularIbl", float(m_settings.m_doSpecularIbl) );
		m_programMesh.setUniform( "uMtxSrt", glm::mat4(1) );
		for (unsigned int i = 0; i < 4; i++) {
			std::string idx = "[" + std::to_string(i) + "]";
			m_programMesh.setUniform("uLightPositions" + idx, lightPositions[i]);
			m_programMesh.setUniform("uLightColors" + idx, lightColors[i]);
		}

		// Texture binding
		m_programMesh.setUniform( "uEnvmapIrr", 4 );
		m_programMesh.setUniform( "uEnvmapPrefilter", 5 );
		m_programMesh.setUniform( "uEnvmapBrdfLUT", 6 );
		m_programMesh.setUniform( "uAlbedoMap", 0 );
		m_programMesh.setUniform( "uNormalMap", 1 );
		m_programMesh.setUniform( "uMetallicMap", 2 );
		m_programMesh.setUniform( "uRoughnessMap", 3 );

		if (0 == m_settings.m_meshSelection)
		{
            glm::mat4 mtxS = glm::scale(glm::mat4(1), glm::vec3(1.f/10));
            m_programMesh.setUniform("uMtxSrt", mtxS);
            for(int i = 0; i < 4; i++)
                m_pistolTex[i].bind(i);
			m_pistol->render();
		}
		else
		{
			// Submit orbs.
            for(float xx = 0, xend = 5.0f; xx < xend; xx += 1.0f)
            {
                for(int i = 0; i < 4; i++) 
                    m_pbrTex[uint32_t(xx)][i].bind(i);

                const float scale = 1.2f;
                const float spacing = 2.2f * 30;
                const float yAdj = -0.8f;
                glm::vec3 translate(0.0f + (xx / xend)*spacing - (1.0f + (scale - 1.0f)*0.5f - 1.0f / xend), 0.0f, 0.0f);
                glm::mat4 mtxS = glm::scale(glm::mat4(1), glm::vec3(scale / xend));
                glm::mat4 mtxST = glm::translate(mtxS, translate);
                m_programMesh.setUniform("uMtxSrt", mtxST);
                m_sphere.draw();
            }
		}
        m_programMesh.unbind();
		renderHUD();
    }

	void renderHUD()
	{
		// restore some state
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        ImGui::Render();
	}

    void glfw_keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods) 
	{
		ImGui_ImplGlfwGL3_KeyCallback(window, key, scancode, action, mods);

		moveCamera( key, action != GLFW_RELEASE);
		bool bPressed = action == GLFW_PRESS;
		if (bPressed) {
			switch (key)
			{
				case GLFW_KEY_ESCAPE:
					bCloseApp = true;

				case GLFW_KEY_W:
					bWireframe = !bWireframe;
					break;

				case GLFW_KEY_T:
					{
						Timer &timer = Timer::getInstance();
						printf( "fps : %d [%.3f ms]\n", timer.getFPS(), timer.getElapsedTime());
					}
					break;

				case GLFW_KEY_R:
					break;

				default:
					break;
			}
		}
    }

    void moveCamera( int key, bool isPressed )
    {
        switch (key)
        {
		case GLFW_KEY_UP:
			camera.keyboardHandler( MOVE_FORWARD, isPressed);
			break;

		case GLFW_KEY_DOWN:
			camera.keyboardHandler( MOVE_BACKWARD, isPressed);
			break;

		case GLFW_KEY_LEFT:
			camera.keyboardHandler( MOVE_LEFT, isPressed);
			break;

		case GLFW_KEY_RIGHT:
			camera.keyboardHandler( MOVE_RIGHT, isPressed);
			break;
        }
    }

    void glfw_motion_callback(GLFWwindow* window, double xpos, double ypos)
    {
		int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
		const bool bPressed = (state == GLFW_PRESS);
		const bool mouseOverGui = ImGui::MouseOverArea();
		if (!mouseOverGui && bPressed) camera.motionHandler( int(xpos), int(ypos), false);    
    }  

    void glfw_mouse_callback(GLFWwindow* window, int button, int action, int mods)
    {
        ImGui_ImplGlfwGL3_MouseButtonCallback(window, button, action, mods);

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) { 
			double xpos, ypos;
			glfwGetCursorPos(window, &xpos, &ypos);
			const bool mouseOverGui = ImGui::MouseOverArea();
			if (!mouseOverGui)
				camera.motionHandler( xpos, ypos, true); 
		}    
    }


	void glfw_char_callback(GLFWwindow* windows, unsigned int c)
	{
		ImGui_ImplGlfwGL3_CharCallback(windows, c);
	}

	void glfw_scroll_callback(GLFWwindow* windows, double xoffset, double yoffset)
	{
		ImGui_ImplGlfwGL3_ScrollCallback(windows, xoffset, yoffset);
	}
}

int main(int argc, char** argv)
{
	initialize(argc, argv);
	mainLoopApp();
	finalizeApp();
    return EXIT_SUCCESS;
}
