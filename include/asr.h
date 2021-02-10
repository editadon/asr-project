#ifndef AUR_H
#define AUR_H

#include <GL/glew.h>
#include <SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <stack>
#include <cstdint>

/*
 * Platform Quirks
 */

#ifdef __APPLE__
#define glGenVertexArrays glGenVertexArraysAPPLE
#define glBindVertexArray glBindVertexArrayAPPLE
#define glDeleteVertexArrays glDeleteVertexArraysAPPLE
#endif

namespace asr
{
    /*
     * SDL Window Globals
     */

    static int window_width{500};
    static int window_height{500};

    static SDL_Window *window{nullptr};
    static SDL_GLContext gl_context;

    std::function<void(int)> key_down_event_handler;
    std::function<void(const uint8_t *)> keys_down_event_handler;

    /*
     * Shader Identifiers
     */

    static GLuint shader_program{0};
    static GLint position_attribute_location{-1};
    static GLint color_attribute_location{-1};
    static GLint time_uniform_location{-1};
    static GLint mvp_matrix_uniform_location{-1};

    /*
     * GPUGeometry Buffers' Data
     */

    enum GeometryType
    {
        Points,
        Lines,
        LineLoop,
        LineStrip,
        Triangles,
        TriangleFan,
        TriangleStrip
    };

    struct Vertex {
        float x, y, z;
        float r, g, b, a;
    };

    struct GPUGeometry {
        GeometryType type;
        unsigned int vertex_count;

        int vertex_array_object;
        int vertex_buffer_object;
        int index_buffer_object;
    };

    static GPUGeometry *current_gpu_geometry{nullptr};

    /*
     * Transformation Data
     */

    enum MatrixMode
    {
        Model,
        View,
        Projection
    };

    std::stack<glm::mat4> model_matrix_stack;
    std::stack<glm::mat4> view_matrix_stack;
    std::stack<glm::mat4> projection_matrix_stack;
    std::stack<glm::mat4> *current_matrix_stack = &model_matrix_stack;

    /*
     * Utility Data
     */

    static std::chrono::system_clock::time_point rendering_start_time;

    /*
     * SDL Window Handling
     */

    static void create_es2_sdl_window()
    {
        SDL_Init(SDL_INIT_VIDEO);

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 2);

