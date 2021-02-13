#include <glad/glad.h>
#include <Windows.h>
#include <glfw3.h>
#include <iostream>
#include <vector>
#include <ctime>
#include "mat.h"
#include "math.h"
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
			unsigned char* pixels1D;
			float* pixels1Df;
		};
		struct Shader {
			const char* filename;
			unsigned int shaderType;
			bool checkError;
		};
		struct Camera {
			glm::vec3 eye, up, forward;
			float fov, zmin, zmax;
			float alpha, gama, r;
			Camera(glm::vec3 eye, glm::vec3 forward, glm::vec3 up, float fov, float zmin, float zmax,
				float r, float alpha, float gama)
			{
				this->eye = eye;
				this->forward = forward;
				this->up = up;
				this->zmin = zmin;
				this->zmax = zmax;
				this->fov = fov;
				this->alpha = alpha;
				this->gama = gama;
				this->r = r;
			}
		};
		struct Vertex
		{
			glm::vec4 position;
			glm::vec4 color;
		};
		struct ExrChannel {
			double* channelData;
			int nRows, nCols, size;
		};

	public:
		application(int width, int height);
		~application();
		void run();

	private:
		//methods
		void glClearErrors();
		bool glDisplayError();

		void glfwInitialize();
		int openGlInitialize(int width, int height);
		std::unique_ptr<Image> loadImage(const char* filename, int channelNum);
		void loadExrChannelFromMatlab(const char* filename, ExrChannel* channel, const char* channelName);
		void loadExrImage(const char* filename);
		
		void setupDepthTrisComputeBuffer(unsigned int programId);
		void getDepthTrisComputeBufferOutput(glm::uvec2 workGroupNum);

		void setupHDGridCoordComputeBuffer(unsigned int programId);
		void getHDGridCoordComputeBufferOutput(glm::uvec2 workGroupNum);

		void setupDepthmapResampling(unsigned int programId);
		void getDepthmapResampling(glm::uvec2 workGroupNum);

		void setup3DWorldCoord(unsigned int programId);
		void get3DWorldCoord(glm::vec2 workGroupNum);

		void setupTrisComputeBuffer(unsigned int programId);
		void getTrisComputeBufferOutput(glm::uvec2 workGroupNum);

		void setupVertexData(unsigned int programId);
		void getVertexData(glm::uvec2 workGroupNUm);
		
		void renderScene();

		void updateCamera();

		glm::mat4 computeModelTransform(const glm::vec3& translation, const glm::vec3& eulerAngles);
		glm::mat4 computeProjectionTransform(const float& fov, const float& zmin, const float& zmax);
		glm::mat4 computeViewTransform(const glm::vec3& eye, const glm::vec3& forward, const glm::vec3& up);
		
		void sphericalCameraTransform(Camera* camera, float deltaAlpha, float deltaGama, float deltaR, const glm::vec3& center);
		void cartesianCameraTransform(Camera* camera, glm::vec3 deltaPosition, glm::vec3 deltaRotation);
		void modifyFieldOfView(float speed);
		void processInput(GLFWwindow* window);
		
		void performShader(Shader shader);
		void performShaders(std::vector<Shader> shaders);
		unsigned int compileShader(Shader shader);
		unsigned int checkLink(unsigned int programId);
		
		void destroyWindow();
		void destroyOpenGl();

		//fields
		GLFWwindow* m_window;

		std::vector<GLuint> m_shaderPrograms;
		std::vector<glm::ivec2> m_workGroupsNum;

		std::unique_ptr<Image> m_depthmap, m_colorImage;
		std::unique_ptr<ExrChannel> m_redChannel, m_greenChannel, m_blueChannel, m_depthChannel;
		std::vector<unsigned int> m_depthtriangles, m_triangles;
		std::vector<glm::vec2> m_projectedUVs;
		std::vector<double> m_projectedZs, m_resampledZs;
		std::vector<glm::vec4> m_world3Dcoord;
		
		glm::mat4 m_model, m_view, m_projection, m_mvp;
		std::unique_ptr<Camera> m_camera;

		std::vector<Vertex> m_vertices;

		int m_windowWidth, m_windowHeight;

		unsigned int m_triBuffer, m_uvBuffer, m_zBuffer, m_depthBuffer, m_resampledZBuffer, m_world3DCoordBuffer,
			m_redBuffer, m_greenBuffer, m_blueBuffer, m_vertexDataBuffer, m_vertexBufferObject, m_vertexArrayObject, 
			m_elementBufferObject;
		unsigned int m_testBuffer;

		float m_rspeed = 0.5, m_tspeed = 0.01, m_fovSpeed = 0.1;
	};
}

