#include "application.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/Stb_Image.h"
#define TRIS_WORK_GROUP_SIZE_X 20
#define TRIS_WORK_GROUP_SIZE_Y 20
#define PROJECT_WORK_GROUP_SIZE_X 20
#define PROJECT_WORK_GROUP_SIZE_Y 20
#define RESAMPLE_WORK_GROUP_SIZE_X 20
#define RESAMPLE_WORK_GROUP_SIZE_Y 20

void frameBufferSizeCallBack(GLFWwindow* window, int height, int width) 
{
    glViewport(0, 0, width, height); 
}

namespace SceneSynthesis {
    application::application(int width, int height)
    {
        std::string filename = "ColorImage.png";
        m_colorImage = loadImage(filename.c_str());
        filename = "Depthmap.png";
        m_depthmap = loadImage(filename.c_str());
        glfwInitialize();
        openGlInitialize(width, height);
    }

    application::~application()
    {
        destroyOpenGl();
        destroyWindow();    
    }


    std::unique_ptr<application::Image> application::loadImage(const char* filename)
    {
        Image image;
        image.pixels1D = stbi_loadf(filename, &image.width, &image.height, &image.channels, STBI_rgb_alpha);
        if (image.pixels1D == NULL)
            std::cout << "Failed to load " << filename;
        else
            image.pixels3D = convert1DTo3D(image.pixels1D, image.width, image.height, image.channels);
        return std::make_unique<Image>(image);
    }

    void application::glfwInitialize()
    {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }

    int application::openGlInitialize(int width, int height)
    {
        m_windowWidth = width;
        m_windowHeight = height;
        m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "Hello", NULL, NULL);
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
        setupDepthTrisComputeBuffer(m_computeShaderPrograms[m_computeShaderPrograms.size() - 1]);
        getDepthTrisComputeBufferOutput(m_workGroupsNum[m_workGroupsNum.size() - 1]);

        performShader({"shaders/calcHDGridCoord.compute", GL_COMPUTE_SHADER, true});
        setupHDGridCoordComputeBuffer(m_computeShaderPrograms[m_computeShaderPrograms.size() - 1]);
        getHDGridCoordComputeBufferOutput(m_workGroupsNum[m_workGroupsNum.size() - 1]);

        performShader({ "shaders/calcDepthmapResampling.compute", GL_COMPUTE_SHADER, true });
        setupDepthmapResampling(m_computeShaderPrograms[m_computeShaderPrograms.size() - 1]);
        getDepthmapResampling(m_workGroupsNum[m_workGroupsNum.size() - 1]);
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

        GLint location = glGetUniformLocation(programId, "channels");
        if (location != -1)
            glUniform1i(location, m_depthmap->channels);
        else
            __debugbreak();

    }

    void application::setupHDGridCoordComputeBuffer(unsigned int programId)
    {
        GLsizeiptr depthBufferSize = m_depthmap->width * m_depthmap->height * sizeof(float);
        GLsizeiptr uvBufferSize = m_depthmap->width * m_depthmap->height * sizeof(glm::vec2);
        GLsizeiptr zBufferSize = m_depthmap->width * m_depthmap->height * sizeof(float);

        glGenBuffers(1, &m_depthBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_depthBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_depthBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, depthBufferSize, m_depthmap->pixels1D, GL_STATIC_DRAW);

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
            glUniform1f(location, 60.7f);
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
        GLsizeiptr resampledZBufferSize = 1920 * 1920 * sizeof(float);

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

        glGenBuffers(1, &m_resampledZ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_resampledZ);
        glBufferData(GL_SHADER_STORAGE_BUFFER, resampledZBufferSize, &m_resampledZs[0], GL_DYNAMIC_DRAW);

        m_workGroupsNum.push_back(glm::ivec2(m_depthmap->width / RESAMPLE_WORK_GROUP_SIZE_X,
            m_depthmap->height / RESAMPLE_WORK_GROUP_SIZE_Y));
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
        

    }

    void application::getDepthmapResampling(glm::uvec2 workGroupNum)
    {
        int bufferLength = 1920 * 1920;
        glDispatchCompute(workGroupNum.x, workGroupNum.y, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_resampledZ);
        float* resampledZs = (float*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        m_resampledZs = std::vector<float>(resampledZs, resampledZs + bufferLength);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        for (int i = 0; i < m_colorImage->height; i++) {
            for (int j = 0; j < m_colorImage->width; j++) {
                std::cout << m_resampledZs[i * m_colorImage->width + j] << " ";
            }
            std::cout << "\n\n";
        }

    }

    void application::run()
    {
        
        while (!glfwWindowShouldClose(m_window))
        {
            processInput(m_window);
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
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
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }
    }

    float*** application::convert1DTo3D(float* pixels, int width, int height, int channels)
    {
        float*** pixels2D = new float** [height];
        for (int i = 0; i < height; i++) {
            pixels2D[i] = new float* [width];
            for (int j = 0; j < width; j++) {
                pixels2D[i][j] = new float[channels];
                for (int c = 0; c < channels; c++) {
                    pixels2D[i][j][c] = pixels[(i * width + j) * channels + c];
                }
            }
        }
        return pixels2D;
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
        m_computeShaderPrograms.push_back(glCreateProgram());
        int lastIndex = m_computeShaderPrograms.size() - 1;
        GLuint computeShader = compileShader(shader);
        glAttachShader(m_computeShaderPrograms[lastIndex], computeShader);
        glLinkProgram(m_computeShaderPrograms[lastIndex]);
        glValidateProgram(m_computeShaderPrograms[lastIndex]);
        checkLink(m_computeShaderPrograms[lastIndex]);
        glDeleteShader(computeShader);
        glUseProgram(m_computeShaderPrograms[lastIndex]);
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

    std::vector<int> application::triangulate2DImageCPU(int width, int height, int channels)
    {

        std::vector<int> triangles;
        triangles.resize((width - 1.0) * (height - 1.0) * 6.0);
        int count = 0;
        for (int i = 0; i < height - 1; i++)
        {
            for (int j = 0; j < width - 1; j++)
            {
                triangles[count++] = (i * width + j) * channels;
                triangles[count++] = (i * width + j + 1) * channels;
                triangles[count++] = ((i + 1) * width + j) * channels;

                triangles[count++] = ((i + 1) * width + j) * channels;
                triangles[count++] = (i * width + j + 1) * channels;
                triangles[count++] = ((i + 1) * width + j + 1) * channels;
            }
        }

        return triangles;
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
        for (int i = 0; i < m_computeShaderPrograms.size(); i++)
        {
            glDeleteProgram(m_computeShaderPrograms[i]);
        }
        glDeleteBuffers(1, &m_triBuffer);
        glDeleteBuffers(1, &m_depthBuffer);
        glDeleteBuffers(1, &m_zBuffer);
        glDeleteBuffers(1, &m_uvBuffer);
        glDeleteBuffers(1, &m_resampledZ);
    }
}