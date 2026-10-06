#include "framework.h"
#include "app.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCommand) {
    SetProcessDPIAware();

    App app(instance);
    if (!app.Initialize(showCommand)) return 1;
    return app.Run();
}
