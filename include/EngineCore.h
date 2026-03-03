#pragma once

/**
 * Engine entry point and Application base class.
 * Game projects inherit from Engine::Application and implement Init/Run/Shutdown.
 */

namespace Engine {

class Application {
public:
    virtual ~Application() = default;
    virtual void Init() {}
    virtual void Run() {}
    virtual void Shutdown() {}
};

} // namespace Engine
