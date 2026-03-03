// Game DLL plugin - implements IGameInstance for hot-reload
#include <flecs.h>

// IGameInstance interface - Game DLL exports this
extern "C" {

void Game_RegisterSystems(flecs::world* world) {
    if (world) {
        // Game-specific Flecs systems registered here
    }
}

void Game_Init() {}
void Game_Shutdown() {}

}
