#pragma once
// Metrics are the measurement instrument and run on the product configuration.
// The verification half below adds synthetic data and the boot-time pixel
// check, so it stays behind its own macro (plan.md work 7).
#ifdef LAUNCHER_RENDER_METRICS
#include "DisplayModel.h"
#include <M5GFX.h>
namespace launcher {
class Renderer;
void recordRender(const ScreenModel&,TimeUs start,TimeUs end,bool painted,uint32_t layouts);
void recordInput(TimeUs now);
void recordWake(TimeUs now);
// One pass of the main loop; idle passes are what work 8-4 removes.
void recordLoop();
void reportRenderDiagnostics(const Renderer&,TimeUs now);
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
class DiagnosticDataSource final : public DisplayDataSource {
public:
    WatchData sample(TimeUs now) override;
};
void runRepaintCheck(Renderer&,M5GFX&,const SlotCatalog&);
#endif
}
#endif
