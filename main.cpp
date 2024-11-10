/**
* Author: Tyler Chen
* Assignment: Rise of the AI
* Date due: 2024-11-03, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f
#define ENEMY_COUNT 1
#define LEVEL1_WIDTH 14
#define LEVEL1_HEIGHT 5

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL_mixer.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"
#include "Map.h"

// ————— GAME STATE ————— //
struct GameState
{
    Entity* player;
    Entity* enemies;
    Entity* backgrounds;

    Map* map;

    Mix_Music* bgm;
    Mix_Chunk* jump_sfx;
};

enum AppStatus { RUNNING, TERMINATED };

// ————— CONSTANTS ————— //
constexpr int WINDOW_WIDTH = 640 * 2,
WINDOW_HEIGHT = 480 * 2;

constexpr float BG_RED = 0.1922f,
BG_BLUE = 0.549f,
BG_GREEN = 0.9059f,
BG_OPACITY = 1.0f;

constexpr int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

constexpr char GAME_WINDOW_NAME[] = "Hello, Maps!";

constexpr char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

constexpr float MILLISECONDS_IN_SECOND = 1000.0;

constexpr char IDLE_FILEPATH[] = "assets/images/anakin_idle.png",
WALKING_FILEPATH[] = "assets/images/anakin_walking.png",
PADAWON_IDLE_FILEPATH[] = "assets/images/padawon_idle.png",
PADAWON_WALKING_FILEPATH[] = "assets/images/padawon_walking.png",
PADAWON_DEATH_FILEPATH[] = "assets/images/padawon_death.png",
MAP_TILESET_FILEPATH[] = "assets/images/redtileset.png",
BGM_FILEPATH[] = "assets/audio/dooblydoo.mp3",
JUMP_SFX_FILEPATH[] = "assets/audio/bounce.wav",
BACKGROUND_FILEPATH[] = "assets/images/scifi wallpaper.png";

constexpr int NUMBER_OF_TEXTURES = 1;
constexpr GLint LEVEL_OF_DETAIL = 0;
constexpr GLint TEXTURE_BORDER = 0;

unsigned int LEVEL_1_DATA[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 21, 1, 1,
    21, 1, 1, 1, 2, 12, 0, 0, 0, 0, 0, 0, 0, 9,
    7, 8, 8, 8, 8, 8, 1, 1, 1, 1, 1, 1, 1, 9,
    14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 16
};

// ————— VARIABLES ————— //
GameState g_game_state;

SDL_Window* g_display_window;
AppStatus g_app_status = RUNNING;
ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f,
g_accumulator = 0.0f;


void initialise();
void process_input();
void update();
void render();
void shutdown();

// ————— GENERAL FUNCTIONS ————— //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint texture_id;
    glGenTextures(NUMBER_OF_TEXTURES, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return texture_id;
}

void initialise()
{
    // ————— GENERAL ————— //
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    g_display_window = SDL_CreateWindow(GAME_WINDOW_NAME,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);
    if (context == nullptr)
    {
        LOG("ERROR: Could not create OpenGL context.\n");
        shutdown();
    }

#ifdef _WINDOWS
    glewInit();
#endif

    // ————— VIDEO SETUP ————— //
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    // ————— MAP SET-UP ————— //
    GLuint map_texture_id = load_texture(MAP_TILESET_FILEPATH);
    g_game_state.map = new Map(LEVEL1_WIDTH, LEVEL1_HEIGHT, LEVEL_1_DATA, map_texture_id, 1.0f, 7, 3);

    // ————— GEORGE SET-UP ————— //

    std::vector<GLuint> player_texture_ids = {
        load_texture(IDLE_FILEPATH),
        load_texture(WALKING_FILEPATH)
    };

    std::vector<std::vector<int>> anakin_animations = {
        {0, 1, 2, 3, 4},
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}
    };

    glm::vec3 acceleration = glm::vec3(0.0f, -9.81f, 0.0f);

    g_game_state.player = new Entity(
            player_texture_ids,               // texture id
            2.5f,                      // speed
            acceleration,              // acceleration
            5.0f,                      // jumping power
            anakin_animations,                // animation index sets
            0.0f,                      // animation time
            5,                         // animation frame amount
            0,                         // current animation index
            1,                         // animation column amount
            5,                         // animation row amount
            0.7f,                      // width
            1.15f,                      // height
            PLAYER,                    // entity type
            STAND                      // player state
        );

    g_game_state.player->set_scale(glm::vec3(1.0f, 1.25f, 0.0f));

    // Background

    std::vector<GLuint> background_texture_ids = { load_texture(BACKGROUND_FILEPATH) };

    g_game_state.backgrounds = new Entity();
    g_game_state.backgrounds->set_texture_id(background_texture_ids);
    g_game_state.backgrounds->set_player_state(STAND);
    g_game_state.backgrounds->set_position(glm::vec3(0.0f, 0.0f, 0.0f));
    g_game_state.backgrounds->set_width(10.0f);
    g_game_state.backgrounds->set_height(6.0f);
    g_game_state.backgrounds->set_scale(glm::vec3(15.0f, 11.5f, 0.0f));
    g_game_state.backgrounds->set_entity_type(BACKGROUND);

    // Enemies

    std::vector<GLuint> enemy_texture_ids = {
        load_texture(PADAWON_WALKING_FILEPATH),
        load_texture(PADAWON_IDLE_FILEPATH),
        load_texture(PADAWON_DEATH_FILEPATH)
    };

    std::vector<std::vector<int>> padawon_animations = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
        {0, 1, 2, 3, 4},
        {0, 1, 2, 3, 4, 5, 6, 7}
    };

    g_game_state.enemies = new Entity[3];

    g_game_state.enemies[0] = Entity(
        enemy_texture_ids,               // texture id
        2.0f,                      // speed
        acceleration,              // acceleration
        3.0f,                      // jumping power
        padawon_animations,                // animation index sets
        0.0f,                      // animation time
        5,                         // animation frame amount
        0,                         // current animation index
        1,                         // animation column amount
        5,                         // animation row amount
        1.0f,                      // width
        1.0f,                      // height
        ENEMY,                    // entity type
        STAND                      // player state
    );

    g_game_state.enemies[1] = Entity(
        enemy_texture_ids,               // texture id
        2.0f,                      // speed
        acceleration,              // acceleration
        3.0f,                      // jumping power
        padawon_animations,                // animation index sets
        0.0f,                      // animation time
        5,                         // animation frame amount
        0,                         // current animation index
        1,                         // animation column amount
        5,                         // animation row amount
        1.0f,                      // width
        1.0f,                      // height
        ENEMY,                    // entity type
        STAND                      // player state
    );

    g_game_state.enemies[2] = Entity(
        enemy_texture_ids,               // texture id
        2.0f,                      // speed
        acceleration,              // acceleration
        2.0f,                      // jumping power
        padawon_animations,                // animation index sets
        0.0f,                      // animation time
        5,                         // animation frame amount
        0,                         // current animation index
        1,                         // animation column amount
        5,                         // animation row amount
        1.0f,                      // width
        1.0f,                      // height
        ENEMY,                    // entity type
        STAND                      // player state
    );

    for (int i = 0; i < 3; i++) {
        g_game_state.enemies[i].set_ai_type(GUARD);
        g_game_state.enemies[i].set_ai_state(IDLE);
        g_game_state.enemies[i].set_position(glm::vec3(i * 2 + 4, -2, 0));
    }

    g_game_state.enemies[0].set_ai_type(STARE);

    g_game_state.enemies[1].set_ai_state(IDLE);
    g_game_state.enemies[1].set_scale(glm::vec3(1.0f, 1.0f, 0.0f));
    g_game_state.enemies[1].set_movement(glm::vec3(0.0f, 0.0f, 0.0f));
    g_game_state.enemies[1].set_scale(glm::vec3(-1.0f, 1.0f, 0.0f));
    g_game_state.enemies[1].set_position(glm::vec3(12.0f, 0.0f, 0.0f));

    g_game_state.enemies[2].set_ai_state(WALKING);
    g_game_state.enemies[2].set_scale(glm::vec3(0.8f, 1.0f, 0.0f));
    g_game_state.enemies[2].set_movement(glm::vec3(1.0f, 0.0f, 0.0f));
    g_game_state.enemies[2].set_position(glm::vec3(10.0f, -3.0f, 0.0f));

    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

    g_game_state.bgm = Mix_LoadMUS(BGM_FILEPATH);
    //    Mix_PlayMusic(g_game_state.bgm, -1);
    //    Mix_VolumeMusic(MIX_MAX_VOLUME / 16.0f);

    g_game_state.jump_sfx = Mix_LoadWAV(JUMP_SFX_FILEPATH);

    // ————— BLENDING ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    g_game_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_app_status = TERMINATED;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_app_status = TERMINATED;
                break;

            case SDLK_SPACE:
                // Jump
                if (g_game_state.player->get_collided_bottom())
                {
                    g_game_state.player->jump();
                    //Mix_PlayChannel(-1, g_game_state.jump_sfx, 0);
                }
                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);
    g_game_state.player->set_player_state(STAND);
    if (key_state[SDL_SCANCODE_A])       g_game_state.player->move_left(), g_game_state.player->set_player_state(WALK), g_game_state.player->set_scale(glm::vec3(-1.0f, 1.25f, 0.0f));
    else if (key_state[SDL_SCANCODE_D]) g_game_state.player->move_right(), g_game_state.player->set_player_state(WALK), g_game_state.player->set_scale(glm::vec3(1.0f, 1.25f, 0.0f));

    if (key_state[SDL_SCANCODE_K]) { g_game_state.enemies[0].set_ai_state(DYING), g_game_state.enemies[0].set_scale(glm::vec3(1.4f, 1.0f, 0.0f)); }

    if (glm::length(g_game_state.player->get_movement()) > 1.0f)
        g_game_state.player->normalise_movement();
}

void update()
{
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_accumulator;

    if (delta_time < FIXED_TIMESTEP)
    {
        g_accumulator = delta_time;
        return;
    }

    while (delta_time >= FIXED_TIMESTEP)
    {
        g_game_state.player->update(FIXED_TIMESTEP, g_game_state.player, g_game_state.enemies, 3,
            g_game_state.map);
        for (int i = 0; i < 3; i++) { g_game_state.enemies[i].update(FIXED_TIMESTEP, g_game_state.player, g_game_state.player, 1, g_game_state.map); }
        delta_time -= FIXED_TIMESTEP;
    }

    g_accumulator = delta_time;

    g_view_matrix = glm::mat4(1.0f);

    // Camera Follows the player
    g_view_matrix = glm::translate(g_view_matrix, glm::vec3(-g_game_state.player->get_position().x, 0.0f, 0.0f));

    g_game_state.backgrounds->set_position(glm::vec3(g_game_state.player->get_position().x + (3.5 - g_game_state.player->get_position().x)/7, 0.0f, 0.0f));
    g_game_state.backgrounds->update(0.0f, g_game_state.player, NULL, 0, g_game_state.map);
}

void render()
{
    g_shader_program.set_view_matrix(g_view_matrix);

    glClear(GL_COLOR_BUFFER_BIT);

    g_game_state.backgrounds->render(&g_shader_program);
    g_game_state.player->render(&g_shader_program);
    for (int i = 0; i < 3; i++) { g_game_state.enemies[i].render(&g_shader_program); }
    g_game_state.map->render(&g_shader_program);

    SDL_GL_SwapWindow(g_display_window);
}

void shutdown()
{
    SDL_Quit();

    delete[] g_game_state.enemies;
    delete    g_game_state.player;
    delete    g_game_state.map;
    Mix_FreeChunk(g_game_state.jump_sfx);
    Mix_FreeMusic(g_game_state.bgm);
}

// ————— GAME LOOP ————— //
int main(int argc, char* argv[])
{
    initialise();

    while (g_app_status == RUNNING)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}