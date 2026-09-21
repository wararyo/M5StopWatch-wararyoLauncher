#pragma once
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
#include "DisplayModel.h"
#include <M5GFX.h>
namespace launcher {
class Renderer;
class DiagnosticDataSource final : public DisplayDataSource {
public:
    WatchData sample(TimeUs now) override;
};
void runRepaintCheck(Renderer&,M5GFX&,const SlotCatalog&);
void recordRender(const ScreenModel&,TimeUs start,TimeUs end,bool painted,uint32_t layouts);
void recordInput(TimeUs now);
void recordWake(TimeUs now);
void reportRenderDiagnostics(const Renderer&,TimeUs now);
}
#endif
