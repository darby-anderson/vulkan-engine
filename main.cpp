#include "Common.hpp"
#include "Engine.hpp"

int main() {
    fmt::print("Wow!");

    Engine engine;
    engine.init();
    engine.run();
}