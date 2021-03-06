#include "shader.h"
#include <iostream>
#include <fstream>

//static std::string LoadShader(const std::string &fileName);

Shader::Shader(const std::string fileName)
{
	m_program = glCreateProgram();
	m_shaders[0] = CreateShader(LoadShader(fileName + ".vs"), GL_VERTEX_SHADER);
	m_shaders[1] = CreateShader(LoadShader(fileName + ".fs"), GL_FRAGMENT_SHADER);

	for (unsigned int i = 0; i < NUM_SHADERS; i++)
		glAttachShader(m_program, m_shaders[i]);

	glBindAttribLocation(m_program, 0, "position");
	glBindAttribLocation(m_program, 1, "texCoord");

	glLinkProgram(m_program);
	CheckShaderError(m_program, GL_LINK_STATUS, true, "Error: Program Failed To Link: ");

	glValidateProgram(m_program);
	CheckShaderError(m_program, GL_VALIDATE_STATUS, true, "Error: Program is Invalid: ");

	m_uniforms[TRANSFORM_U] = glGetUniformLocation(m_program, "transform");
	m_uniforms[CAM_POSITION] = glGetUniformLocation(m_program, "camPosition");

	m_timer = glGetUniformLocation(m_program, "timer");
	m_drawDistance = glGetUniformLocation(m_program, "drawDistance");


	GLuint m_diffuseTexLoc = glGetUniformLocation(m_program, "diffuseMap");
	GLuint m_normalTexLoc = glGetUniformLocation(m_program, "normalMap");
	GLuint m_renderTexLoc = glGetUniformLocation(m_program, "renderMap");
	GLuint m_depthTexLoc = glGetUniformLocation(m_program, "depthMap");
	GLuint m_ssaoTexLoc = glGetUniformLocation(m_program, "ssaoMap");

	if (m_diffuseTexLoc == -1)
		std::cout << "Diffuse Tex Location Not Found!" << fileName << std::endl;

	if (m_normalTexLoc == -1)
		std::cout << "Normal Tex Location Not Found! " << fileName << std::endl;

	if (m_renderTexLoc == -1)
		std::cout << "Render Tex Location Not Found! " << fileName << std::endl;

	glUseProgram(m_program);

	glUniform1i(m_diffuseTexLoc, 0);
	glUniform1i(m_normalTexLoc, 1);
	glUniform1i(m_renderTexLoc, 2);
	glUniform1i(m_depthTexLoc, 3);
	glUniform1i(m_ssaoTexLoc, 4);
}

Shader::~Shader()
{
	for (unsigned int i = 0; i < NUM_SHADERS; i++)
	{
		glDetachShader(m_program, m_shaders[i]);
		glDeleteShader(m_shaders[i]);
	}
	glDeleteProgram(m_program);
}

void Shader::Bind()
{
	glUseProgram(m_program);
}

void Shader::Update(Transform &transform, const Camera &camera)
{
	glm::mat4 model = camera.GetViewProjectoin() * transform.GetModel();
	glUniformMatrix4fv(m_uniforms[0], 1, GL_FALSE, &model[0][0]);
	glUniform3f(m_uniforms[CAM_POSITION], camera.position.x, camera.position.y, camera.position.z);

	if(m_timer != -1)
	glUniform1f(m_timer, timer);
	timer += 0.013;
}

GLuint Shader::CreateShader(const std::string &text, GLenum shaderType)
{
	GLuint shader = glCreateShader(shaderType);

	if (shader == 0)
		std::cerr << "Error: Shader Creation Failed" << std::endl;

	const GLchar* shaderSourceStrings[1];
	GLint shaderSourceLengths[1];

	shaderSourceStrings[0] = text.c_str();
	shaderSourceLengths[0] = text.length();

	glShaderSource(shader, 1, shaderSourceStrings, shaderSourceLengths);
	glCompileShader(shader);

	CheckShaderError(shader, GL_COMPILE_STATUS, false, "Error: Shader Compilation Failed: ");

	return shader;
}

std::string Shader::LoadShader(const std::string &fileName)
{
	std::ifstream file;
	file.open((fileName).c_str());

	std::string output;
	std::string line;

	if (file.is_open())
	{
		while (file.good())
		{
			getline(file, line);
			output.append(line + "\n");
		}
	}
	else
	{
		std::cerr << "Couldn't load Shader: " << fileName << std::endl;
	}

	return output;
}

void Shader::CheckShaderError(GLuint shader, GLuint flag, bool isProgram, const std::string& errorMessage)
{
	GLint success = 0;
	GLchar error[1024] = { 0 };

	if (isProgram)
		glGetProgramiv(shader, flag, &success);
	else
		glGetShaderiv(shader, flag, &success);

	if (success == GL_FALSE)
	{
		if (isProgram)
			glGetProgramInfoLog(shader, sizeof(error), NULL, error);
		else
			glGetShaderInfoLog(shader, sizeof(error), NULL, error);

		std::cerr << errorMessage << ": '" << error << "'" << std::endl;
	}
}

