#include "application.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/Stb_Image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

#define TRIS_WORK_GROUP_SIZE_X 20
#define TRIS_WORK_GROUP_SIZE_Y 20
#define PROJECT_WORK_GROUP_SIZE_X 20
#define PROJECT_WORK_GROUP_SIZE_Y 20
#define RESAMPLE_WORK_GROUP_SIZE_X 20
#define RESAMPLE_WORK_GROUP_SIZE_Y 20
#define WORLD3D_WORK_GROUP_SIZE_X 30
#define WORLD3D_WORK_GROUP_SIZE_Y 30
#define VERTEX_WORK_GROUP_SIZE_X 30
#define VERTEX_WORK_GROUP_SIZE_Y 30

void frameBufferSizeCallBack(GLFWwindow* window, int height, int width) 
{
    glViewport(0, 0, width, height); 
}

namespace SceneSynthesis {
    application::application(int width, int height)
    {
        std::string filename = "Images/ColorImage.png";
        m_colorImage = loadImage(filename.c_str(), STBI_rgb_alpha);
        filename = "Images/Depthmap.png";
        m_depthmap = loadImage(filename.c_str(), STBI_grey);
		filename = "Images/ExrData.mat";
		loadExrImage(filename.c_str());
		m_camera = std::make_unique<Camera>(glm::vec3(0, 0, -20), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), 20, 90, -90);
		sphericalCameraTransform(m_camera.get(), 0, 0, 0, glm::vec3(0));
		glfwInitialize();
        openGlInitialize(width, height);
    }

    application::~application()
    {
        destroyOpenGl();
        destroyWindow();    
    }

    std::unique_ptr<application::Image> application::loadImage(const char* filename, int channelNum)
    {
        Image image;
        image.pixels1D = stbi_load(filename, &image.width, &image.height, &image.channels, channelNum);
        if (image.pixels1D == NULL)
            std::cout << "Failed to load " << filename;
        else 
        {
            image.pixels1Df = new float[image.width * image.height * image.channels];
            for (int i = 0; i < image.width * image.height * image.channels; i++)
            {
                image.pixels1Df[i] = (float)image.pixels1D[i] / 255.;
            }
        }
        return std::make_unique<Image>(image);
    }

	void application::loadExrChannelFromMatlab(const char* filename, ExrChannel* channel, const char* channelName)
	{
		mxArray* data;

		MATFile* matFile = matOpen(filename, "r");

		if (matFile == NULL) {
			printf("Error opening file %s\n", filename);
			return;
		}

		data = matGetVariable(matFile, channelName);

		double* dataElement;
		if (data != NULL && !mxIsEmpty(data)) {
			// copy data
			mwSize num = mxGetNumberOfElements(data);

			dataElement = mxGetPr(data);

			if (dataElement != NULL) {
				channel->channelData = dataElement;
				channel->nRows = mxGetM(data);
				channel->nCols = mxGetN(data);
				channel->size = mxGetNumberOfElements(data);
			}
			printf("matread_Matrix \n");
			printf("oDoubleMat_LOC.nRows %i ; oDoubleMat_LOC.nCols %i \n", channel->nRows, channel->nCols);

		}
		else {
			printf("nothing to read \n");
		}

		matClose(matFile);
	}

	void application::loadExrImage(const char* filename)
	{
		m_redChannel = std::make_unique<ExrChannel>();
		m_greenChannel = std::make_unique<ExrChannel>();
		m_blueChannel = std::make_unique<ExrChannel>();
		m_depthChannel = std::make_unique<ExrChannel>();
		loadExrChannelFromMatlab(filename, m_redChannel.get(), "R");
		loadExrChannelFromMatlab(filename, m_greenChannel.get(), "G");
		loadExrChannelFromMatlab(filename, m_blueChannel.get(), "B");
		loadExrChannelFromMatlab(filename, m_depthChannel.get(), "Z");
		std::cout << m_redChannel->channelData[2];
	}
	
	void application::glfwInitialize()
    {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }

    int application::openGlInitialize(int width, int height)
    {
        m_windowWidth = width;
        m_windowHeight = height;
        m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "3D Scene Synthesis", NULL, NULL);
        glfwMakeContextCurrent(m_window);
        if (m_window == NULL)
        {
            std::cout << "Failed to create a new window\n";
            glfwTerminate();
        }

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cout << "Failed to initialize GLAD" << std::endl;
            return -1;
        }

        glViewport(0, 0, m_windowWidth, m_windowHeight);

        glfwSetFramebufferSizeCallback(m_window, frameBufferSizeCallBack);

        performShader({ "shaders/calcTris.compute", GL_COMPUTE_SHADER, true });
        setupDepthTrisComputeBuffer(m_shaderPrograms[m_shaderPrograms.size() - 1]);
        getDepthTrisComputeBufferOutput(m_workGroupsNum[m_workGroupsNum.size() - 1]);

        performShader({"shaders/calcHDGridCoord.compute", GL_COMPUTE_SHADER, true});
        setupHDGridCoordComputeBuffer(m_shaderPrograms[m_shaderPrograms.size() - 1]);
        getHDGridCoordComputeBufferOutput(m_workGroupsNum[m_workGroupsNum.size() - 1]);

        performShader({ "shaders/calcDepthmapResampling.compute", GL_COMPUTE_SHADER, true });
        setupDepthmapResampling(m_shaderPrograms[m_shaderPrograms.size() - 1]);
        getDepthmapResampling(m_workGroupsNum[m_workGroupsNum.size() - 1]);

        performShader({ "shaders/calc3DWorldCoord.compute", GL_COMPUTE_SHADER, true });
        setup3DWorldCoord(m_shaderPrograms[m_shaderPrograms.size() - 1]);
        get3DWorldCoord(m_workGroupsNum[m_workGroupsNum.size() - 1]);

		performShader({ "shaders/calcTris.compute", GL_COMPUTE_SHADER, true });
		setupTrisComputeBuffer(m_shaderPrograms[m_shaderPrograms.size() - 1]);
		getTrisComputeBufferOutput(m_workGroupsNum[m_workGroupsNum.size() - 1]);

		performShader({ "shaders/calcVertexData.txt", GL_COMPUTE_SHADER, true });
		setupVertexData(m_shaderPrograms[m_shaderPrograms.size() - 1]);
		getVertexData(m_workGroupsNum[m_workGroupsNum.size() - 1]);

		glEnable(GL_DEPTH_TEST);
        return 0;
    }

    void application::setupDepthTrisComputeBuffer(unsigned int programId)
    {
        GLsizeiptr triBufferSize = (m_depthmap->width - 1.0) * (m_depthmap->height - 1.0) * 6.0 * sizeof(unsigned int);
        glGenBuffers(1, &m_triBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_triBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, triBufferSize, NULL, GL_DYNAMIC_DRAW);
        m_workGroupsNum.push_back(glm::ivec2(m_depthmap->width / TRIS_WORK_GROUP_SIZE_X, 
            m_depthmap->height / TRIS_WORK_GROUP_SIZE_Y));
    }

    void application::setupHDGridCoordComputeBuffer(unsigned int programId)
    {
        GLsizeiptr depthBufferSize = m_depthmap->width * m_depthmap->height * sizeof(float);
        GLsizeiptr uvBufferSize = m_depthmap->width * m_depthmap->height * sizeof(glm::vec2);
        GLsizeiptr zBufferSize = m_depthmap->width * m_depthmap->height * sizeof(float);

        glGenBuffers(1, &m_depthBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_depthBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_depthBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, depthBufferSize, m_depthmap->pixels1Df, GL_STATIC_DRAW);

        glGenBuffers(1, &m_uvBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_uvBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, uvBufferSize, NULL, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &m_zBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_zBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, zBufferSize, NULL, GL_DYNAMIC_DRAW);

        m_workGroupsNum.push_back(glm::ivec2(m_depthmap->width / PROJECT_WORK_GROUP_SIZE_X,
            m_depthmap->height / PROJECT_WORK_GROUP_SIZE_Y));

        GLint location = glGetUniformLocation(programId, "depthCamFocal");
        if (location != -1)
            glUniform1f(location, 60.0f);
        else
            __debugbreak();

        location = glGetUniformLocation(programId, "colorCamFocal");
        if (location != -1)
            glUniform1f(location, 583.f);
        else
            __debugbreak();

        location = glGetUniformLocation(programId, "principalpoint");
        if (location != -1)
            glUniform2f(location, 0, 0);
        else
            __debugbreak();

        glm::mat4 transform = glm::translate(glm::vec3(0));
        transform = glm::rotate(transform, 0.f, glm::vec3(0, 0, 1));
        transform = glm::rotate(transform, 0.f, glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, 0.f, glm::vec3(1, 0, 0));
        location = glGetUniformLocation(programId, "transformCam");
        if (location != -1)
            glUniformMatrix4fv(location, 1, false, &transform[0][0]);
        else
            __debugbreak();
    }

    void application::setupDepthmapResampling(unsigned int programId)
    {
        GLsizeiptr triBufferSize = (m_depthmap->width - 1.0) * (m_depthmap->height - 1.0) * 6.0 * sizeof(unsigned int);
        GLsizeiptr uvBufferSize = m_depthmap->width * m_depthmap->height * sizeof(glm::vec2);
        GLsizeiptr zBufferSize = m_depthmap->width * m_depthmap->height * sizeof(float);
        GLsizeiptr resampledZBufferSize = m_colorImage->width * m_colorImage->height * sizeof(float);

        m_resampledZs = std::vector<float>(resampledZBufferSize, 100);

        glGenBuffers(1, &m_triBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_triBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_triBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, triBufferSize, &m_depthtriangles[0], GL_STATIC_DRAW);

        glGenBuffers(1, &m_uvBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_uvBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_uvBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, uvBufferSize, &m_projectedUVs[0], GL_STATIC_DRAW);

        glGenBuffers(1, &m_zBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_zBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_zBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, zBufferSize, &m_projectedZs[0], GL_STATIC_DRAW);

        glGenBuffers(1, &m_resampledZBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_resampledZBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, resampledZBufferSize, &m_resampledZs[0], GL_DYNAMIC_DRAW);

        m_workGroupsNum.push_back(glm::ivec2(m_depthmap->width / RESAMPLE_WORK_GROUP_SIZE_X,
            m_depthmap->height / RESAMPLE_WORK_GROUP_SIZE_Y));
    }

    void application::setup3DWorldCoord(unsigned int programId)
    {
        GLsizeiptr resampledZBufferSize = m_colorImage->width * m_colorImage->height * sizeof(float);
        GLsizeiptr world3DcoordBufferSize = m_colorImage->width * m_colorImage->height * sizeof(glm::vec3);

        glGenBuffers(1, &m_resampledZBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_resampledZBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_resampledZBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, resampledZBufferSize, &m_resampledZs[0], GL_STATIC_DRAW);

        glGenBuffers(1, &m_world3DCoordBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_world3DCoordBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, world3DcoordBufferSize, NULL, GL_DYNAMIC_DRAW);

        GLint location = glGetUniformLocation(programId, "colorCamFocal");
        if (location != -1)
            glUniform1f(location, 583.f);
        else
            __debugbreak();

        m_workGroupsNum.push_back(glm::ivec2(m_colorImage->width / WORLD3D_WORK_GROUP_SIZE_X,
            m_colorImage->height / WORLD3D_WORK_GROUP_SIZE_Y));

    }

	void application::setupTrisComputeBuffer(unsigned int programId)
	{
		GLsizeiptr triBufferSize = (m_colorImage->width - 1.0) * (m_colorImage->height - 1.0) * 6.0 * sizeof(unsigned int);
		glGenBuffers(1, &m_triBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_triBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, triBufferSize, NULL, GL_DYNAMIC_DRAW);
		m_workGroupsNum.push_back(glm::ivec2(m_colorImage->width / TRIS_WORK_GROUP_SIZE_X,
			m_colorImage->height / TRIS_WORK_GROUP_SIZE_Y));
	}
	
	void application::setupVertexData(unsigned int programId)
	{
		GLsizeiptr worldCoorBufferSize = m_world3Dcoord.size() * sizeof(glm::vec3);
		GLsizeiptr colorBufferSize = m_colorImage->width * m_colorImage->height * m_colorImage->channels * sizeof(float);
		GLsizeiptr verticesBufferSize = m_world3Dcoord.size() * sizeof(Vertex);

		glGenBuffers(1, &m_world3DCoordBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_world3DCoordBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, worldCoorBufferSize, &m_world3Dcoord[0], GL_STATIC_DRAW);
		
		glGenBuffers(1, &m_colorBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_colorBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, colorBufferSize, m_colorImage->pixels1Df, GL_STATIC_DRAW);

		glGenBuffers(1, &m_vertexDataBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_vertexDataBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, verticesBufferSize, NULL, GL_DYNAMIC_DRAW);

		m_workGroupsNum.push_back(glm::ivec2(m_colorImage->width / VERTEX_WORK_GROUP_SIZE_X,
			m_colorImage->height / VERTEX_WORK_GROUP_SIZE_Y));
	}

	void application::getDepthTrisComputeBufferOutput(glm::uvec2 workGroupNum)
    {
        int triBufferLength = (m_depthmap->width - 1.0) * (m_depthmap->height - 1.0) * 6.0;
        glDispatchCompute(workGroupNum.x, workGroupNum.y, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_triBuffer);
        unsigned int* triangles = (unsigned int*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        m_depthtriangles = std::vector<unsigned int>(triangles, triangles + triBufferLength);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void application::getHDGridCoordComputeBufferOutput(glm::uvec2 workGroupNum)
    {
        
        int bufferLength = m_depthmap->width * m_depthmap->height;
        glDispatchCompute(workGroupNum.x, workGroupNum.y, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_uvBuffer);
        glm::vec2* uv = (glm::vec2*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        m_projectedUVs = std::vector<glm::vec2>(uv, uv + bufferLength);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_zBuffer);
        float* z = (float*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        m_projectedZs = std::vector<float>(z, z + bufferLength);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		/*for (int i = 0; i < m_projectedUVs.size(); i++)
		{
			glm::vec2 uv = m_projectedUVs[i];
			std::cout << i << " " << uv.x << " " << uv.y << std::endl;
		}*/
    }

    void application::getDepthmapResampling(glm::uvec2 workGroupNum)
    {
        int bufferLength = m_colorImage->width * m_colorImage->height;
        glDispatchCompute(workGroupNum.x, workGroupNum.y, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_resampledZBuffer);
        float* resampledZs = (float*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        m_resampledZs = std::vector<float>(resampledZs, resampledZs + bufferLength);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        unsigned char* b = new unsigned char[bufferLength * 1];
		for (int i = 0; i < bufferLength; i++)
        {
            b[i] = (unsigned char)(resampledZs[i] * 255.);
        }
        stbi_write_png("Images/Z.png", m_colorImage->width, m_colorImage->height, 1, b, m_colorImage->width * 1);
    }

    void application::get3DWorldCoord(glm::vec2 workGroupNum)
    {
        int bufferLength = m_colorImage->width * m_colorImage->height;
        glDispatchCompute(workGroupNum.x, workGroupNum.y, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_world3DCoordBuffer);
        glm::vec3* world3Dcoord = (glm::vec3*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        m_world3Dcoord = std::vector<glm::vec3>(world3Dcoord, world3Dcoord + bufferLength);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

	void application::getTrisComputeBufferOutput(glm::uvec2 workGroupNum)
	{
		int triBufferLength = (m_colorImage->width - 1.0) * (m_colorImage->height - 1.0) * 6.0;
		glDispatchCompute(workGroupNum.x, workGroupNum.y, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_triBuffer);
		unsigned int* triangles = (unsigned int*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		m_triangles = std::vector<unsigned int>(triangles, triangles + triBufferLength);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
	
	void application::getVertexData(glm::uvec2 workGroupNum)
	{
		int bufferLength = m_world3Dcoord.size();
		glDispatchCompute(workGroupNum.x, workGroupNum.y, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_vertexDataBuffer);
		Vertex* vertices = (Vertex*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		m_vertices = std::vector<Vertex>(vertices, vertices + bufferLength);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		/*for(int i = 0; i<m_vertices.size(); i++)
		{
			glm::vec4 position = m_vertices[i].position;
			std::cout << position.x << " " << position.y << " " << position.z << std::endl;
		}*/
	}

	void application::renderScene()
	{
		std::vector<Shader> shaders = { {"shaders/vertexShader.shader", GL_VERTEX_SHADER, true },
			{"shaders/fragmentShader.shader", GL_FRAGMENT_SHADER, true } };
		performShaders(shaders);

		m_model = computeModelTransform(glm::vec3(0), glm::vec3(0, 0, 180));
		m_view = computeViewTransform(m_camera->eye, m_camera->forward, m_camera->up);
		m_projection = computeProjectionTransform(45.0, 2, 100);

		m_mvp = m_projection * m_view * m_model;

		glGenVertexArrays(1, &m_vertexArrayObject);
		glBindVertexArray(m_vertexArrayObject);

		glGenBuffers(1, &m_vertexBufferObject);
		glBindBuffer(GL_ARRAY_BUFFER, m_vertexBufferObject);
		glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), &m_vertices[0], GL_STATIC_DRAW);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec4)));
		glEnableVertexAttribArray(1);

		glGenBuffers(1, &m_elementBufferObject);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementBufferObject);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_triangles.size() * sizeof(unsigned int), &m_triangles[0], GL_STATIC_DRAW);

		GLint location = glGetUniformLocation(m_shaderPrograms[m_shaderPrograms.size() - 1], 
			"mvp");
		if (location != -1)
			glUniformMatrix4fv(location, 1, false, &m_mvp[0][0]);
		else
			__debugbreak();
	}

	glm::mat4 application::computeModelTransform(const glm::vec3& translation, const glm::vec3& eulerAngles)
	{
		glm::mat4 model;
		model = glm::translate(translation);
		model = glm::rotate(model, glm::radians(eulerAngles.z), glm::vec3(0, 0, 1));
		model = glm::rotate(model, glm::radians(eulerAngles.y), glm::vec3(0, 1, 0));
		model = glm::rotate(model, glm::radians(eulerAngles.x), glm::vec3(1, 0, 0));
		return model;
	}

	glm::mat4 application::computeViewTransform(const glm::vec3& eye, const glm::vec3& forward, const glm::vec3& up)
	{
		glm::mat4 view = glm::lookAt(eye, eye + forward, up);
		return view;
	}

	glm::mat4 application::computeProjectionTransform(const float& fov, const float& zmin, const float& zmax)
	{
		glfwGetWindowSize(m_window, &m_windowWidth, &m_windowHeight);
		glViewport(0, 0, m_windowWidth, m_windowHeight);
		float aspect = (float)m_windowWidth / (float)m_windowHeight;
		glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, zmin, zmax);
		return projection;
	}

	void application::sphericalCameraTransform(Camera* camera, float deltaAlpha, float deltaGama, float deltaR, const glm::vec3& center)
	{
		camera->gama = fmod(camera->gama + deltaGama, 360);
		camera->alpha = glm::clamp(camera->alpha + deltaAlpha, 0.1f, 179.9f);
		camera->r += deltaR;

		glm::vec3 pos = camera->eye;
		pos.y = camera->r * cos(glm::radians(camera->alpha));
		pos.x = camera->r * sin(glm::radians(camera->alpha)) * cos(glm::radians(camera->gama));
		pos.z = camera->r * sin(glm::radians(camera->alpha)) * sin(glm::radians(camera->gama));
		camera->forward = glm::normalize(center - pos);
		camera->up = glm::vec3(0, 1, 0);
		camera->eye = pos;
	}

	void application::updateCamera()
	{
		m_view = computeViewTransform(m_camera->eye, m_camera->forward, m_camera->up);
		m_projection = computeProjectionTransform(45.0, 2.0, 100.0);
		m_mvp = m_projection * m_view * m_model;
		GLint location = glGetUniformLocation(m_shaderPrograms[m_shaderPrograms.size() - 1],
			"mvp");
		if (location != -1)
			glUniformMatrix4fv(location, 1, false, &m_mvp[0][0]);
		else
			__debugbreak();
	}

    void application::run()
    {
		renderScene();
		float angle = 0;
        while (!glfwWindowShouldClose(m_window))
        {
            processInput(m_window);
			updateCamera();
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDrawElements(GL_TRIANGLES, m_triangles.size(), GL_UNSIGNED_INT, 0);
            glfwSwapBuffers(m_window);
            glfwPollEvents();

        }
    }

    void application::glClearErrors()
    {
        while (glGetError() != GL_NO_ERROR);
    }

    bool application::glDisplayError()
    {
        while (GLenum error = glGetError())
        {
            std::cout << error << std::endl;
            return false;
        }
        return true;
    }

    void application::processInput(GLFWwindow* window)
    {
		float speed = 0.5;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

		else if(glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
		{
			sphericalCameraTransform(m_camera.get(), 0, -speed, 0, glm::vec3(0));
		}

		else if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
		{
			sphericalCameraTransform(m_camera.get(), 0, speed, 0, glm::vec3(0));
		}

		else if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
		{
			sphericalCameraTransform(m_camera.get(), speed, 0, 0, glm::vec3(0));
		}

		else if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
		{
			sphericalCameraTransform(m_camera.get(), -speed, 0, 0, glm::vec3(0));
		}

		else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		{
			sphericalCameraTransform(m_camera.get(), 0, 0, speed, glm::vec3(0));
		}

		else if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		{
			sphericalCameraTransform(m_camera.get(), 0, 0, -speed, glm::vec3(0));
		}
    }

    unsigned int application::compileShader(Shader shader)
    {
        unsigned int result = 0;
        FILE* fp;
        size_t filesize;
        char* data;
        fp = fopen(shader.filename, "rb");
        if (!fp)
            return 0;
        fseek(fp, 0, SEEK_END);
        filesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        data = new char[filesize + 1];
        if (!data)
            return 0;
        fread(data, 1, filesize, fp);
        data[filesize] = 0;
        fclose(fp);
        result = glCreateShader(shader.shaderType);
        if (!result)
            return 0;
        glShaderSource(result, 1, &data, NULL);
        delete[] data;
        data = nullptr;

        glCompileShader(result);
        if (shader.checkError)
        {
            int status = 0;
            glGetShaderiv(result, GL_COMPILE_STATUS, &status);
            if (status == GL_FALSE)
            {
                int length;
                glGetShaderiv(result, GL_INFO_LOG_LENGTH, &length);
                char* message = (char*)alloca(length * sizeof(char));
                glGetShaderInfoLog(result, length, &length, message);
                std::cout << "failed to compile a shader !" << std::endl;
                std::cout << message << std::endl;
                glDeleteShader(result);
                return 0;
            }
        }
        return result;
    }

    void application::performShader(Shader shader)
    {
        m_shaderPrograms.push_back(glCreateProgram());
        int lastIndex = m_shaderPrograms.size() - 1;
        GLuint computeShader = compileShader(shader);
        glAttachShader(m_shaderPrograms[lastIndex], computeShader);
        glLinkProgram(m_shaderPrograms[lastIndex]);
        glValidateProgram(m_shaderPrograms[lastIndex]);
        checkLink(m_shaderPrograms[lastIndex]);
        glDeleteShader(computeShader);
        glUseProgram(m_shaderPrograms[lastIndex]);
    }

	void application::performShaders(std::vector<Shader> shaders)
	{
		m_shaderPrograms.push_back(glCreateProgram());
		int lastIndex = m_shaderPrograms.size() - 1;
		for (Shader shader : shaders)
		{
			GLuint s = compileShader(shader);
			glAttachShader(m_shaderPrograms[lastIndex], s);
			glDeleteShader(s);
		}	
		glLinkProgram(m_shaderPrograms[lastIndex]);
		glValidateProgram(m_shaderPrograms[lastIndex]);
		checkLink(m_shaderPrograms[lastIndex]);
		glUseProgram(m_shaderPrograms[lastIndex]);
	}

    unsigned int application::checkLink(unsigned int programId)
    {
        int error;
        glGetProgramiv(programId, GL_LINK_STATUS, &error);
        if (!error)
        {
            int length;
            glGetShaderiv(programId, GL_INFO_LOG_LENGTH, &length);
            char* message = (char*)alloca(length * sizeof(char));
            glGetShaderInfoLog(programId, length, &length, message);
            printf("Error: Linker log:\n%s\n", message);
            return -1;
        }
        return 1;
    }

    void application::destroyWindow()
    {
        if (m_window != NULL)
        {
            glfwDestroyWindow(m_window);
        }
        glfwTerminate();
    }

    void application::destroyOpenGl()
    {
        for (int i = 0; i < m_shaderPrograms.size(); i++)
        {
            glDeleteProgram(m_shaderPrograms[i]);
        }
        glDeleteBuffers(1, &m_triBuffer);
        glDeleteBuffers(1, &m_depthBuffer);
        glDeleteBuffers(1, &m_zBuffer);
        glDeleteBuffers(1, &m_uvBuffer);
        glDeleteBuffers(1, &m_resampledZBuffer);
        glDeleteBuffers(1, &m_world3DCoordBuffer);
		glDeleteBuffers(1, &m_colorBuffer);
		glDeleteBuffers(1, &m_vertexDataBuffer);
		glDeleteBuffers(1, &m_vertexBufferObject);
		glDeleteBuffers(1, &m_vertexArrayObject);
		glDeleteBuffers(1, &m_elementBufferObject);
		
    }
}