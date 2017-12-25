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

#include <tools/TCamera.hpp>
#include <tools/Timer.hpp>
#include <tools/Logger.hpp>
#include <tools/gltools.hpp>

#include <GLType/ProgramShader.h>
#include <GLType/Texture.h>
#include <SkyBox.h>
#include <Skydome.h>
#include <Mesh.h>
#include <ModelAssImp.h>

#include <fstream>
#include <memory>
#include <vector>
#include <algorithm>

struct LightProbe
{
	enum Enum
	{
		Bolonga,
		Kyoto,

		Count
	};
};

struct Settings
{
	Settings()
	{
		m_envRotCurr = 0.0f;
		m_envRotDest = 0.0f;
		m_lightDir[0] = -0.8f;
		m_lightDir[1] = 0.2f;
		m_lightDir[2] = -0.5f;
		m_lightCol[0] = 1.0f;
		m_lightCol[1] = 1.0f;
		m_lightCol[2] = 1.0f;
		m_glossiness = 0.7f;
		m_exposure = 0.0f;
		m_bgType = 3.0f;
		m_radianceSlider = 2.0f;
		m_reflectivity = 0.85f;
		m_rgbDiff[0] = 1.0f;
		m_rgbDiff[1] = 1.0f;
		m_rgbDiff[2] = 1.0f;
		m_rgbSpec[0] = 1.0f;
		m_rgbSpec[1] = 1.0f;
		m_rgbSpec[2] = 1.0f;
		m_lod = 0.0f;
		m_doDiffuse = false;
		m_doSpecular = false;
		m_doDiffuseIbl = true;
		m_doSpecularIbl = true;
		m_showLightColorWheel = true;
		m_showDiffColorWheel = true;
		m_showSpecColorWheel = true;
		m_metalOrSpec = 0;
		m_meshSelection = 0;
	}

	float m_envRotCurr;
	float m_envRotDest;
	float m_lightDir[3];
	float m_lightCol[3];
	float m_glossiness;
	float m_exposure;
	float m_radianceSlider;
	float m_bgType;
	float m_reflectivity;
	float m_rgbDiff[3];
	float m_rgbSpec[3];
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
    const unsigned int WINDOW_HEIGHT = 960;
	const char* WINDOW_NAME = "Irradiance Environment Mapping";

	bool bCloseApp = false;
	GLFWwindow* window = nullptr;  

    ProgramShader m_program;
	std::shared_ptr<Texture2D> m_texture;
    SphereMesh m_mesh( 48, 5.0f);
	SkyBox m_skybox;
	Skydome m_skydome;
	Settings m_settings;
	ModelPtr m_bunny;

	LightProbe::Enum m_currentLightProbe;

    //?

    TCamera camera;

    bool bWireframe = false;

    //?

	void initApp(int argc, char** argv);
	void initExtension();
	void initGL();
	void initWindow(int argc, char** argv);
    void finalizeApp();
	void mainLoopApp();
    void moveCamera( int key, bool isPressed );
	void handleInput();
    void handleKeyboard(float delta);
	void prefilter(fRGB* im, int width, int height);
	void printcoeffs();
	void tomatrix();
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
# define eTB_CriticalBreakPoint() if (IsDebuggerPresent ()) __debugbreak ();
#else
# define eTB_CriticalBreakPoint() asm (" int $3");
#endif

namespace {
	using namespace std;

	typedef unsigned long DWORD;
	const char* ETB_GL_DEBUG_SOURCE_STR (GLenum source)
	{
		static const char* sources [] = {
			"API",   "Window System", "Shader Compiler", "Third Party", "Application",
			"Other", "Unknown"
		};

		int str_idx = min ( size_t(source - GL_DEBUG_SOURCE_API), sizeof (sources) / sizeof (const char *) );

		return sources [str_idx];
	}

	const char* ETB_GL_DEBUG_TYPE_STR (GLenum type)
	{
		static const char* types [] = {
			"Error",       "Deprecated Behavior", "Undefined Behavior", "Portability",
			"Performance", "Other",               "Unknown"
		};

		int str_idx = min ( (size_t)(type - GL_DEBUG_TYPE_ERROR), sizeof (types) / sizeof (const char *) );

		return types [str_idx];
	}

	const char* ETB_GL_DEBUG_SEVERITY_STR (GLenum severity)
	{
		static const char* severities [] = {
			"High", "Medium", "Low", "Unknown"
		};
		int str_idx = min ( (size_t)(severity - GL_DEBUG_SEVERITY_HIGH), sizeof(severities)/sizeof(const char *));
		return severities [str_idx];
	}

	void ETB_GL_ERROR_CALLBACK (
			GLenum        source,
			GLenum        type,
			GLuint        id,
			GLenum        severity,
			GLsizei       length,
			const GLchar* message,
			GLvoid*       userParam)
	{
		printf("OpenGL Error:\n");
		printf("=============\n");
		printf(" Object ID: ");
		printf("%d\n", id);
		printf(" Severity:  ");
		printf("%s\n", ETB_GL_DEBUG_SEVERITY_STR(severity));

		printf(" Type:      ");
		printf("%s\n", ETB_GL_DEBUG_TYPE_STR     (type));

		printf(" Source:    ");
		printf("%s\n", ETB_GL_DEBUG_SOURCE_STR   (source));

		printf(" Message:   ");
		printf("%s\n\n", message);

		// Force the console to flush its contents before executing a breakpoint
		fflush(stdout);

		// Trigger a breakpoint in gDEBugger...
		glFinish ();

		// Trigger a breakpoint in traditional debuggers...
		eTB_CriticalBreakPoint ();
	}

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


