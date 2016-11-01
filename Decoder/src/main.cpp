#include "decoder.h"

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

struct StreamingStrategy
{
	std::string format;
	std::string path;

	StreamingStrategy(const std::string &&_format, const std::string &&_path)
		: format(_format)
		, path(_path)
	{}
};

StreamingStrategy file_strategy("mp4", "test.mp4");
StreamingStrategy rtsp_strategy("rtsp", "rtsp://127.0.0.1:54321/live.sdp");
StreamingStrategy rtmp_strategy("rtmp", "rtmp://127.0.0.1:54321/live.sdp");
StreamingStrategy mpegts_strategy("mpegts", "udp://127.0.0.1:54321");

Decoder decoder;

constexpr int width_start = 1280;
constexpr int height_start = 720;
constexpr int bitdepth = 3;

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

int main(int argc, char** argv)
{
	decoder.init();
	decoder.open_stream(file_strategy.format, file_strategy.path);

	GLFWwindow* window;
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		exit(EXIT_FAILURE);
	window = glfwCreateWindow(width_start, height_start, "Decoder Window", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwSetKeyCallback(window, key_callback);

	glewInit();

	while (!glfwWindowShouldClose(window))
	{
		float ratio;
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		ratio = width / (float)height;

		if (decoder.initialized() && decoder.stream_opened()) {
			auto &frame = decoder.get_current_frame();

			GLuint texColorBuffer;
			glGenTextures(1, &texColorBuffer);
			glBindTexture(GL_TEXTURE_2D, texColorBuffer);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame.info.width, frame.info.height, 0, GL_RGB, GL_UNSIGNED_BYTE, frame.data.data());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// Draw the texture
			glClear(GL_COLOR_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texColorBuffer);
			glPushAttrib(GL_ALL_ATTRIB_BITS);
			glEnable(GL_TEXTURE_2D);
			glViewport(0, 0, width, height);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 1.0f);
			glVertex3f(-1.0f, 1.0f, 0.f);
			glTexCoord2f(1.0f, 1.0f);
			glVertex3f(1.0f, 1.0f, 0.f);
			glTexCoord2f(1.0f, 0.0f);
			glVertex3f(1.0f, -1.0f, 0.f);
			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(-1.0f, -1.0f, 0.f);
			glEnd();
			glPopAttrib();
			glBindTexture(GL_TEXTURE_2D, 0);

			glDeleteTextures(1, &texColorBuffer);
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	decoder.close_stream();
	decoder.shutdown();

	return 0;
}