        window =
            SDL_CreateWindow(
                "ASR: Version 2.0",
                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                window_width, window_height,
                SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
            );
        SDL_GL_GetDrawableSize(
            window,
            reinterpret_cast<int *>(&window_width),
            reinterpret_cast<int *>(&window_height)
        );

        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);

        if (glewInit() != GLEW_OK) {
            std::cerr << "Failed to initialize the OpenGL loader." << std::endl;
            exit(-1);
        }

        if (SDL_GL_SetSwapInterval(-1) < 0) {
            SDL_GL_SetSwapInterval(1);
        }

        key_down_event_handler = [&](int key) { if (key == SDLK_ESCAPE) { exit(0); }};
        keys_down_event_handler = [&](const uint8_t *keys) { };
    }

    static void set_es2_sdl_key_down_event_handler(std::function<void(int)> event_handler)
    {
        key_down_event_handler = event_handler;
    }

    static void set_es2_sdl_keys_down_event_handler(std::function<void(const uint8_t *)> event_handler)
    {
        keys_down_event_handler = event_handler;
    }

    static void process_es2_sdl_window_events(bool *should_stop)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                *should_stop = true;
            } else if (event.type == SDL_KEYDOWN) {
                key_down_event_handler(event.key.keysym.sym);
            }
        }
    }

    static void destroy_es2_sdl_window()
    {
        SDL_GL_DeleteContext(gl_context);

        SDL_DestroyWindow(window);
        window = nullptr;

        SDL_Quit();
    }

    /*
     * Shader Program Handling
     */

    static void create_es2_shader_program(const char *vertex_shader_source, const char *fragment_shader_source)
    {
        GLint status;

        GLuint vertex_shader_object = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader_object, 1, static_cast<const GLchar **>(&vertex_shader_source), nullptr);
        glCompileShader(vertex_shader_object);
        glGetShaderiv(vertex_shader_object, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            GLint info_log_length;
            glGetShaderiv(vertex_shader_object, GL_INFO_LOG_LENGTH, &info_log_length);
            if (info_log_length > 0) {
                auto *info_log = new GLchar[static_cast<size_t>(info_log_length)];

                glGetShaderInfoLog(vertex_shader_object, info_log_length, nullptr, info_log);
                std::cerr << "Failed to compile a vertex shader" << std::endl
                          << "Compilation log:\n" << info_log << std::endl << std::endl;

                delete[] info_log;
            }
        }

        GLuint fragment_shader_object = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader_object, 1, static_cast<const GLchar **>(&fragment_shader_source), nullptr);
        glCompileShader(fragment_shader_object);
        glGetShaderiv(fragment_shader_object, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            GLint info_log_length;
            glGetShaderiv(fragment_shader_object, GL_INFO_LOG_LENGTH, &info_log_length);
            if (info_log_length > 0) {
                auto *info_log = new GLchar[static_cast<size_t>(info_log_length)];

                glGetShaderInfoLog(fragment_shader_object, info_log_length, nullptr, info_log);
                std::cerr << "Failed to compile a fragment shader" << std::endl
                          << "Compilation log:\n" << info_log << std::endl;

                delete[] info_log;
            }
        }

        shader_program = glCreateProgram();
        glAttachShader(shader_program, vertex_shader_object);
        glAttachShader(shader_program, fragment_shader_object);
        glLinkProgram(shader_program);
        glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
        if (status == GL_FALSE) {
            GLint info_log_length;
            glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &info_log_length);
            if (info_log_length > 0) {
                auto *info_log = new GLchar[static_cast<size_t>(info_log_length)];

                glGetProgramInfoLog(shader_program, info_log_length, nullptr, info_log);
                std::cerr << "Failed to link a shader program" << std::endl
                          << "Linker log:\n" << info_log << std::endl;

                delete[] info_log;
            }
        }

        glDetachShader(shader_program, vertex_shader_object);
        glDetachShader(shader_program, fragment_shader_object);
        glDeleteShader(vertex_shader_object);
        glDeleteShader(fragment_shader_object);

        position_attribute_location = glGetAttribLocation(shader_program, "position");
        color_attribute_location = glGetAttribLocation(shader_program, "color");

        time_uniform_location = glGetUniformLocation(shader_program, "time");
        mvp_matrix_uniform_location = glGetUniformLocation(shader_program, "model_view_projection_matrix");
    }

    static void destroy_es2_shader_program()
    {
        glUseProgram(0);
        glDeleteProgram(shader_program);
        shader_program = 0;

        position_attribute_location = -1;
        color_attribute_location = -1;
        time_uniform_location = -1;
    }

    /*
     * GPUGeometry Buffer Handling
     */

    static GLenum convert_geometry_type_to_es2_geometry_type(GeometryType type)
    {
        switch (type) {
            case GeometryType::Points:
                return GL_POINTS;
            case GeometryType::Lines:
                return GL_LINES;
            case GeometryType::LineLoop:
                return GL_LINE_LOOP;
            case GeometryType::LineStrip:
                return GL_LINE_STRIP;
            case GeometryType::Triangles:
                return GL_TRIANGLES;
            case GeometryType::TriangleFan:
                return GL_TRIANGLE_FAN;
            case GeometryType::TriangleStrip:
                return GL_TRIANGLE_STRIP;
        }

        return GL_TRIANGLES;
    }

    static GPUGeometry generate_es2_gpu_geometry(GeometryType type, std::vector<Vertex> vertices, std::vector<unsigned int> indices)
    {
        GPUGeometry geometry;

        geometry.vertex_count = indices.size();
        geometry.type = type;

        GLuint vertex_array_object{0};
        GLuint vertex_buffer_object{0};
        GLuint index_buffer_object{0};

        glGenVertexArrays(1, &vertex_array_object);
        geometry.vertex_array_object = static_cast<int>(vertex_array_object);
        glBindVertexArray(vertex_array_object);

        glGenBuffers(1, &vertex_buffer_object);
        geometry.vertex_buffer_object = static_cast<int>(vertex_buffer_object);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object);
        glBufferData(
            GL_ARRAY_BUFFER,
            vertices.size() * 7 * sizeof(float),
            reinterpret_cast<const float *>(vertices.data()),
            GL_STATIC_DRAW
        );

        glGenBuffers(1, &index_buffer_object);
        geometry.index_buffer_object = static_cast<int>(index_buffer_object);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_object);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(unsigned int),
            reinterpret_cast<const unsigned int *>(indices.data()),
            GL_STATIC_DRAW
        );

        GLsizei stride = sizeof(GLfloat) * 7;
        glEnableVertexAttribArray(position_attribute_location);
        glVertexAttribPointer(
            position_attribute_location,
            3, GL_FLOAT, GL_FALSE, stride, static_cast<const GLvoid *>(nullptr)
        );
        glEnableVertexAttribArray(color_attribute_location);
        glVertexAttribPointer(
            color_attribute_location,
            4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const GLvoid *>(sizeof(GLfloat) * 3)
        );

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        return geometry;
    }

    static void set_es2_gpu_geometry_current(GPUGeometry *geometry)
    {
        current_gpu_geometry = geometry;
        if (geometry != nullptr) {
            glBindVertexArray(static_cast<GLuint>(geometry->vertex_array_object));
        } else {
            glBindVertexArray(0);
        }
    }

    static void destroy_es2_gpu_geometry(GPUGeometry &geometry)
    {
        GLuint vertex_array_object{static_cast<GLuint>(geometry.vertex_array_object)};
        GLuint vertex_buffer_object{static_cast<GLuint>(geometry.vertex_buffer_object)};
        GLuint index_buffer_object{static_cast<GLuint>(geometry.index_buffer_object)};

        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vertex_array_object);
        geometry.vertex_array_object = 0;

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &vertex_buffer_object);
        geometry.vertex_buffer_object = 0;

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &index_buffer_object);
        geometry.index_buffer_object = 0;
    }

    /*
     * Transformation
     */

    static void set_matrix_mode(MatrixMode mode)
    {
        switch (mode) {
            case Model:
                current_matrix_stack = &model_matrix_stack;
                break;
            case View:
                current_matrix_stack = &view_matrix_stack;
                break;
            case Projection:
                current_matrix_stack = &projection_matrix_stack;
                break;
        }
    }

    static void translate_matrix(glm::vec3 translation)
    {
        glm::mat4 current_matrix = current_matrix_stack->top();
        glm::mat4 translated_matrix = glm::translate(current_matrix, translation);

        current_matrix_stack->pop();
        current_matrix_stack->push(translated_matrix);
    }

    static void rotate_matrix(glm::vec3 rotation)
    {
        glm::mat4 current_matrix = current_matrix_stack->top();
        glm::mat4 rotated_matrix = current_matrix;
        rotated_matrix = glm::rotate(rotated_matrix, rotation.y, glm::vec3{0.0f, 1.0f, 0.0f});
        rotated_matrix = glm::rotate(rotated_matrix, rotation.x, glm::vec3{1.0f, 0.0f, 0.0f});
        rotated_matrix = glm::rotate(rotated_matrix, rotation.z, glm::vec3{0.0f, 0.0f, 1.0f});

        current_matrix_stack->pop();
        current_matrix_stack->push(rotated_matrix);
    }

    static void scale_matrix(glm::vec3 scale)
    {
        glm::mat4 current_matrix = current_matrix_stack->top();
        glm::mat4 scaled_matrix = glm::scale(current_matrix, scale);

        current_matrix_stack->pop();
        current_matrix_stack->push(scaled_matrix);
    }

    static void load_matrix(glm::mat4 matrix)
    {
        current_matrix_stack->pop();
        current_matrix_stack->push(matrix);
    }

    static void load_identity_matrix()
    {
        load_matrix(glm::mat4{1.0f});
    }

    static void load_look_at_matrix(glm::vec3 position, glm::vec3 target)
    {
        glm::vec3 up{0.0f, 1.0f, 0.0f};
        load_matrix(glm::lookAt(position, target, up));
    }

    static void load_orthographic_projection_matrix(float zoom, float near_plane, float far_plane)
    {
        float aspect_ratio{static_cast<float>(window_width) / static_cast<float>(window_height)};

        float left{-(zoom * aspect_ratio)};
        float right{zoom * aspect_ratio};
        float bottom{-zoom};
        float top{zoom};

        load_matrix(glm::ortho(left, right, bottom, top, near_plane, far_plane));
    }

    static void load_perspective_projection_matrix(float field_of_view, float near_plane, float far_plane)
    {
        float aspect_ratio{static_cast<float>(window_width) / static_cast<float>(window_height)};

        load_matrix(glm::perspective(field_of_view, aspect_ratio, near_plane, far_plane));
    }

    static void push_matrix()
    {
        glm::mat4 top_matrix = current_matrix_stack->top();
        current_matrix_stack->push(top_matrix);
    }

    static void pop_matrix()
    {
        current_matrix_stack->pop();
        if (current_matrix_stack->empty()) {
            current_matrix_stack->push(glm::mat4{1.0f});
        }
    }

    /*
     * Rendering
     */

    static void prepare_for_es2_rendering()
    {
        glClearColor(0, 0, 0, 0);
        glViewport(0, 0, static_cast<GLsizei>(window_width), static_cast<GLsizei>(window_height));
        glEnable(GL_PROGRAM_POINT_SIZE);

        while(!model_matrix_stack.empty()) model_matrix_stack.pop();
        model_matrix_stack.push(glm::mat4{1.0f});

        while(!view_matrix_stack.empty()) view_matrix_stack.pop();
        view_matrix_stack.push(glm::mat4{1.0f});

        while(!projection_matrix_stack.empty()) projection_matrix_stack.pop();
        projection_matrix_stack.push(glm::mat4{1.0f});

        rendering_start_time = std::chrono::system_clock::now();
    }

    static void prepare_to_render_es2_frame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    static void set_es2_line_width(float line_width)
    {
        glLineWidth(static_cast<GLfloat>(line_width));
    }

    static void enable_face_culling()
    {
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);
    }

    static void disable_face_culling()
    {
        glDisable(GL_CULL_FACE);
    }

    static void enable_depth_test()
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }

    static void disable_depth_test()
    {
        glDisable(GL_DEPTH_TEST);
    }

    static void render_current_es2_gpu_geometry()
    {
        assert(current_gpu_geometry);

        glUseProgram(shader_program);

        if (time_uniform_location!=-1) {
            double time =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now()-rendering_start_time
                ).count();

            glUniform1f(time_uniform_location, time/1000.0);
        }

        if (mvp_matrix_uniform_location!=-1) {
            glm::mat4 model_matrix = model_matrix_stack.top();
            glm::mat4 view_matrix = glm::inverse(view_matrix_stack.top());
            glm::mat4 projection_matrix = projection_matrix_stack.top();
            glm::mat4 model_view_projection_matrix = projection_matrix * view_matrix * model_matrix;
            glUniformMatrix4fv(
                mvp_matrix_uniform_location,
                1, GL_FALSE,
                glm::value_ptr(model_view_projection_matrix)
            );
        }

        glDrawElements(
            convert_geometry_type_to_es2_geometry_type(current_gpu_geometry->type),
            static_cast<GLsizei>(current_gpu_geometry->vertex_count),
            GL_UNSIGNED_INT,
            nullptr
        );
    }

    static void finish_es2_frame_rendering()
    {
        SDL_GL_SwapWindow(window);
    }
}

#endif
