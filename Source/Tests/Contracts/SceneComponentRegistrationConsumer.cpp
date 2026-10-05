#include "Hyperion/AutomationHost/SceneComponentOperations.h"

int main()
{
	Hyperion::FOperationCatalog Catalog;
	Hyperion::RegisterSceneComponentOperations<Hyperion::FSceneTransform>(Catalog, nullptr,
	                                                                      {"public-consumer", true, true});
	Catalog.Seal();
	return Catalog.Size() == 3 ? 0 : 1;
}
