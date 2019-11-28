#pragma once

#include "renderer.hpp"
#include "sparsevoxeloctree.hpp"

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