	void initApp(int argc, char** argv)
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
		//ImGuiIO& io = ImGui::GetIO();
		//io.Fonts->AddFontDefault();
		//io.Fonts->AddFontFromFileTTF("../../extra_fonts/Cousine-Regular.ttf", 15.0f);
		//io.Fonts->AddFontFromFileTTF("../../extra_fonts/DroidSans.ttf", 16.0f);
		//io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyClean.ttf", 13.0f);
		//io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyTiny.ttf", 10.0f);
		//io.Fonts->AddFontFromFileTTF("c:??Windows??Fonts??ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

		// GLSW : shader file manager
		glswInit();
		glswSetPath("./shaders/", ".glsl");
		glswAddDirectiveToken("*", "#version 330 core");

        // App Objects
        camera.setViewParams( glm::vec3( 0.0f, 2.0f, 15.0f), glm::vec3( 0.0f, 0.0f, 0.0f) );
        camera.setMoveCoefficient(0.35f);

        Timer::getInstance().start();

        m_program.initalize();
        m_program.addShader( GL_VERTEX_SHADER, "Default.Vertex");
        m_program.addShader( GL_FRAGMENT_SHADER, "Default.Fragment");
        m_program.link();  

        GLuint m_VertexArrayID;
        GL_ASSERT(glGenVertexArrays(1, &m_VertexArrayID));
        GL_ASSERT(glBindVertexArray(m_VertexArrayID));

        m_mesh.init();

		m_texture = std::make_shared<Texture2D>();
        m_texture->initialize();

		m_skybox.init();
		m_skybox.addCubemap( "resource/Cube/*.bmp" );
	#if !_DEBUG
		// m_skybox.addCubemap( "resource/MountainPath/*.jpg" );
	#endif
		m_skybox.setCubemap( 0u );

		m_bunny = std::make_shared<ModelAssImp>();
		m_bunny->loadFromFile( "resource/Meshes/bunny.obj" );
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

		// SUPER VERBOSE DEBUGGING!
    #if 0
		if (glDebugMessageControlARB != NULL) {
			glEnable                  (GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
			glDebugMessageControlARB  (GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
			glDebugMessageCallbackARB ((GLDEBUGPROCARB)ETB_GL_ERROR_CALLBACK, NULL);
		}
    #endif
		GLint flags; 
		glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
		if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
		{
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
			glDebugMessageCallback((GLDEBUGPROCARB)ETB_GL_ERROR_CALLBACK, NULL);
		}

        glClearColor( 0.15f, 0.15f, 0.15f, 0.0f);

        glEnable( GL_DEPTH_TEST );
        glDepthFunc( GL_LEQUAL );

        glDisable( GL_STENCIL_TEST );
        glClearStencil( 0 );

        glCullFace( GL_BACK );    
        glFrontFace(GL_CCW);

        glDisable( GL_MULTISAMPLE );
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
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
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
		m_bunny->destroy();
        glswShutdown();  
        m_program.destroy();
        m_mesh.destroy();
        m_texture->destroy();
		m_skydome.shutdown();
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
			ImGui::ColorWheel("Color:", m_settings.m_lightCol, 0.6f);
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

		ImGui::ColorWheel("Diffuse:", &m_settings.m_rgbDiff[0], 0.7f);
		ImGui::Separator();
		if ( (1 == m_settings.m_metalOrSpec) && isBunny )
		{
			ImGui::ColorWheel("Specular:", &m_settings.m_rgbSpec[0], 0.7f);
		}
		ImGui::End();
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

		m_skybox.render( camera );
		// m_skydome.render( camera );

        // Use our shader
        m_program.bind();

        glm::mat4 modelMatrix = m_mesh.getModelMatrix();
        glm::mat4 mvp = camera.getViewProjMatrix() * modelMatrix;
        m_program.setUniform( "uModelViewProjMatrix", mvp );

		// Vertex uniforms
		m_program.setUniform( "uModelMatrix", modelMatrix);
		m_program.setUniform( "uNormalMatrix", m_mesh.getNormalMatrix());
		m_program.setUniform( "uEyePosWS", camera.getPosition());
		m_program.setUniform( "uInvSkyboxRotation", m_skybox.getInvRotateMatrix() );
		TextureCubemap *cubemap = m_skybox.getCurrentCubemap();

		if (cubemap->hasSphericalHarmonics())
		{
			glm::mat4* matrix = cubemap->getSHMatrices();
			m_program.setUniform( "uIrradianceMatrix[0]", matrix[0]);
			m_program.setUniform( "uIrradianceMatrix[1]", matrix[1]);
			m_program.setUniform( "uIrradianceMatrix[2]", matrix[2]);
		}

        cubemap->bind(0u);
        m_mesh.draw();

        mvp = camera.getViewProjMatrix() * modelMatrix;
		m_program.setUniform( "uModelMatrix", modelMatrix);
        m_program.setUniform( "uModelViewProjMatrix", mvp );
		m_bunny->render();

        cubemap->unbind(0u);
        m_program.unbind();

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
					m_skybox.toggleAutoRotate();
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
		if (!mouseOverGui && bPressed) camera.motionHandler( xpos, ypos, false);    
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
	initApp(argc, argv);
	mainLoopApp();
	finalizeApp();
    return EXIT_SUCCESS;
}
