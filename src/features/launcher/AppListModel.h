#pragma once
namespace launcher {
struct AppListModel {
    int selection=0;
    float transition=0,scroll=0;
    bool dragging=false,animating=false;
    const char* names[5]{};
    bool rowDimmed[5]{};
};
}
