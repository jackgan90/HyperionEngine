#pragma once
#include "Hyperion/Automation/Catalog.h"

namespace Hyperion
{
// Metadata and domain logic belong to the caller; request/result descriptors must outlive the catalog.
template<class TRequest, class TResult, class TFunction>
FOperationDescriptor MakeWireOperation(FOperationInfo InInfo, const FRecordDescriptor& InRequestType,
                                       const FRecordDescriptor& InResultType, const TRequest& InExample,
                                       TFunction InFunction)
{
	InInfo.Example = WriteRecordWire(InRequestType, &InExample);
	return {std::move(InInfo), &InRequestType, &InResultType, false,
	        [Function = std::move(InFunction), ResultType = &InResultType](const void* InRequest)
	        {
		        return InvokeAutomation(
		            [&]() -> FOperationTask
		            {
			            const TResult Result = Function(*static_cast<const TRequest*>(InRequest));
			            return {WriteRecordWire(*ResultType, &Result)};
		            });
	        }};
}

template<class TRequest, class TResult, class TFunction>
void RegisterSceneOperation(FOperationCatalog& InCatalog, FOperationInfo InInfo, const FRecordDescriptor& InRequestType,
                            const FRecordDescriptor& InResultType, const TRequest& InExample, TFunction InFunction)
{
	InCatalog.Register(MakeWireOperation<TRequest, TResult>(std::move(InInfo), InRequestType, InResultType, InExample,
	                                                        std::move(InFunction)));
}

template<class TRequest, class TResult, class TFunction>
void RegisterSceneOperation(FOperationCatalog& InCatalog, FOperationInfo InInfo, const TRequest& InExample,
                            TFunction InFunction)
{
	RegisterSceneOperation<TRequest, TResult>(InCatalog, std::move(InInfo), RecordType<TRequest>(),
	                                          RecordType<TResult>(), InExample, std::move(InFunction));
}
} // namespace Hyperion
