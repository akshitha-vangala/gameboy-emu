#include <SDL.h>
#include <cstdint>
#include <cstdio>

constexpr int GB_W = 160, GB_H = 144, SCALE = 4;

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("gb-emu", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, GB_W * SCALE, GB_H * SCALE, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, GB_W, GB_H);

    uint32_t framebuffer[GB_W * GB_H];
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT) running = false;

        // placeholder: a gradient (the PPU will fill this later)
        for (int y = 0; y < GB_H; y++)
            for (int x = 0; x < GB_W; x++)
                framebuffer[y * GB_W + x] = 0xFF000000 | (x * 255 / GB_W) << 8;

        SDL_UpdateTexture(tex, nullptr, framebuffer, GB_W * sizeof(uint32_t));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    }
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}