#pragma once
// Metrics are the measurement instrument and run on the product configuration.
// The verification half below adds synthetic data and the boot-time pixel
// check, so it stays behind its own macro (plan.md work 7).
//
// This is the app's side of the measurement: frames are classified by the
// activity the app composed, and timed around the whole HostRenderer::draw,
// so the shared Renderer knows nothing of screens or of these figures.
#ifdef LAUNCHER_RENDER_METRICS
#include "host/FrameModel.h"
#include "host/RenderPort.h"
#include "features/home/DisplayDataSource.h"
#include "multifirm/SlotCatalog.h"
#include <M5GFX.h>
namespace launcher {
class HostRenderer;
// `end` is taken after the frame's endWrite, so it covers the transfer.
void recordRender(FrameActivity activity,TimeUs start,TimeUs end,bool painted);
void recordInput(TimeUs now);
void recordWake(TimeUs now);
// One pass of the main loop; idle passes are what work 8-4 removes.
void recordLoop();
void reportRenderDiagnostics(const HostRenderer&,TimeUs now);
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
class DiagnosticDataSource final : public DisplayDataSource {
public:
    WatchData sample(TimeUs now) override;
};
void runRepaintCheck(HostRenderer&,M5GFX&,const SlotCatalog&);
#endif
}
#endif
