#include "engine.hpp"

void Engine::init() { renderer.init(); }

void Engine::update()
{
    renderer.updateWindow();
    renderer.render();
}

void Engine::cleanup() { renderer.cleanup(); }
