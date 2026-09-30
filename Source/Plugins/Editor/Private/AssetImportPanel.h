#pragma once
#include "EditorPreferences.h"
#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/Gui/Gui.h"
#include <functional>

namespace Hyperion
{
class FAssetImportPanel
{
public:
	FAssetImportPanel(FAssetImportWorkspace* InImports, FContentRootService& InRoots, FEditorPreferences& InPreferences,
	                  std::function<void()> InSavePreferences);
	void Draw(FGui& InGui);
	void Process(FNativeSurface InOwner);
	void Show();
	void SetOutputDirectory(const std::filesystem::path& InDirectory);
	std::string OutputFolder;
	bool bOpen{};
	FImportRequest Request;
	FVec4 SourceBounds{};
	FVec4 OutputBounds{};
	FVec4 ImportBounds{};
	FVec4 CloseBounds{};
	FVec4 PreviewNameBounds{};
	FVec4 UndoBounds{};
	FVec4 RedoBounds{};
	FVec4 RefreshApplyBounds{};
	FVec4 RefreshCancelBounds{};
	FVec4 ImportResultBounds{};
	std::string DraftId;

private:
	void DrawSource(FGui& InGui);
	void DrawSettings(FGui& InGui);
	void DrawOutput(FGui& InGui);
	void DrawImportAction(FGui& InGui, const std::string& InMessage);
	std::string ImportMessage() const;
	void DrawOutputPath(FGui& InGui);
	void UpdateSource();
	void BrowseOutput(FNativeSurface InOwner);
	FImportRequest Snapshot() const;
	std::string RequestKey() const;
	void ProcessDraft();
	void SubmitImport();
	void ProcessImportResult();
	void DrawImportResult(FGui& InGui);
	void ShowImportResult(std::string InMessage);
	void DrawImportProperties(FGui& InGui);
	void DrawPropertyHistory(FGui& InGui);
	void DrawRefreshConfirmation(FGui& InGui);
	void RestorePreparedRequest();
	void DrawDraftProperties(FGui& InGui, FImportDraftInfo& InInfo);
	void DrawDraftModel(FGui& InGui, FImportDraftInfo& InInfo);
	void ApplyDraftEdit(const FImportDraftInfo& InInfo, FImportPropertyEdits InEdits);
	FImportDraftInfo PreviewInfo;
	std::string PreparedKey;
	std::string ObservedRequestKey;
	FImportRequest PreparedRequest;
	bool bSettingsEditing{};
	bool bConfirmRefresh{};
	bool bRefreshDialog{};
	bool bSourceEditing{};
	bool bAutoPrepare{};
	bool bRefreshDraft{};
	std::uint32_t PreviewOffset{};
	std::size_t SelectedNode{};
	std::size_t SelectedPrimitive{};
	FAssetImportWorkspace* Imports{};
	FContentRootService& Roots;
	FEditorPreferences& Preferences;
	std::function<void()> SavePreferences;
	std::string PreviousSource;
	std::string Message;
	std::string ImportTask;
	std::string ImportResultMessage;
	bool bImportResultPending{};
	bool bImportResultOpen{};
	std::string OutputError;
	std::string ValidatedOutputKey;
	std::size_t EncodingIndex = 1;
	bool bBrowse{};
	bool bBrowseOutput{};
	bool bFocus{};
	FEnvironmentBakeSettings Bake;
};
} // namespace Hyperion
