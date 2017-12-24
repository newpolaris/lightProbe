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

#include <fstream>
#include <memory>

#define GL_ASSERT(x) {x; CHECKGLERROR()}

namespace ImGui
{
	inline bool MouseOverArea()
	{
		return false
			|| ImGui::IsAnyItemHovered()
			|| ImGui::IsMouseHoveringAnyWindow()
			;
	}
}

namespace
{
	struct fRGB {
		float r, g, b; 
	};
    const unsigned int WINDOW_WIDTH = 800u;
    const unsigned int WINDOW_HEIGHT = 600u;
	const char* WINDOW_NAME = "Irradiance Environment Mapping";

	bool bCloseApp = false;
	GLFWwindow* window = nullptr;  

    ProgramShader m_program;
	std::shared_ptr<Texture2D> m_texture;
    SphereMesh m_mesh( 48, 5.0f);
	SkyBox m_skybox;
	Skydome m_skydome;

	float coeffs[9][3] ;                /* Spherical harmonic coefficients */
	glm::mat4 matrix[3] ;               /* Matrix for quadratic form */

    //~

    TCamera camera;

    // App app;

    bool bWireframe = false;
    bool bFullscreen = false;

    //~

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
    void renderScene();
	void updatecoeffs(float hdr[3], float domega, float x, float y, float z);

    void glfw_keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void glfw_reshape_callback(GLFWwindow* window, int width, int height);
    void glfw_mouse_callback(GLFWwindow* window, int button, int action, int mods);
    void glfw_motion_callback(GLFWwindow* window, double xpos, double ypos);
	void glfw_char_callback(GLFWwindow* windows, unsigned int c);
	void glfw_scroll_callback(GLFWwindow* windows, double xoffset, double yoffset);
}

namespace {

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

		ImGui_ImplGlfwGL3_Init(window, false);

		// Load FontsR
		// (there is a default font, this is only if you want to change it. see extra_fonts/README.txt for more details)
		//ImGuiIO& io = ImGui::GetIO();
		//io.Fonts->AddFontDefault();
		//io.Fonts->AddFontFromFileTTF("../../extra_fonts/Cousine-Regular.ttf", 15.0f);
		//io.Fonts->AddFontFromFileTTF("../../extra_fonts/DroidSans.ttf", 16.0f);
		//io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyClean.ttf", 13.0f);
		//io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyTiny.ttf", 10.0f);
		//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

		// GLSW : shader file manager
		glswInit();
		glswSetPath("./shaders/", ".glsl");
		glswAddDirectiveToken("*", "#version 330 core");

        // App Objects
        camera.setViewParams( glm::vec3( 0.0f, 2.0f, 15.0f), glm::vec3( 0.0f, 0.0f, 0.0f) );
        camera.setMoveCoefficient(0.35f);

        Timer::getInstance().start();
        // Logger::getInstance().open("logfile");

        // app.init( &camera ); 

        GLuint VertexArrayID;
        GL_ASSERT(glGenVertexArrays(1, &VertexArrayID));
        GL_ASSERT(glBindVertexArray(VertexArrayID));

        m_program.initalize();
        m_program.addShader( GL_VERTEX_SHADER, "Default.Vertex");
        m_program.addShader( GL_FRAGMENT_SHADER, "Default.Fragment");
        m_program.link();  

        m_mesh.init();

		m_texture = std::make_shared<Texture2D>();
        m_texture->initialize();
#if 0
        m_texture->load("resource/skydome/021.jpg");
#else
		const int width = 1000;
		ByteArray image = ReadFileSync( "resource/grace_probe.float" );
		if (image.size() > 0) {
			prefilter(reinterpret_cast<fRGB*>(image.data()), width, width);
			tomatrix();
			// printcoeffs();
			m_texture->load(GL_RGB, width, width, GL_RGB, GL_FLOAT, image.data() );
		}
#endif
		m_skybox.init();
		// m_skybox.addCubemap( "resource/MountainPath/*.jpg" );
		m_skybox.addCubemap( "resource/Cube/*.bmp" );
		m_skybox.setCubemap( 0u );
		
