#include "Hyperion/Config/ApplicationSettings.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FApplicationSettingsState>()
{
	static const auto Type = MakeRecord<FApplicationSettingsState>(
	    "hyperion.applicationsettings.state",
	    {Member("values", &FApplicationSettingsState::Values), Member("revision", &FApplicationSettingsState::Revision),
	     Member("activeReversedZ", &FApplicationSettingsState::bActiveReversedZ)});
	return Type;
}
} // namespace Hyperion
