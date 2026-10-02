#include "SceneTestTarget.h"

void CheckSceneComponentDomainCase(Hyperion::FTaskSystem& InTasks, Hyperion::ISceneEditTarget& InTarget,
                                   Hyperion::FScene& InScene, unsigned InCase);

void CheckSceneComponentDomainChanges()
{
	Hyperion::FTaskSystem Tasks(1, 1);
	for (unsigned Case = 0; Case < 5; ++Case)
	{
		Hyperion::FSceneTestTarget Target(Tasks);
		CheckSceneComponentDomainCase(Tasks, Target, Target.Scene, Case);
	}
}
