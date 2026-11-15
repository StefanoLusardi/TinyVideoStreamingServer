// Entrypoint: solo parsing degli argomenti e avvio. Tutta la logica sta nella libreria.

#include <mjpegproxy/Server.hpp>
#include <mjpegproxy/Settings.hpp>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace mjpegproxy;

    const std::vector<std::string> args(argv + 1, argv + argc);
    if (std::ranges::any_of(args, [](const std::string& a) { return a == "-h" || a == "--help"; })) {
        std::fputs(Settings::usage(argv[0]).c_str(), stdout);
        return 0;
    }

    Settings settings;
    try {
        settings = Settings::parse(args);
    } catch (const std::invalid_argument& e) {
        std::fprintf(stderr, "errore: %s\n\n%s", e.what(), Settings::usage(argv[0]).c_str());
        return 2;
    }

    try {
        Server server(settings);
        server.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "errore: %s\n", e.what());
        return 1;
    }
    return 0;
}
