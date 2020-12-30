#include <glad/glad.h>
#include <glfw3.h>
#include <iostream>
#include <vector>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/rotate_vector.hpp"

#define ASSERT(x) \
	if (!(x))     \
		__debugbreak()
#define GL_CHECK_ERROR(x) \
	GlClearErrors();      \
	x;                    \
	ASSERT(GlDisplayError())

namespace SceneSynthesis {
	class application
	{
		struct Image
		{
			int width, height, channels;
			float* pixels1D;
			float*** pixels3D;
		};
		struct Shader {
			const char* filename;
			unsigned int shaderType;
			bool checkError;
		};
	public:
		application(int width, int height);
		~application();
		void run();

	private:
		//methods
		std::unique_ptr<Image> loadImage(const char* filename);
		void glfwInitialize();
		int openGlInitialize(int width, int height);
		void setupDepthTrisComputeBuffer(unsigned int programId);
		void getDepthTrisComputeBufferOutput(glm::uvec2 workGroupNum);
		void setupHDGridCoordComputeBuffer(unsigned int programId);
		void getHDGridCoordComputeBufferOutput(glm::uvec2 workGroupNum);
		void setupDepthmapResampling(unsigned int programId);
		void getDepthmapResampling(glm::uvec2 workGroupNum);
		void glClearErrors();
		bool glDisplayError();
		void processInput(GLFWwindow* window);
		float*** convert1DTo3D(float* pixels, int width, int height, int channels);
		unsigned int compileShader(Shader shader);
		void performShader(Shader shader);
		unsigned int checkLink(unsigned int programId);
		std::vector<int> triangulate2DImageCPU(int width, int height, int channels);
		void destroyWindow();
		void destroyOpenGl();

		//fields
		std::unique_ptr<Image> m_depthmap, m_colorImage;
		std::vector<unsigned int> m_depthtriangles;
		std::vector<glm::vec2> m_projectedUVs;
		std::vector<float> m_projectedZs, m_resampledZs;
		GLFWwindow* m_window;
		std::vector<GLuint> m_computeShaderPrograms;
		std::vector<glm::ivec2> m_workGroupsNum;
		int m_windowWidth, m_windowHeight;
		unsigned int m_triBuffer, m_uvBuffer, m_zBuffer, m_depthBuffer, m_resampledZ;

	};
}

