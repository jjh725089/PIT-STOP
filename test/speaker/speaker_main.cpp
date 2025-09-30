// Standard Library
#include <iostream>
#include <csignal>

// Project Headers
#include "speaker.h"

// Global signal control
std::atomic<bool> interrupted{ false };
Speaker* global_speaker = nullptr;

// Signal handler for graceful shutdown (SIGINT)
void handleSigint(int)
{
    if (!interrupted.exchange(true))
    {
        std::cerr << "[Speaker] SIGINT received. Requesting shutdown..." << std::endl;
        if (global_speaker)
        {
            global_speaker->requestShutdown();
        }
    }
}

// Entry point for the speaker process (Example)
int main()
{
    Speaker speaker;
    global_speaker = &speaker;

    std::signal(SIGINT, handleSigint);

    std::cout << "[Speaker] Initializing server..." << std::endl;
    if (!speaker.init())
    {
        std::cerr << "[Speaker] Failed to initialize server." << std::endl;
        return 1;
    }

    std::cout << "[Speaker] Running. Press Ctrl+C to stop." << std::endl;

    try
    {
        speaker.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Speaker] Exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "[Speaker] Unknown exception occurred." << std::endl;
    }

    std::cout << "[Speaker] Shutting down..." << std::endl;
    speaker.stop();
    std::cout << "[Speaker] Shutdown complete." << std::endl;

    return 0;
}