#pragma once

#include "renderer.hpp"

class Engine
{
  public:
    void init();
    void update();
    void cleanup();

    bool isRunning() { return renderer.isRunning(); }

  private:
    Renderer renderer;

};