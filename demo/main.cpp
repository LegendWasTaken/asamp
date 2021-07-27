#include <asp.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glad/glad.h>

#include <string>
#include <vector>

[[nodiscard]]
std::vector<uint8_t> load_image(const std::string &path, int *out_width, int *out_height) {
    int width;
    int height;
    int channels;

    // We're gonna scale this into black / white anyways
    auto data = stbi_load(path.c_str(), &width, &height, &channels, 3);
    auto black_white_data = std::vector<uint8_t>(width * height);

    for (auto i = 0; i < width * height; i++) {
        const auto r = (static_cast<float>(data[i * 3 + 0]) / 255.0f) * .299;
        const auto g = (static_cast<float>(data[i * 3 + 1]) / 255.0f) * .587;
        const auto b = (static_cast<float>(data[i * 3 + 2]) / 255.0f) * .144;

        const auto gray = floor(r + g + b + 0.5f);
        black_white_data[i] = gray * 255.f;
    }
    stbi_image_free(data);

    *out_width = width;
    *out_height = height;
    return black_white_data;
}

void draw_box(int x, int y, int width, int height, int image_width, int image_height, float level) {
    constexpr auto offset = 0.0025;

    const auto bx = float((static_cast<float>(x) / float(image_width)) * 2.0f - 1.0f + offset);
    const auto by = float((static_cast<float>(y) / float(image_height)) * 2.0f - 1.0f + offset);
    const auto tx = float((static_cast<float>(x + width) / float(image_width)) * 2.0f - 1.0f - offset);
    const auto ty = float((static_cast<float>(y + height) / float(image_height)) * 2.0f - 1.0f - offset);

    glColor3f(0.2, 0.2, 0.8);
//    glColor3f(level, level, level);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    glVertex2f(bx, by);
    glVertex2f(bx, ty);

    glVertex2f(bx, ty);
    glVertex2f(tx, ty);

    glVertex2f(tx, ty);
    glVertex2f(tx, by);

    glVertex2f(tx, by);
    glVertex2f(bx, by);
    glEnd();
}

int main() {
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    const auto data = load_image("./demo/noise_buffer_samples/fake_noise2.png", &width, &height);

    // Data is a single black / white buffer, we need an RGB buffer for open-gl
    auto opengl_texture_data = std::vector<uint8_t>(data.size() * 3);
    for (auto i = 0; i < data.size(); i++) {
        opengl_texture_data[i * 3 + 0] = data[i] * -1 + 255;
        opengl_texture_data[i * 3 + 1] = data[i] * -1 + 255;
        opengl_texture_data[i * 3 + 2] = data[i] * -1 + 255;
    }

    glfwInit();

    glfwInitHint(GLFW_VERSION_MAJOR, 4);
    glfwInitHint(GLFW_VERSION_MINOR, 6);
    glfwInitHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    const auto window = glfwCreateWindow(width, height, "Demo", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    auto noise_buffer = asp::noise_buffer();
    // API expects a float buffer so... yknow what this means...
    auto as_float = std::vector<float>(data.size());
    for (auto i = 0; i < data.size(); i++)
        as_float[i] = (float(data[i]) / 255.0f) * -1 + 1;
    noise_buffer.noise = as_float.data();
    noise_buffer.max_noise = 1.0f;
    noise_buffer.width = width;
    noise_buffer.height = height;

    const auto tiles = false ? asp::analyzed_noise_bottom_up(noise_buffer) : asp::analyse_noise(noise_buffer);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Main rendering stuff here
        glDrawPixels(width, height, GL_RGB, GL_UNSIGNED_BYTE, opengl_texture_data.data());

        for (const auto &tile : tiles.tiles)
            draw_box(tile.x, tile.y, tile.width, tile.height, width, height, tile.noise / tiles.max_noise);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

}