		// m_skydome.initialize();
		// m_skydome.setTexture( m_texture );
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

        glClearColor( 0.15f, 0.15f, 0.15f, 0.0f);

        glEnable( GL_DEPTH_TEST );
        glDepthFunc( GL_LEQUAL );

        glDisable( GL_STENCIL_TEST );
        glClearStencil( 0 );

        glDisable( GL_CULL_FACE );
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
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		
		window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_NAME, NULL, NULL );
		if( window == NULL ){
			fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
			glfwTerminate();
			exit( EXIT_FAILURE );
		}
		glfwMakeContextCurrent(window);

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
			renderScene();

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

	// eq (10)
	void prefilter(fRGB* im, int width, int height)
	{
		/* The main integration routine.  Of course, there are better ways
		   to do quadrature but this suffices.  Calls updatecoeffs to
		   actually increment the integral. Width is the size of the
		   environment map */
		const float PI = 3.141529f;

		int i,j ;
		for (i = 0 ; i < height; i++) {
			for (j = 0 ; j < width ; j++) {

				/* We now find the cartesian components for the point (i,j) */
				float u,v,r,theta,phi,x,y,z,domega ;

				// from image 'http://www.pauldebevec.com/Probes/'
				v = (height/2.0 - i)/(height/2.0);  /* v ranges from -1 to 1 */
				u = (j-width/2.0)/(width/2.0);      /* u ranges from -1 to 1 */
				r = sqrt(u*u+v*v) ;                 /* The "radius" */
				if (r > 1.0) continue ;             /* Consider only circle with r<1 */

				theta = PI*r ;                      /* theta parameter of (i,j) */
				phi = atan2(v,u) ;                  /* phi parameter */

				x = sin(theta)*cos(phi) ;           /* Cartesian components */
				y = sin(theta)*sin(phi) ;
				z = cos(theta) ;

				/* Computation of the solid angle.  This follows from some
				   elementary calculus converting eq(10)'s sin(theta) d theta d phi into
				   coordinates in terms of r.  This calculation should be redone 
				   if the form of the input changes */

				domega = (2*PI/height)*(2*PI/width)*sinc(theta) ;

				updatecoeffs((float*)(im + width*i + j),domega,x,y,z) ; /* Update Integration */
			}
		}
	}

	void printcoeffs()
	{
		int i, j;
		/* Output Results */

		printf("\n         Lighting Coefficients\n\n") ;
		printf("(l,m)       RED        GREEN     BLUE\n") ;

		printf("L_{0,0}   %9.6f %9.6f %9.6f\n",
				coeffs[0][0],coeffs[0][1],coeffs[0][2]) ;
		printf("L_{1,-1}  %9.6f %9.6f %9.6f\n",
				coeffs[1][0],coeffs[1][1],coeffs[1][2]) ;
		printf("L_{1,0}   %9.6f %9.6f %9.6f\n",
				coeffs[2][0],coeffs[2][1],coeffs[2][2]) ;
		printf("L_{1,1}   %9.6f %9.6f %9.6f\n",
				coeffs[3][0],coeffs[3][1],coeffs[3][2]) ;
		printf("L_{2,-2}  %9.6f %9.6f %9.6f\n",
				coeffs[4][0],coeffs[4][1],coeffs[4][2]) ;
		printf("L_{2,-1}  %9.6f %9.6f %9.6f\n",
				coeffs[5][0],coeffs[5][1],coeffs[5][2]) ;
		printf("L_{2,0}   %9.6f %9.6f %9.6f\n",
				coeffs[6][0],coeffs[6][1],coeffs[6][2]) ;
		printf("L_{2,1}   %9.6f %9.6f %9.6f\n",
				coeffs[7][0],coeffs[7][1],coeffs[7][2]) ;
		printf("L_{2,2}   %9.6f %9.6f %9.6f\n",
				coeffs[8][0],coeffs[8][1],coeffs[8][2]) ;

		printf("\nMATRIX M: RED\n") ;
		for (i = 0 ; i < 4 ; i++) {
			for (j = 0 ; j < 4 ; j++)
				printf("%9.6f ",matrix[0][i][j]) ;
			printf("\n") ;
		}
		printf("\nMATRIX M: GREEN\n") ;
		for (i = 0 ; i < 4 ; i++) {
			for (j = 0 ; j < 4 ; j++)
				printf("%9.6f ",matrix[1][i][j]) ;
			printf("\n") ;
		}
		printf("\nMATRIX M: BLUE\n") ;
		for (i = 0 ; i < 4 ; i++) {
			for (j = 0 ; j < 4 ; j++)
				printf("%9.6f ",matrix[2][i][j]) ;
			printf("\n") ;
		}

	}

	void tomatrix(void) {

		/* Form the quadratic form matrix (see equations 11 and 12 in paper) */

		int col ;
		float c1,c2,c3,c4,c5 ;
		c1 = 0.429043 ; c2 = 0.511664 ;
		c3 = 0.743125 ; c4 = 0.886227 ; c5 = 0.247708 ;

		for (col = 0 ; col < 3 ; col++) { /* Equation 12 */

			matrix[col][0][0] = c1*coeffs[8][col] ; /* c1 L_{22}  */
			matrix[col][0][1] = c1*coeffs[4][col] ; /* c1 L_{2-2} */
			matrix[col][0][2] = c1*coeffs[7][col] ; /* c1 L_{21}  */
			matrix[col][0][3] = c2*coeffs[3][col] ; /* c2 L_{11}  */

			matrix[col][1][0] = c1*coeffs[4][col] ; /* c1 L_{2-2} */
			matrix[col][1][1] = -c1*coeffs[8][col]; /*-c1 L_{22}  */
			matrix[col][1][2] = c1*coeffs[5][col] ; /* c1 L_{2-1} */
			matrix[col][1][3] = c2*coeffs[1][col] ; /* c2 L_{1-1} */

			matrix[col][2][0] = c1*coeffs[7][col] ; /* c1 L_{21}  */
			matrix[col][2][1] = c1*coeffs[5][col] ; /* c1 L_{2-1} */
			matrix[col][2][2] = c3*coeffs[6][col] ; /* c3 L_{20}  */
			matrix[col][2][3] = c2*coeffs[2][col] ; /* c2 L_{10}  */

			matrix[col][3][0] = c2*coeffs[3][col] ; /* c2 L_{11}  */
			matrix[col][3][1] = c2*coeffs[1][col] ; /* c2 L_{1-1} */
			matrix[col][3][2] = c2*coeffs[2][col] ; /* c2 L_{10}  */
			matrix[col][3][3] = c4*coeffs[0][col] - c5*coeffs[6][col] ; 
			/* c4 L_{00} - c5 L_{20} */
		}
	}


	void updatecoeffs(float hdr[3], float domega, float x, float y, float z) 
	{ 

		/****************************************************************** 
		  Update the coefficients (i.e. compute the next term in the
		  integral) based on the lighting value hdr[3], the differential
		  solid angle domega and cartesian components of surface normal x,y,z

		  Inputs:  hdr = L(x,y,z) [note that x^2+y^2+z^2 = 1]
		  i.e. the illumination at position (x,y,z)

		  domega = The solid angle at the pixel corresponding to 
		  (x,y,z).  For these light probes, this is given by 

		  x,y,z  = Cartesian components of surface normal

		  Notes:   Of course, there are better numerical methods to do
		  integration, but this naive approach is sufficient for our
		  purpose.

		 *********************************************************************/

		int col ;
		for (col = 0 ; col < 3 ; col++) {
			float c ; /* A different constant for each coefficient */

			/* L_{00}.  Note that Y_{00} = 0.282095 */
			c = 0.282095 ;
			coeffs[0][col] += hdr[col]*c*domega ;

			/* L_{1m}. -1 <= m <= 1.  The linear terms */
			c = 0.488603 ;
			coeffs[1][col] += hdr[col]*(c*y)*domega ;   /* Y_{1-1} = 0.488603 y  */
			coeffs[2][col] += hdr[col]*(c*z)*domega ;   /* Y_{10}  = 0.488603 z  */
			coeffs[3][col] += hdr[col]*(c*x)*domega ;   /* Y_{11}  = 0.488603 x  */

			/* The Quadratic terms, L_{2m} -2 <= m <= 2 */

			/* First, L_{2-2}, L_{2-1}, L_{21} corresponding to xy,yz,xz */
			c = 1.092548 ;
			coeffs[4][col] += hdr[col]*(c*x*y)*domega ; /* Y_{2-2} = 1.092548 xy */ 
			coeffs[5][col] += hdr[col]*(c*y*z)*domega ; /* Y_{2-1} = 1.092548 yz */ 
			coeffs[7][col] += hdr[col]*(c*x*z)*domega ; /* Y_{21}  = 1.092548 xz */ 

			/* L_{20}.  Note that Y_{20} = 0.315392 (3z^2 - 1) */
			c = 0.315392 ;
			coeffs[6][col] += hdr[col]*(c*(3*z*z-1))*domega ; 

			/* L_{22}.  Note that Y_{22} = 0.546274 (x^2 - y^2) */
			c = 0.546274 ;
			coeffs[8][col] += hdr[col]*(c*(x*x-y*y))*domega ;

		}
	}

    // GLFW Callbacks_________________________________________________  


    void glfw_reshape_callback(GLFWwindow* window, int width, int height)
    {
        glViewport( 0, 0, width, height);

        float aspectRatio = ((float)width) / ((float)height);
        camera.setProjectionParams( 60.0f, aspectRatio, 0.1f, 250.0f);
    }

    void renderScene()
    {    
        Timer::getInstance().update();
        camera.update();
        // app.update();

        ImGui_ImplGlfwGL3_NewFrame();

		bool show_test_window = true;
		bool show_another_window = false;
		ImVec4 clear_color = ImColor(114, 144, 154);

		// 1. Show a simple window
		// Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
		{
			static float f = 0.0f;
			ImGui::Text("Hello, world!");
			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
			ImGui::ColorEdit3("clear color", (float*)&clear_color);
			if (ImGui::Button("Test Window")) show_test_window ^= 1;
			if (ImGui::Button("Another Window")) show_another_window ^= 1;
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		}

        // 2. Show another simple window, this time using an explicit Begin/End pair
        if (show_another_window)
        {
            ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello");
            ImGui::End();
        }

        // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
        if (show_test_window)
        {
            ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
            ImGui::ShowTestWindow(&show_test_window);
        }

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        glPolygonMode(GL_FRONT_AND_BACK, (bWireframe)? GL_LINE : GL_FILL);
		m_skybox.render( camera );
		// m_skydome.render( camera );

        // Use our shader
        m_program.bind();

        glm::mat4 mvp = camera.getViewProjMatrix() * m_mesh.getModelMatrix();
        m_program.setUniform( "uModelViewProjMatrix", mvp );

		// Vertex uniforms
		m_program.setUniform( "uModelMatrix", m_mesh.getModelMatrix());
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
        cubemap->unbind(0u);

        // app.render();
        m_program.unbind();

        ImGui::Render();
    }

    void glfw_keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods) 
	{
		if (ImGui::IsWindowFocused())
		{
			ImGui_ImplGlfwGL3_KeyCallback(window, key, scancode, action, mods);
			return;
		}

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

				case GLFW_KEY_F:      
					bFullscreen = !bFullscreen;
#if UNDONE
					if (bFullscreen) {
						glutFullScreen();
					} else {
						glutReshapeWindow( WINDOW_WIDTH, WINDOW_HEIGHT);
					}
#endif
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
		const bool mouseOverGui = ImGui::MouseOverArea();
		if (!mouseOverGui) camera.motionHandler( xpos, ypos, false);    
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
