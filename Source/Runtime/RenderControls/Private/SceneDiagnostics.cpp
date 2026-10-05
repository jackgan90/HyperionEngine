#include "Hyperion/RenderControls/SceneDiagnostics.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FScenePrimitiveDiagnostic>()
{
	static const auto Type = MakeRecord<FScenePrimitiveDiagnostic>(
	    "hyperion.primitive-diagnostics",
	    {Member("primitive", &FScenePrimitiveDiagnostic::Primitive), Member("name", &FScenePrimitiveDiagnostic::Name),
	     Member("appliedRevision", &FScenePrimitiveDiagnostic::AppliedRevision),
	     Member("appliedWorld", &FScenePrimitiveDiagnostic::AppliedWorld),
	     Member("appliedVisible", &FScenePrimitiveDiagnostic::bAppliedVisible),
	     Member("status", &FScenePrimitiveDiagnostic::Status),
	     Member("lastDrawFrame", &FScenePrimitiveDiagnostic::LastDrawFrame),
	     Member("lastDrawView", &FScenePrimitiveDiagnostic::LastDrawView),
	     Member("lastDrawPass", &FScenePrimitiveDiagnostic::LastDrawPass),
	     Member("lastDrawReady", &FScenePrimitiveDiagnostic::bLastDrawReady),
	     Member("error", &FScenePrimitiveDiagnostic::Error),
	     Member("renderSlot", &FScenePrimitiveDiagnostic::RenderSlot),
	     Member("renderGeneration", &FScenePrimitiveDiagnostic::RenderGeneration)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneComponentDiagnostics>()
{
	static const auto Type = MakeRecord<FSceneComponentDiagnostics>(
	    "hyperion.component-diagnostics",
	    {Member("expectedRevision", &FSceneComponentDiagnostics::ExpectedPrimitiveRevision,
	            {.bPersistent = false, .Inspector = FPropertyPresentation{"Expected render revision", {}, true}}),
	     Member(
	         "applied", &FSceneComponentDiagnostics::bApplied,
	         {.bPersistent = false, .Inspector = FPropertyPresentation{"Render has applied this revision", {}, true}}),
	     Member("primitives", &FSceneComponentDiagnostics::Primitives,
	            {.bPersistent = false, .Inspector = FPropertyPresentation{"Render primitives", {}, true}})});
	return Type;
}
} // namespace Hyperion
