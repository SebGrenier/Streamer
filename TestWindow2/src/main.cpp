#include "streamer.h"

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

#include "freetype/freetype_utils.h"
#include <fstream>

Streamer streamer;

constexpr int width_start = 1920;
constexpr int height_start = 1080;
constexpr int bitdepth = 4;

std::vector<uint8_t>& operator << (std::vector<uint8_t> &vec, std::string &string)
{
	for(auto &c : string) {
		vec.push_back(c);
	}
	return vec;
}

struct ImageInfo
{
	unsigned width;
	unsigned height;
	short depth;
};

bool operator != (const ImageInfo &lhs, const ImageInfo rhs)
{
	return lhs.width != rhs.width ||
		lhs.height != rhs.height ||
		lhs.depth != rhs.depth;
}

struct StreamingStrategy
{
	std::string format;
	std::string path;

	StreamingStrategy(const std::string &&_format, const std::string &&_path)
		: format(_format)
		, path(_path)
	{}
};

StreamingStrategy file_strategy("mp4", "../../HTML5/live/video.mp4");
StreamingStrategy rtsp_strategy("rtsp", "rtsp://127.0.0.1:54321/live.sdp");
StreamingStrategy rtmp_strategy("rtmp", "rtmp://127.0.0.1:54321/live.sdp");
StreamingStrategy mpegts_strategy("mpegts", "udp://127.0.0.1:54321");
StreamingStrategy http_strategy("hls", "../../HTML5/live/index.m3u8");
StreamingStrategy dash_strategy("stream_segment", "../../HTML5/live/video.mp4");
StreamingStrategy raw_h264_strategy("h264", "video.h264");
auto &current_strategy = raw_h264_strategy;

void save_image(const std::vector<uint8_t> image, const ImageInfo &image_info, const std::string &filename)
{
	std::ofstream file;
	file.open(filename, std::ios::out | std::ios::binary);

	if (file.is_open()) {
		file.write((char*)image.data(), image.size());
		file.close();
	}
}

std::vector<uint8_t> flip_frame(const std::vector<uint8_t> &original, int width, int height, int depth)
{
	std::vector<uint8_t> flipped_frame;
	flipped_frame.reserve(original.size());
	const int row_size = width * depth;
	int index = 0;
	for (int y = height - 1; y >= 0; --y) {
		index = y * row_size;
		for (int x = 0; x < row_size; ++x) {
			flipped_frame.push_back(original[index]);
			++index;
		}
	}
	return flipped_frame;
}

void flip_frame2(const std::vector<uint8_t> &original, std::vector<uint8_t> &output, int width, int height, int depth)
{
	output.resize(original.size());
	const int row_size = width * depth;
	auto *orig_data_start = original.data() + (height - 1) * row_size;
	auto *output_data_start = output.data();
	for (int y = 0; y < height; ++y) {
		memcpy_s(output_data_start, row_size, orig_data_start, row_size);
		output_data_start += row_size;
		orig_data_start -= row_size;
	}
}

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

int main(void)
{
	streamer.init();
	ImageInfo info = {width_start, height_start, bitdepth};
	streamer.open_stream(info.width, info.height, info.depth, current_strategy.format, current_strategy.path);

	// GLFW
	GLFWwindow* window;
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		exit(EXIT_FAILURE);
	window = glfwCreateWindow(width_start, height_start, "Test Window", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwSetKeyCallback(window, key_callback);

	glewInit();

	GLuint frame_buffer_id;
	glGenFramebuffers(1, &frame_buffer_id);
	glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);

	GLuint texColorBuffer;
	glGenTextures(1, &texColorBuffer);
	glBindTexture(GL_TEXTURE_2D, texColorBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_start, height_start, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);

	GLuint depthBuffer;
	glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width_start, height_start);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Error generating framebuffer" << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// This Holds All The Information For The Font That We Are Going To Create.
	freetype::font_data our_font;
	our_font.init("C:/Windows/Fonts/Arial.ttf", 16);

	// Main loop
	std::vector<uint8_t> frame;
	std::vector<uint8_t> flipped_frame;
	int count = 0;
	ImageInfo new_info;
	while (!glfwWindowShouldClose(window))
	{
		float ratio;
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		ratio = width / (float)height;

		glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);

		new_info = { (unsigned)width, (unsigned)height, (unsigned)bitdepth };
		if (new_info != info) {
			streamer.close_stream();
			streamer.open_stream(width, height, bitdepth, current_strategy.format, current_strategy.path);
			info = new_info;

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
			glDeleteTextures(1, &texColorBuffer);
			glGenTextures(1, &texColorBuffer);
			glBindTexture(GL_TEXTURE_2D, texColorBuffer);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);

			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
			glDeleteBuffers(1, &depthBuffer);
			glGenRenderbuffers(1, &depthBuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
		}

		// Draw into the frame buffer
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-ratio, ratio, -1.f, 1.f, -1.f, 1.f);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glPushMatrix();
		glRotatef((float)glfwGetTime() * 50.f, 0.f, 0.f, 1.f);
		glBegin(GL_TRIANGLES);
		glColor3f(1.f, 0.f, 0.f);
		glVertex3f(-0.6f, -0.4f, 0.f);
		glColor3f(0.f, 1.f, 0.f);
		glVertex3f(0.6f, -0.4f, 0.f);
		glColor3f(0.f, 0.f, 1.f);
		glVertex3f(0.f, 0.6f, 0.f);
		glEnd();
		glPopMatrix();
		glPopAttrib();

		freetype::print(our_font, 320, 200, "Frame number : %i", count);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

		glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
		frame.clear();
		frame.resize(width * height * bitdepth);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (void*)frame.data());
		info.width = width;
		info.height = height;
		info.depth = bitdepth;

		flip_frame2(frame, flipped_frame, width, height, bitdepth);
		streamer.stream_frame(flipped_frame.data(), width, height, bitdepth);
		++count;

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteBuffers(1, &depthBuffer);
	glDeleteTextures(1, &texColorBuffer);
	glDeleteBuffers(1, &frame_buffer_id);

	glfwDestroyWindow(window);
	glfwTerminate();

	streamer.shutdown();

	return 0;
}