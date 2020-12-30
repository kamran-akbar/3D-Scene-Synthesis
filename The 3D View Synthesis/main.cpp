#include "application.h"

int main()
{
    std::unique_ptr<SceneSynthesis::application> app= std::unique_ptr<SceneSynthesis::application>
        (new SceneSynthesis::application(800, 600));
    app->run();

    return 0;
}