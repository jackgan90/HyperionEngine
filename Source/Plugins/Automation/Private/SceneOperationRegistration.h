#pragma once
#include "SceneOperations.h"

namespace Hyperion
{
// Metadata stays with each operation family; typed wire adaptation uses the common automation error boundary.
template<class TRequest, class TResult, class TFunction>
void RegisterSceneOperation(FOperationCatalog& InCatalog, FOperationInfo InInfo, const FRecordDescriptor& InRequestType,
                            const FRecordDescriptor& InResultType, const TRequest& InExample, TFunction InFunction)
{
	InInfo.Example = WriteRecordWire(InRequestType, &InExample);
	InCatalog.Register({std::move(InInfo), &InRequestType, &InResultType, false,
	                    [Function = std::move(InFunction), ResultType = &InResultType](const void* InRequest)
	                    {
		                    return InvokeAutomation(
		                        [&]() -> FOperationTask
		                        {
			                        const TResult Result = Function(*static_cast<const TRequest*>(InRequest));
			                        return {WriteRecordWire(*ResultType, &Result)};
		                        });
	                    }});
}

template<class TRequest, class TResult, class TFunction>
void RegisterSceneOperation(FOperationCatalog& InCatalog, FOperationInfo InInfo, const TRequest& InExample,
                            TFunction InFunction)
{
	RegisterSceneOperation<TRequest, TResult>(InCatalog, std::move(InInfo), RecordType<TRequest>(),
	                                          RecordType<TResult>(), InExample, std::move(InFunction));
}
} // namespace Hyperion
