//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
#include "WinConfiguration.h"
#include "Common.h"
#include "Exceptions.h"
#include "Bookmarks.h"
#include "Terminal.h"
#include "TextsWin.h"
#include "WinInterface.h"
#include "GUITools.h"
#include "Tools.h"
#include "Setup.h"
#include "Security.h"
#include "TerminalManager.h"
#include "Cryptography.h"
#include <VCLCommon.h>
#include <LanguagesDEPfix.hpp>
#include <InitGUID.h>
#include <DragExt.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
__fastcall TEditorData::TEditorData() :
  FileMask(L"*.*"),
  Editor(edInternal),
  ExternalEditor(L""),
  ExternalEditorText(true),
  SDIExternalEditor(false),
  DetectMDIExternalEditor(false)
{
}
//---------------------------------------------------------------------------
__fastcall TEditorData::TEditorData(const TEditorData & Source) :
  FileMask(Source.FileMask),
  Editor(Source.Editor),
  ExternalEditor(Source.ExternalEditor),
  ExternalEditorText(Source.ExternalEditorText),
  SDIExternalEditor(Source.SDIExternalEditor),
  DetectMDIExternalEditor(Source.DetectMDIExternalEditor)
{
}
//---------------------------------------------------------------------------
#define C(Property) (Property == rhd.Property)
bool __fastcall TEditorData::operator==(const TEditorData & rhd) const
{
  return
    C(FileMask) &&
    C(Editor) &&
    C(ExternalEditor) &&
    C(ExternalEditorText) &&
    C(SDIExternalEditor) &&
    C(DetectMDIExternalEditor) &&
    true;
}
#undef C
//---------------------------------------------------------------------------
__fastcall TEditorPreferences::TEditorPreferences()
{
}
//---------------------------------------------------------------------------
__fastcall TEditorPreferences::TEditorPreferences(const TEditorData & Data) :
  FData(Data)
{
}
//---------------------------------------------------------------------------
bool __fastcall TEditorPreferences::operator==(const TEditorPreferences & rhp) const
{
  return (FData == rhp.FData);
}
#undef C
//---------------------------------------------------------------------------
bool __fastcall TEditorPreferences::Matches(const UnicodeString FileName,
  bool Local, const TFileMasks::TParams & Params) const
{
  return FData.FileMask.Matches(FileName, Local, false, &Params);
}
//---------------------------------------------------------------------------
void __fastcall TEditorPreferences::LegacyDefaults()
{
  FData.ExternalEditor = L"notepad.exe";
}
//---------------------------------------------------------------------------
void __fastcall TEditorPreferences::Load(THierarchicalStorage * Storage, bool Legacy)
{
  if (!Legacy)
  {
    FData.FileMask = Storage->ReadString(L"FileMask", FData.FileMask.Masks);
  }
  FData.Editor = (TEditor)Storage->ReadInteger(L"Editor", FData.Editor);
  FData.ExternalEditor = Storage->ReadString(L"ExternalEditor", FData.ExternalEditor);
  FData.ExternalEditorText = Storage->ReadBool(L"ExternalEditorText", FData.ExternalEditorText);
  FData.SDIExternalEditor = Storage->ReadBool(L"SDIExternalEditor", FData.SDIExternalEditor);
  FData.DetectMDIExternalEditor = Storage->ReadBool(L"DetectMDIExternalEditor", FData.DetectMDIExternalEditor);
}
//---------------------------------------------------------------------------
void __fastcall TEditorPreferences::Save(THierarchicalStorage * Storage) const
{
  Storage->WriteString(L"FileMask", FData.FileMask.Masks);
  Storage->WriteInteger(L"Editor", FData.Editor);
  Storage->WriteString(L"ExternalEditor", FData.ExternalEditor);
  Storage->WriteBool(L"ExternalEditorText", FData.ExternalEditorText);
  Storage->WriteBool(L"SDIExternalEditor", FData.SDIExternalEditor);
  Storage->WriteBool(L"DetectMDIExternalEditor", FData.DetectMDIExternalEditor);
}
//---------------------------------------------------------------------------
TEditorData * __fastcall TEditorPreferences::GetData()
{
  // returning non-const data, possible data change, invalidate cached name
  FName = L"";
  return &FData;
};
//---------------------------------------------------------------------------
UnicodeString __fastcall TEditorPreferences::GetName() const
{
  if (FName.IsEmpty())
  {
    if (FData.Editor == edInternal)
    {
      // StripHotkey is relic from times when INTERNAL_EDITOR_NAME was used
      // also for the menu item caption
      FName = StripHotkey(LoadStr(INTERNAL_EDITOR_NAME));
    }
    else if (FData.Editor == edOpen)
    {
      FName = StripHotkey(LoadStr(OPEN_EDITOR_NAME));
    }
    else
    {
      UnicodeString Program, Params, Dir;
      UnicodeString ExternalEditor = FData.ExternalEditor;
      ReformatFileNameCommand(ExternalEditor);
      SplitCommand(ExternalEditor, Program, Params, Dir);
      FName = ExtractFileName(Program);
      int P = FName.LastDelimiter(L".");
      if (P > 0)
      {
        FName.SetLength(P - 1);
      }

      if (FName.ByteType(1) == mbSingleByte)
      {
        if (FName.UpperCase() == FName)
        {
          FName = FName.LowerCase();
        }

        if (FName.LowerCase() == FName)
        {
          FName = FName.SubString(1, 1).UpperCase() +
            FName.SubString(2, FName.Length() - 1);
        }
      }
    }
  }

  return FName;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
__fastcall TEditorList::TEditorList()
{
  Init();
}
//---------------------------------------------------------------------------
void __fastcall TEditorList::Init()
{
  FEditors = new TList();
  FModified = false;
}
//---------------------------------------------------------------------------
__fastcall TEditorList::~TEditorList()
{
  Clear();
  delete FEditors;
}
//---------------------------------------------------------------------
void __fastcall TEditorList::Modify()
{
  FModified = true;
}
//---------------------------------------------------------------------------
void __fastcall TEditorList::Saved()
{
  FModified = false;
}
//---------------------------------------------------------------------------
TEditorList & __fastcall TEditorList::operator=(const TEditorList & rhl)
{
  Clear();

  for (int Index = 0; Index < rhl.Count; Index++)
  {
    Add(new TEditorPreferences(*rhl.Editors[Index]));
  }
  // there should be comparison of with the assigned list, but we rely on caller
  // to do it instead (TWinConfiguration::SetEditorList)
  Modify();
  return *this;
}
//---------------------------------------------------------------------------
bool __fastcall TEditorList::operator==(const TEditorList & rhl) const
{
  bool Result = (Count == rhl.Count);
  if (Result)
  {
    int i = 0;
    while ((i < Count) && Result)
    {
      Result = (*Editors[i]) == (*rhl.Editors[i]);
      i++;
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TEditorList::Clear()
{
  for (int i = 0; i < Count; i++)
  {
    delete Editors[i];
  }
  FEditors->Clear();
}
//---------------------------------------------------------------------------
void __fastcall TEditorList::Add(TEditorPreferences * Editor)
{
  Insert(Count, Editor);
}
//---------------------------------------------------------------------------
void __fastcall TEditorList::Insert(int Index, TEditorPreferences * Editor)
{
  FEditors->Insert(Index, reinterpret_cast<TObject *>(Editor));
  Modify();
}
//---------------------------------------------------------------------------
void __fastcall TEditorList::Change(int Index, TEditorPreferences * Editor)
{
  if (!((*Editors[Index]) == *Editor))
  {
    delete Editors[Index];
    FEditors->Items[Index] = (reinterpret_cast<TObject *>(Editor));
    Modify();
  }
  else
  {
    delete Editor;
  }
}
//---------------------------------------------------------------------------
void __fastcall TEditorList::Move(int CurIndex, int NewIndex)
{
  if (CurIndex != NewIndex)
  {
    FEditors->Move(CurIndex, NewIndex);
    Modify();
  }
}
//---------------------------------------------------------------------------
void __fastcall TEditorList::Delete(int Index)
{
  assert((Index >= 0) && (Index < Count));
  delete Editors[Index];
  FEditors->Delete(Index);
  Modify();
}
//---------------------------------------------------------------------------
const TEditorPreferences * __fastcall TEditorList::Find(
  const UnicodeString FileName, bool Local, const TFileMasks::TParams & Params) const
{
  const TEditorPreferences * Result = NULL;
  int i = 0;
  while ((i < FEditors->Count) && (Result == NULL))
  {
    Result = Editors[i];
    if (!Result->Matches(FileName, Local, Params))
    {
      Result = NULL;
    }
    i++;
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TEditorList::Load(THierarchicalStorage * Storage)
{
  int Index = 0;
  bool Next;

  do
  {
    UnicodeString Name = IntToStr(Index);
    TEditorPreferences * Editor = NULL;
    try
    {
      Next = Storage->OpenSubKey(Name, false);
      if (Next)
      {
        try
        {
          Editor = new TEditorPreferences();
          Editor->Load(Storage, false);
        }
        __finally
        {
          Storage->CloseSubKey();
        }
      }
    }
    catch(...)
    {
      delete Editor;
      throw;
    }

    if (Editor != NULL)
    {
      FEditors->Add(reinterpret_cast<TObject *>(Editor));
    }

    Index++;
  }
  while (Next);

  FModified = false;
}
//---------------------------------------------------------------------------
void __fastcall TEditorList::Save(THierarchicalStorage * Storage) const
{
  Storage->ClearSubKeys();
  for (int Index = 0; Index < Count; Index++)
  {
    if (Storage->OpenSubKey(IntToStr(Index), true))
    {
      try
      {
        Editors[Index]->Save(Storage);
      }
      __finally
      {
        Storage->CloseSubKey();
      }
    }
  }
}
//---------------------------------------------------------------------------
int __fastcall TEditorList::GetCount() const
{
  int X = FEditors->Count;
  return X;
}
//---------------------------------------------------------------------------
const TEditorPreferences * __fastcall TEditorList::GetEditor(int Index) const
{
  return reinterpret_cast<TEditorPreferences *>(FEditors->Items[Index]);
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
__fastcall TWinConfiguration::TWinConfiguration(): TCustomWinConfiguration()
{
  FInvalidDefaultTranslationMessage = L"";
  FDDExtInstalled = -1;
  FBookmarks = new TBookmarks();
  FCustomCommandList = new TCustomCommandList();
  FEditorList = new TEditorList();
  FDefaultUpdatesPeriod = 0;
  FDontDecryptPasswords = 0;
  Default();

  try
  {
    CheckTranslationVersion(
      GetResourceModuleName(Application->Name, ModuleFileName().c_str()), true);
  }
  catch(Exception & E)
  {
    FInvalidDefaultTranslationMessage = E.Message;
  }
}
//---------------------------------------------------------------------------
__fastcall TWinConfiguration::~TWinConfiguration()
{
  if (!FTemporarySessionFile.IsEmpty()) DeleteFile(FTemporarySessionFile);
  ClearTemporaryLoginData();

  delete FBookmarks;
  delete FCustomCommandList;
  delete FEditorList;
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::Default()
{
  FCustomCommandsDefaults = true;

  TCustomWinConfiguration::Default();

  FDDAllowMove = false;
  FDDAllowMoveInit = false;
  FDDTransferConfirmation = true;
  FDDTemporaryDirectory = L"";
  FDDWarnLackOfTempSpace = true;
  FDDWarnLackOfTempSpaceRatio = 1.1;
  FDDExtEnabled = DDExtInstalled;
  FDDExtTimeout = MSecsPerSec;
  FDeleteToRecycleBin = true;
  FSelectDirectories = false;
  FSelectMask = L"*.*";
  FShowHiddenFiles = true;
  FFormatSizeBytes = true;
  FShowInaccesibleDirectories = true;
  FConfirmTransferring = true;
  FConfirmDeleting = true;
  FConfirmRecycling = true;
  FConfirmClosingSession = true;
  FDoubleClickAction = dcaEdit;
  FCopyOnDoubleClickConfirmation = false;
  FDimmHiddenFiles = true;
  FRenameWholeName = false;
  FAutoStartSession = L"";
  FExpertMode = true;
  FUseLocationProfiles = false;
  FUseSharedBookmarks = false;
  FDefaultDirIsHome = true;
  FDDDeleteDelay = 120;
  FTemporaryDirectoryAppendSession = false;
  FTemporaryDirectoryAppendPath = true;
  FTemporaryDirectoryCleanup = true;
  FConfirmTemporaryDirectoryCleanup = true;
  FPreservePanelState = true;
  FTheme = L"OfficeXP";
  FLastStoredSession = L"";
  FPathInCaption = picShort;
  FMinimizeToTray = false;
  FBalloonNotifications = true;
  FNotificationsTimeout = 10;
  FNotificationsStickTime = 2;
  FCopyParamAutoSelectNotice = true;
  FLockToolbars = false;
  FAutoOpenInPutty = false;
  FRefreshRemotePanel = false;
  FRefreshRemotePanelInterval = TDateTime(0, 1, 0, 0);
  FFullRowSelect = false;
  FVersionHistory = L"";
  AddVersionToHistory();
  FUseMasterPassword = false;
  FPlainMasterPasswordEncrypt = L"";
  FPlainMasterPasswordDecrypt = L"";
  FMasterPasswordVerifier = L"";
  FOpenedStoredSessionFolders = L"";

  FEditor.FontName = L"Courier New";
  FEditor.FontHeight = -12;
  FEditor.FontStyle = 0;
  FEditor.FontCharset = DEFAULT_CHARSET;
  FEditor.WordWrap = false;
  FEditor.FindText = L"";
  FEditor.ReplaceText = L"";
  FEditor.FindMatchCase = false;
  FEditor.FindWholeWord = false;
  FEditor.FindDown = true;
  FEditor.TabSize = 7;
  FEditor.MaxEditors = 500;
  FEditor.EarlyClose = 2; // seconds
  FEditor.SDIShellEditor = false;
  FEditor.WindowParams = L"";
  FEditor.Encoding = CP_ACP;

  FQueueView.Height = 100;
  FQueueView.Layout = L"70,160,160,80,80,80";
  FQueueView.Show = qvHideWhenEmpty;
  FQueueView.LastHideShow = qvHideWhenEmpty;
  FQueueView.ToolBar = false;

  FEnableQueueByDefault = true;

  FUpdates.Period = FDefaultUpdatesPeriod;
  FUpdates.LastCheck = 0;
  FUpdates.HaveResults = false;
  FUpdates.ShownResults = false;
  FUpdates.BetaVersions = asAuto;
  // for backward compatibility the default is decided based on value of ProxyHost
  FUpdates.ConnectionType = (TConnectionType)-1;
  FUpdates.ProxyHost = L""; // keep empty (see above)
  FUpdates.ProxyPort = 8080;
  FUpdates.Results.Reset();
  FUpdates.DotNetVersion = "";
  FUpdates.ConsoleVersion = "";

  FLogWindowOnStartup = true;
  FLogWindowParams = L"-1;-1;500;400";

  bool BigResolution = ((Screen->Width > 1000) && (Screen->Height > 700));

  FScpExplorer.WindowParams = BigResolution ? L"-1;-1;860;520;0" : L"-1;-1;600;400;0";
  FScpExplorer.DirViewParams = L"0;1;0|150,1;70,1;120,1;79,1;62,1;55,1;20,0;150,0;125,0|6;7;8;0;1;2;3;4;5";
  FScpExplorer.ToolbarsLayout =
    L"Queue_Visible=1,Queue_LastDock=QueueDock,Queue_DockRow=0,Queue_DockPos=-1,Queue_FloatLeft=0,Queue_FloatTop=0,Queue_FloatRightX=0,"
     "Menu_Visible=1,Menu_DockedTo=TopDock,Menu_LastDock=TopDock,Menu_DockRow=0,Menu_DockPos=0,Menu_FloatLeft=0,Menu_FloatTop=0,Menu_FloatRightX=0,"
     "Buttons_Visible=1,Buttons_DockedTo=TopDock,Buttons_LastDock=TopDock,Buttons_DockRow=2,Buttons_DockPos=0,Buttons_FloatLeft=0,Buttons_FloatTop=0,Buttons_FloatRightX=0,"
     "Selection_Visible=0,Selection_DockedTo=TopDock,Selection_LastDock=TopDock,Selection_DockRow=3,Selection_DockPos=0,Selection_FloatLeft=227,Selection_FloatTop=445,Selection_FloatRightX=0,"
     "Session_Visible=0,Session_DockedTo=TopDock,Session_LastDock=TopDock,Session_DockRow=6,Session_DockPos=0,Session_FloatLeft=39,Session_FloatTop=160,Session_FloatRightX=0,"
     "Preferences_Visible=1,Preferences_DockedTo=TopDock,Preferences_LastDock=TopDock,Preferences_DockRow=4,Preferences_DockPos=0,Preferences_FloatLeft=0,Preferences_FloatTop=0,Preferences_FloatRightX=0,"
     "Sort_Visible=0,Sort_DockedTo=TopDock,Sort_LastDock=TopDock,Sort_DockRow=5,Sort_DockPos=0,Sort_FloatLeft=0,Sort_FloatTop=0,Sort_FloatRightX=0,"
     "Address_Visible=1,Address_DockedTo=TopDock,Address_LastDock=TopDock,Address_DockRow=1,Address_DockPos=0,Address_FloatLeft=0,Address_FloatTop=0,Address_FloatRightX=0,"
     "Updates_Visible=1,Updates_DockedTo=TopDock,Updates_LastDock=TopDock,Updates_DockRow=4,Updates_DockPos=302,Updates_FloatLeft=0,Updates_FloatTop=0,Updates_FloatRightX=0,"
     "Transfer_Visible=1,Transfer_DockedTo=TopDock,Transfer_LastDock=TopDock,Transfer_DockRow=4,Transfer_DockPos=155,Transfer_FloatLeft=0,Transfer_FloatTop=0,Transfer_FloatRightX=0"
     "CustomCommands_Visible=0,CustomCommands_DockedTo=TopDock,CustomCommands_LastDock=TopDock,CustomCommands_DockRow=7,CustomCommands_DockPos=0,CustomCommands_FloatLeft=0,CustomCommands_FloatTop=0,CustomCommands_FloatRightX=0";
  FScpExplorer.SessionsTabs = true;
  FScpExplorer.StatusBar = true;
  UnicodeString PersonalFolder;
  ::SpecialFolderLocation(CSIDL_PERSONAL, PersonalFolder);
  FScpExplorer.LastLocalTargetDirectory = PersonalFolder;
  FScpExplorer.ViewStyle = 0; /* vsIcon */
  FScpExplorer.ShowFullAddress = true;
  FScpExplorer.DriveView = true;
  FScpExplorer.DriveViewWidth = 180;

  FScpCommander.WindowParams =
    BigResolution ? L"-1;-1;950;650;0" : L"-1;-1;600;400;0";
  FScpCommander.LocalPanelWidth = 0.5;
  FScpCommander.SwappedPanels = false;
  FScpCommander.SessionsTabs = true;
  FScpCommander.StatusBar = true;
  FScpCommander.NortonLikeMode = nlKeyboard;
  FScpCommander.PreserveLocalDirectory = false;
  // Toolbar_FloatRightX=1 makes keybar apper initialy "in column" when undocked
  FScpCommander.ToolbarsLayout =
    L"Queue_Visible=1,Queue_LastDock=QueueDock,Queue_DockRow=0,Queue_DockPos=-1,Queue_FloatLeft=0,Queue_FloatTop=0,Queue_FloatRightX=0,"
     "Session_Visible=0,Session_DockedTo=TopDock,Session_LastDock=TopDock,Session_DockRow=1,Session_DockPos=602,Session_FloatLeft=380,Session_FloatTop=197,Session_FloatRightX=0,"
     "Preferences_Visible=1,Preferences_DockedTo=TopDock,Preferences_LastDock=TopDock,Preferences_DockRow=1,Preferences_DockPos=0,Preferences_FloatLeft=0,Preferences_FloatTop=0,Preferences_FloatRightX=0,"
     "Selection_Visible=1,Selection_DockedTo=TopDock,Selection_LastDock=TopDock,Selection_DockRow=1,Selection_DockPos=257,Selection_FloatLeft=0,Selection_FloatTop=0,Selection_FloatRightX=0,"
     "Command_Visible=0,Command_DockedTo=TopDock,Command_LastDock=TopDock,Command_DockRow=2,Command_DockPos=0,Command_FloatLeft=0,Command_FloatTop=0,Command_FloatRightX=0,"
     "Sort_Visible=0,Sort_DockedTo=TopDock,Sort_LastDock=TopDock,Sort_DockRow=3,Sort_DockPos=0,Sort_FloatLeft=0,Sort_FloatTop=0,Sort_FloatRightX=0,"
     "Commands_Visible=1,Commands_DockedTo=TopDock,Commands_LastDock=TopDock,Commands_DockRow=1,Commands_DockPos=97,Commands_FloatLeft=0,Commands_FloatTop=0,Commands_FloatRightX=0,"
     "Menu_Visible=1,Menu_DockedTo=TopDock,Menu_LastDock=TopDock,Menu_DockRow=0,Menu_DockPos=0,Menu_FloatLeft=0,Menu_FloatTop=0,Menu_FloatRightX=0,"
     "Updates_Visible=1,Updates_DockedTo=TopDock,Updates_LastDock=TopDock,Updates_DockRow=1,Updates_DockPos=557,Updates_FloatLeft=0,Updates_FloatTop=0,Updates_FloatRightX=0,"
     "Transfer_Visible=1,Transfer_DockedTo=TopDock,Transfer_LastDock=TopDock,Transfer_DockRow=1,Transfer_DockPos=411,Transfer_FloatLeft=0,Transfer_FloatTop=0,Transfer_FloatRightX=0,"
     "UploadDownload_Visible=0,UploadDownload_DockedTo=TopDock,UploadDownload_LastDock=TopDock,UploadDownload_DockRow=4,UploadDownload_DockPos=0,UploadDownload_FloatLeft=0,UploadDownload_FloatTop=0,UploadDownload_FloatRightX=0,"
     "CustomCommands_Visible=0,CustomCommands_DockedTo=TopDock,CustomCommands_LastDock=TopDock,CustomCommands_DockRow=5,CustomCommands_DockPos=0,CustomCommands_FloatLeft=0,CustomCommands_FloatTop=0,CustomCommands_FloatRightX=0,"
     "RemotePath_Visible=1,RemotePath_DockedTo=RemoteTopDock,RemotePath_LastDock=RemoteTopDock,RemotePath_DockRow=0,RemotePath_DockPos=0,RemotePath_FloatLeft=0,RemotePath_FloatTop=0,RemotePath_FloatRightX=0,"
     "RemoteHistory_Visible=1,RemoteHistory_DockedTo=RemoteTopDock,RemoteHistory_LastDock=RemoteTopDock,RemoteHistory_DockRow=0,RemoteHistory_DockPos=208,RemoteHistory_FloatLeft=0,RemoteHistory_FloatTop=0,RemoteHistory_FloatRightX=0,"
     "RemoteNavigation_Visible=1,RemoteNavigation_DockedTo=RemoteTopDock,RemoteNavigation_LastDock=RemoteTopDock,RemoteNavigation_DockRow=0,RemoteNavigation_DockPos=288,RemoteNavigation_FloatLeft=0,RemoteNavigation_FloatTop=0,RemoteNavigation_FloatRightX=0,"
     "LocalPath_Visible=1,LocalPath_DockedTo=LocalTopDock,LocalPath_LastDock=LocalTopDock,LocalPath_DockRow=0,LocalPath_DockPos=0,LocalPath_FloatLeft=0,LocalPath_FloatTop=0,LocalPath_FloatRightX=0,"
     "LocalHistory_Visible=1,LocalHistory_DockedTo=LocalTopDock,LocalHistory_LastDock=LocalTopDock,LocalHistory_DockRow=0,LocalHistory_DockPos=207,LocalHistory_FloatLeft=0,LocalHistory_FloatTop=0,LocalHistory_FloatRightX=0,"
     "LocalNavigation_Visible=1,LocalNavigation_DockedTo=LocalTopDock,LocalNavigation_LastDock=LocalTopDock,LocalNavigation_DockRow=0,LocalNavigation_DockPos=287,LocalNavigation_FloatLeft=0,LocalNavigation_FloatTop=0,LocalNavigation_FloatRightX=0,"
     "Toolbar_Visible=1,Toolbar_DockedTo=BottomDock,Toolbar_LastDock=BottomDock,Toolbar_DockRow=1,Toolbar_DockPos=0,Toolbar_FloatLeft=0,Toolbar_FloatTop=0,Toolbar_FloatRightX=1,"
     "CommandLine_Visible=0,CommandLine_DockedTo=BottomDock,CommandLine_LastDock=BottomDock,CommandLine_DockRow=0,CommandLine_DockPos=0,CommandLine_FloatLeft=0,CommandLine_FloatTop=0,CommandLine_FloatRightX=0";
  FScpCommander.CurrentPanel = osLocal;
  FScpCommander.CompareByTime = true;
  FScpCommander.CompareBySize = false;
  FScpCommander.TreeOnLeft = false;
  FScpCommander.RemotePanel.DirViewParams = L"0;1;0|150,1;70,1;120,1;79,1;62,1;55,0;20,0;150,0;125,0|6;7;8;0;1;2;3;4;5";
  FScpCommander.RemotePanel.StatusBar = true;
  FScpCommander.RemotePanel.DriveView = false;
  FScpCommander.RemotePanel.DriveViewHeight = 100;
  FScpCommander.RemotePanel.DriveViewWidth = 100;
  FScpCommander.LocalPanel.DirViewParams = L"0;1;0|150,1;70,1;101,1;120,1;62,1;55,0|5;0;1;2;3;4";
  FScpCommander.LocalPanel.StatusBar = true;
  FScpCommander.LocalPanel.DriveView = false;
  FScpCommander.LocalPanel.DriveViewHeight = 100;
  FScpCommander.LocalPanel.DriveViewWidth = 100;

  FBookmarks->Clear();
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::DefaultLocalized()
{
  TGUIConfiguration::DefaultLocalized();

  if (FCustomCommandsDefaults)
  {
    FCustomCommandList->Clear();
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_EXECUTE), L"\"./!\"", 0);
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_TOUCH), L"touch \"!\"", ccApplyToDirectories | ccRecursive);
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_TAR),
      FORMAT(L"tar -cz  -f \"!?%s?archive.tgz!\" !&",
        (LoadStr(CUSTOM_COMMAND_TAR_ARCHIVE))), ccApplyToDirectories);
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_UNTAR),
      FORMAT(L"tar -xz --directory=\"!?%s?.!\" -f \"!\"",
        (LoadStr(CUSTOM_COMMAND_UNTAR_DIRECTORY))), 0);
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_GREP),
      FORMAT(L"grep \"!?%s?!\" !&", (LoadStr(CUSTOM_COMMAND_GREP_PATTERN))),
      ccShowResults);
    if (Win32Platform == VER_PLATFORM_WIN32_NT)
    {
      FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_FC),
        L"cmd /c fc \"!\" \"\!^!\" | more && pause", ccLocal);
    }
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_PRINT), L"notepad.exe /p \"!\"", ccLocal);
    FCustomCommandList->Reset();
    FCustomCommandsDefaults = true;
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinConfiguration::DetectRegistryStorage(HKEY RootKey)
{
  bool Result = false;
  TRegistryStorage * Storage = new TRegistryStorage(RegistryStorageKey, RootKey);
  try
  {
    if (Storage->OpenRootKey(false))
    {
      Result = true;
      Storage->CloseSubKey();
    }
  }
  __finally
  {
    delete Storage;
  }
  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall TWinConfiguration::CanWriteToStorage()
{
  bool Result = false;
  try
  {
    THierarchicalStorage * Storage = CreateScpStorage(false);
    try
    {
      Storage->AccessMode = smReadWrite;
      // This is actually not very good test, as we end up potentially with
      // the very same config, and TIniFileStorage file won't even try to
      // write the file then. Lucliky, we use this for empty config only,
      // so we end up with at least an empty section.
      if (Storage->OpenSubKey(ConfigurationSubKey, true))
      {
        Storage->WriteBool(L"WriteTest", true);
        Storage->DeleteValue(L"WriteTest");
      }
      Storage->Flush();
    }
    __finally
    {
      delete Storage;
    }
    Result = true;
  }
  catch(...)
  {
  }
  return Result;
}
//---------------------------------------------------------------------------
TStorage __fastcall TWinConfiguration::GetStorage()
{
  if (FStorage == stDetect)
  {
    if (FindResourceEx(NULL, RT_RCDATA, L"WINSCP_SESSION",
      MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)))
    {
      FTemporarySessionFile =
        IncludeTrailingBackslash(SystemTemporaryDirectory()) + L"winscps.tmp";
      DumpResourceToFile(L"WINSCP_SESSION", FTemporarySessionFile);
      FEmbeddedSessions = true;
      FTemporaryKeyFile =
        IncludeTrailingBackslash(SystemTemporaryDirectory()) + L"winscpk.tmp";
      if (!DumpResourceToFile(L"WINSCP_KEY", FTemporaryKeyFile))
      {
        FTemporaryKeyFile = L"";
      }
    }

    FStorage = stIniFile;
    if (!FileExists(IniFileStorageName))
    {
      if (DetectRegistryStorage(HKEY_CURRENT_USER) ||
          DetectRegistryStorage(HKEY_LOCAL_MACHINE) ||
          !CanWriteToStorage())
      {
        FStorage = stRegistry;
      }
    }
  }
  return TCustomWinConfiguration::GetStorage();
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::Saved()
{
  TCustomWinConfiguration::Saved();
  FBookmarks->ModifyAll(false);
  FCustomCommandList->Reset();
  FEditorList->Saved();
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::RecryptPasswords()
{
  TCustomWinConfiguration::RecryptPasswords();
  TTerminalManager * Manager = TTerminalManager::Instance(false);
  assert(Manager != NULL);
  if (Manager != NULL)
  {
    Manager->RecryptPasswords();
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinConfiguration::GetUseMasterPassword()
{
  return FUseMasterPassword;
}
//---------------------------------------------------------------------------
THierarchicalStorage * TWinConfiguration::CreateScpStorage(bool SessionList)
{
  if (SessionList && !FTemporarySessionFile.IsEmpty())
  {
    return new TIniFileStorage(FTemporarySessionFile);
  }
  else
  {
    return TCustomWinConfiguration::CreateScpStorage(SessionList);
  }
}
//---------------------------------------------------------------------------
// duplicated from core\configuration.cpp
#define LASTELEM(ELEM) \
  ELEM.SubString(ELEM.LastDelimiter(L".>")+1, ELEM.Length() - ELEM.LastDelimiter(L".>"))
#define BLOCK(KEY, CANCREATE, BLOCK) \
  if (Storage->OpenSubKey(KEY, CANCREATE, true)) try { BLOCK } __finally { Storage->CloseSubKey(); }
#define KEY(TYPE, VAR) KEYEX(TYPE, VAR, VAR)
#define REGCONFIG(CANCREATE) \
  BLOCK(L"Interface", CANCREATE, \
    KEYEX(Integer,DoubleClickAction, CopyOnDoubleClick); \
    KEY(Bool,     CopyOnDoubleClickConfirmation); \
    KEY(Bool,     DDAllowMove); \
    KEY(Bool,     DDAllowMoveInit); \
    KEY(Bool,     DDTransferConfirmation); \
    KEY(String,   DDTemporaryDirectory); \
    KEY(Bool,     DDWarnLackOfTempSpace); \
    KEY(Float,    DDWarnLackOfTempSpaceRatio); \
    KEY(Bool,     DeleteToRecycleBin); \
    KEY(Bool,     DimmHiddenFiles); \
    KEY(Bool,     RenameWholeName); \
    KEY(Bool,     SelectDirectories); \
    KEY(String,   SelectMask); \
    KEY(Bool,     ShowHiddenFiles); \
    KEY(Bool,     FormatSizeBytes); \
    KEY(Bool,     ShowInaccesibleDirectories); \
    KEY(Bool,     ConfirmTransferring); \
    KEY(Bool,     ConfirmDeleting); \
    KEY(Bool,     ConfirmRecycling); \
    KEY(Bool,     ConfirmClosingSession); \
    KEY(String,   AutoStartSession); \
    KEY(Bool,     UseLocationProfiles); \
    KEY(Bool,     UseSharedBookmarks); \
    KEY(Integer,  LocaleSafe); \
    KEY(Bool,     DDExtEnabled); \
    KEY(Integer,  DDExtTimeout); \
    KEY(Bool,     DefaultDirIsHome); \
    KEY(Bool,     TemporaryDirectoryAppendSession); \
    KEY(Bool,     TemporaryDirectoryAppendPath); \
    KEY(Bool,     TemporaryDirectoryCleanup); \
    KEY(Bool,     ConfirmTemporaryDirectoryCleanup); \
    KEY(Bool,     PreservePanelState); \
    KEY(String,   Theme); \
    KEY(String,   LastStoredSession); \
    KEY(Integer,  PathInCaption); \
    KEY(Bool,     MinimizeToTray); \
    KEY(Bool,     BalloonNotifications); \
    KEY(Integer,  NotificationsTimeout); \
    KEY(Integer,  NotificationsStickTime); \
    KEY(Bool,     CopyParamAutoSelectNotice); \
    KEY(Bool,     LockToolbars); \
    KEY(Bool,     AutoOpenInPutty); \
    KEY(Bool,     RefreshRemotePanel); \
    KEY(DateTime, RefreshRemotePanelInterval); \
    KEY(Bool,     FullRowSelect); \
    KEY(Integer,  LastMonitor); \
    KEY(String,   VersionHistory); \
    KEY(Bool,     EnableQueueByDefault); \
    KEY(String,   OpenedStoredSessionFolders); \
  ); \
  BLOCK(L"Interface\\Editor", CANCREATE, \
    KEY(String,   Editor.FontName); \
    KEY(Integer,  Editor.FontHeight); \
    KEY(Integer,  Editor.FontStyle); \
    KEY(Integer,  Editor.FontCharset); \
    KEY(Bool,     Editor.WordWrap); \
    KEY(String,   Editor.FindText); \
    KEY(String,   Editor.ReplaceText); \
    KEY(Bool,     Editor.FindMatchCase); \
    KEY(Bool,     Editor.FindWholeWord); \
    KEY(Bool,     Editor.FindDown); \
    KEY(Integer,  Editor.TabSize); \
    KEY(Integer,  Editor.MaxEditors); \
    KEY(Integer,  Editor.EarlyClose); \
    KEY(Bool,     Editor.SDIShellEditor); \
    KEY(String,   Editor.WindowParams); \
    KEY(Integer,  Editor.Encoding); \
  ); \
  BLOCK(L"Interface\\QueueView", CANCREATE, \
    KEY(Integer,  QueueView.Height); \
    KEY(String,   QueueView.Layout); \
    KEY(Integer,  QueueView.Show); \
    KEY(Integer,  QueueView.LastHideShow); \
    KEY(Bool,     QueueView.ToolBar); \
  ); \
  BLOCK(L"Interface\\Updates", CANCREATE, \
    KEY(Integer,  FUpdates.Period); \
    KEY(DateTime, FUpdates.LastCheck); \
    KEY(Integer,  FUpdates.HaveResults); \
    KEY(Integer,  FUpdates.ShownResults); \
    KEY(Integer,  FUpdates.BetaVersions); \
    KEY(Integer,  FUpdates.ConnectionType); \
    KEY(String,   FUpdates.ProxyHost); \
    KEY(Integer,  FUpdates.ProxyPort); \
    KEY(Integer,  FUpdates.Results.ForVersion); \
    KEY(Integer,  FUpdates.Results.Version); \
    KEY(String,   FUpdates.Results.Message); \
    KEY(Integer,  FUpdates.Results.Critical); \
    KEY(String,   FUpdates.Results.Release); \
    KEY(Bool,     FUpdates.Results.Disabled); \
    KEY(String,   FUpdates.Results.Url); \
    KEY(String,   FUpdates.Results.UrlButton); \
    KEY(String,   FUpdates.DotNetVersion); \
    KEY(String,   FUpdates.ConsoleVersion); \
  ); \
  BLOCK(L"Interface\\Explorer", CANCREATE, \
    KEY(String,  ScpExplorer.ToolbarsLayout); \
    KEY(String,  ScpExplorer.DirViewParams); \
    KEY(String,  ScpExplorer.LastLocalTargetDirectory); \
    KEY(Bool,    ScpExplorer.SessionsTabs); \
    KEY(Bool,    ScpExplorer.StatusBar); \
    KEY(String,  ScpExplorer.WindowParams); \
    KEY(Integer, ScpExplorer.ViewStyle); \
    KEY(Bool,    ScpExplorer.ShowFullAddress); \
    KEY(Bool,    ScpExplorer.DriveView); \
    KEY(Integer, ScpExplorer.DriveViewWidth); \
  ); \
  BLOCK(L"Interface\\Commander", CANCREATE, \
    KEY(String,  ScpCommander.ToolbarsLayout); \
    KEY(Integer, ScpCommander.CurrentPanel); \
    KEY(Float,   ScpCommander.LocalPanelWidth); \
    KEY(Bool,    ScpCommander.SwappedPanels); \
    KEY(Bool,    ScpCommander.SessionsTabs); \
    KEY(Bool,    ScpCommander.StatusBar); \
    KEY(String,  ScpCommander.WindowParams); \
    KEYEX(Integer, ScpCommander.NortonLikeMode, ExplorerStyleSelection); \
    KEY(Bool,    ScpCommander.PreserveLocalDirectory); \
    KEY(Bool,    ScpCommander.CompareByTime); \
    KEY(Bool,    ScpCommander.CompareBySize); \
    KEY(Bool,    ScpCommander.TreeOnLeft); \
  ); \
  BLOCK(L"Interface\\Commander\\LocalPanel", CANCREATE, \
    KEY(String,  ScpCommander.LocalPanel.DirViewParams); \
    KEY(Bool,    ScpCommander.LocalPanel.StatusBar); \
    KEY(Bool,    ScpCommander.LocalPanel.DriveView); \
    KEY(Integer, ScpCommander.LocalPanel.DriveViewHeight); \
    KEY(Integer, ScpCommander.LocalPanel.DriveViewWidth); \
  ); \
  BLOCK(L"Interface\\Commander\\RemotePanel", CANCREATE, \
    KEY(String,  ScpCommander.RemotePanel.DirViewParams); \
    KEY(Bool,    ScpCommander.RemotePanel.StatusBar); \
    KEY(Bool,    ScpCommander.RemotePanel.DriveView); \
    KEY(Integer, ScpCommander.RemotePanel.DriveViewHeight); \
    KEY(Integer, ScpCommander.RemotePanel.DriveViewWidth); \
  ); \
  BLOCK(L"Logging", CANCREATE, \
    KEY(Bool,    LogWindowOnStartup); \
    KEY(String,  LogWindowParams); \
  ); \
  BLOCK(L"Security", CANCREATE, \
    KEYEX(Bool,  FUseMasterPassword, UseMasterPassword); \
    KEYEX(String,FMasterPasswordVerifier, MasterPasswordVerifier); \
  );
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SaveData(THierarchicalStorage * Storage, bool All)
{
  TCustomWinConfiguration::SaveData(Storage, All);

  // duplicated from core\configuration.cpp
  #define KEYEX(TYPE, VAR, NAME) Storage->Write ## TYPE(LASTELEM(UnicodeString(TEXT(#NAME))), VAR)
  REGCONFIG(true);
  #undef KEYEX

  if (Storage->OpenSubKey(L"Bookmarks", true))
  {
    FBookmarks->Save(Storage, All);

    Storage->CloseSubKey();
  }
  if ((All && !FCustomCommandsDefaults) || FCustomCommandList->Modified)
  {
    FCustomCommandList->Save(Storage);
  }

  if ((All || FEditorList->Modified) &&
      Storage->OpenSubKey(L"Interface\\Editor", true, true))
  try
  {
    FEditorList->Save(Storage);
  }
  __finally
  {
    Storage->CloseSubKey();
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::Load()
{
  FLegacyEditor = new TEditorPreferences();
  try
  {
    FLegacyEditor->LegacyDefaults();

    TCustomWinConfiguration::Load();

    int EditorCount = FEditorList->Count;
    if (EditorCount == 0)
    {
      TEditorPreferences * AlternativeEditor = NULL;
      try
      {
        if (FLegacyEditor->Data->Editor == edInternal)
        {
          if (!FLegacyEditor->Data->ExternalEditor.IsEmpty())
          {
            AlternativeEditor = new TEditorPreferences(*FLegacyEditor);
            AlternativeEditor->GetData()->Editor = edExternal;
            FLegacyEditor->GetData()->ExternalEditor = L"";
          }
        }
        else
        {
          if (FLegacyEditor->Data->ExternalEditor.IsEmpty())
          {
            FLegacyEditor->GetData()->Editor = edInternal;
          }
          else
          {
            AlternativeEditor = new TEditorPreferences(*FLegacyEditor);
            AlternativeEditor->GetData()->Editor = edInternal;
          }
        }
      }
      catch(...)
      {
        delete AlternativeEditor;
        throw;
      }

      FEditorList->Add(FLegacyEditor);
      FLegacyEditor = NULL;
      if (AlternativeEditor != NULL)
      {
        FEditorList->Add(AlternativeEditor);
      }
    }
  }
  __finally
  {
    delete FLegacyEditor;
    FLegacyEditor = NULL;
  }

  if (FUpdates.ConnectionType == -1)
  {
    FUpdates.ConnectionType = (FUpdates.ProxyHost.IsEmpty() ? ctAuto : ctProxy);
  }
  AddVersionToHistory();
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::LoadData(THierarchicalStorage * Storage)
{
  TCustomWinConfiguration::LoadData(Storage);

  // duplicated from core\configuration.cpp
  #define KEYEX(TYPE, VAR, NAME) VAR = Storage->Read ## TYPE(LASTELEM(UnicodeString(TEXT(#NAME))), VAR)
  #pragma warn -eas
  REGCONFIG(false);
  #pragma warn +eas
  #undef KEYEX

  if (Storage->OpenSubKey(L"Bookmarks", false))
  {
    FBookmarks->Load(Storage);
    Storage->CloseSubKey();
  }

  if (Storage->HasSubKey("CustomCommands"))
  {
    FCustomCommandList->Load(Storage);
    FCustomCommandsDefaults = false;
  }
  else if (FCustomCommandList->Modified)
  {
    // can this (=reloading of configuration) even happen?
    // if it does, shouldn't we reset default commands?
    assert(false);
    FCustomCommandList->Clear();
    FCustomCommandsDefaults = false;
  }
  FCustomCommandList->Reset();

  if (Storage->OpenSubKey(L"Interface\\Editor", false, true))
  try
  {
    FEditorList->Clear();
    FEditorList->Load(Storage);
  }
  __finally
  {
    Storage->CloseSubKey();
  }

  // load legacy editor configuration
  assert(FLegacyEditor != NULL);
  if (Storage->OpenSubKey(L"Interface\\Editor", false, true))
  {
    try
    {
      FLegacyEditor->Load(Storage, true);
    }
    __finally
    {
      Storage->CloseSubKey();
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::LoadAdmin(THierarchicalStorage * Storage)
{
  TCustomWinConfiguration::LoadAdmin(Storage);
  FDisableOpenEdit = Storage->ReadBool(L"DisableOpenEdit", FDisableOpenEdit);
  FDefaultUpdatesPeriod = Storage->ReadInteger(L"DefaultUpdatesPeriod", FDefaultUpdatesPeriod);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::CopyData(THierarchicalStorage * Source, THierarchicalStorage * Target)
{
  TCustomWinConfiguration::CopyData(Source, Target);

  if (Source->OpenSubKey(ConfigurationSubKey, false))
  {
    if (Target->OpenSubKey(ConfigurationSubKey, true))
    {
      Target->WriteString(L"JumpList", Source->ReadString(L"JumpList", L""));
      Target->CloseSubKey();
    }
    Source->CloseSubKey();
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::ClearTemporaryLoginData()
{
  if (!FTemporaryKeyFile.IsEmpty())
  {
    DeleteFile(FTemporaryKeyFile);
    FTemporaryKeyFile = "";
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::AddVersionToHistory()
{
  int CurrentVersion = CompoundVersion;

  int From = 1;
  bool CurrentVersionPresent = false;
  while (!CurrentVersionPresent && (From < FVersionHistory.Length()))
  {
    UnicodeString VersionInfo = CopyToChars(FVersionHistory, From, L";", true);
    UnicodeString VersionStr = ::CutToChar(VersionInfo, L',', true);
    int Version;

    if (TryStrToInt(VersionStr, Version) &&
        (Version == CurrentVersion))
    {
      CurrentVersionPresent = true;
    }
  }

  if (!CurrentVersionPresent)
  {
    UnicodeString CurrentVersionInfo =
      IntToStr(CurrentVersion) + L"," + FileInfoString[L"ReleaseType"];
    AddToList(FVersionHistory, CurrentVersionInfo, L';');
  }

  Usage->Set(L"AnyBetaUsed", AnyBetaInVersionHistory);
}
//---------------------------------------------------------------------------
bool __fastcall TWinConfiguration::DoIsBeta(const UnicodeString & ReleaseType)
{
  return AnsiSameText(ReleaseType, L"beta") || AnsiSameText(ReleaseType, L"rc");
}
//---------------------------------------------------------------------------
bool __fastcall TWinConfiguration::GetIsBeta()
{
  return DoIsBeta(FileInfoString[L"ReleaseType"]);
}
//---------------------------------------------------------------------------
bool __fastcall TWinConfiguration::GetAnyBetaInVersionHistory()
{
  int From = 1;
  bool AnyBeta = false;
  while (!AnyBeta && (From < VersionHistory.Length()))
  {
    UnicodeString VersionInfo = CopyToChars(VersionHistory, From, L";", true);
    ::CutToChar(VersionInfo, L',', true);
    UnicodeString ReleaseType = ::CutToChar(VersionInfo, ',', true);

    if (DoIsBeta(ReleaseType))
    {
      AnyBeta = true;
    }
  }
  return AnyBeta;
}
//---------------------------------------------------------------------------
bool __fastcall TWinConfiguration::GetDDExtInstalled()
{
  if (FDDExtInstalled < 0)
  {
    if (IsWin64())
    {
      // temporarily consider dragext always present on 64-bit system
      FDDExtInstalled = 1;
    }
    else
    {
      void * DragExtRef;
      int CreateResult =
        CoCreateInstance(CLSID_ShellExtension, NULL,
          CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_IUnknown,
          &DragExtRef);
      bool Result = (CreateResult == S_OK);
      FDDExtInstalled = (Result ? 1 : 0);
      if (Result)
      {
        reinterpret_cast<IUnknown *>(DragExtRef)->Release();
        CoFreeUnusedLibraries();
      }
    }
  }
  return (FDDExtInstalled > 0);
}
//---------------------------------------------------------------------------
RawByteString __fastcall TWinConfiguration::StronglyRecryptPassword(RawByteString Password, UnicodeString Key)
{
  RawByteString Dummy;
  RawByteString Result;
  if (GetExternalEncryptedPassword(Password, Dummy) ||
      !FUseMasterPassword)
  {
    // already-strongly encrypted
    // or no master password set, so we cannot strongly-encrypt it
    Result = Password;
  }
  else
  {
    UnicodeString PasswordText =
      TCustomWinConfiguration::DecryptPassword(Password, Key);
    if (!PasswordText.IsEmpty())
    {
      assert(!FPlainMasterPasswordEncrypt.IsEmpty());
      Password = ScramblePassword(PasswordText);
      AES256EncyptWithMAC(Password, FPlainMasterPasswordEncrypt, Result);
      Result = SetExternalEncryptedPassword(Result);
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TWinConfiguration::DecryptPassword(RawByteString Password, UnicodeString Key)
{
  UnicodeString Result;
  RawByteString Buf;
  if (GetExternalEncryptedPassword(Password, Buf))
  {
    if (FDontDecryptPasswords == 0)
    {
      if (FPlainMasterPasswordDecrypt.IsEmpty())
      {
        assert(FOnMasterPasswordPrompt != NULL);
        FOnMasterPasswordPrompt();
      }
      if (!AES256DecryptWithMAC(Buf, FPlainMasterPasswordDecrypt, Buf) ||
          !UnscramblePassword(Buf, Result))
      {
        throw Exception(LoadStr(DECRYPT_PASSWORD_ERROR));
      }
    }
    else
    {
      Abort();
    }
  }
  else
  {
    Result = TCustomWinConfiguration::DecryptPassword(Password, Key);
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetMasterPassword(UnicodeString value)
{
  if (FUseMasterPassword && ValidateMasterPassword(value))
  {
    // just store the plain-text version of the password
    FPlainMasterPasswordEncrypt = value;
    FPlainMasterPasswordDecrypt = value;
  }
  else
  {
    RawByteString Verifier;
    AES256CreateVerifier(value, Verifier);
    FMasterPasswordVerifier = BytesToHex(Verifier);
    FPlainMasterPasswordEncrypt = value;
    FUseMasterPassword = true;
    try
    {
      RecryptPasswords();
    }
    __finally
    {
      FPlainMasterPasswordDecrypt = value;
    }
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinConfiguration::ValidateMasterPassword(UnicodeString value)
{
  assert(UseMasterPassword);
  assert(!FMasterPasswordVerifier.IsEmpty());
  return AES256Verify(value, HexToBytes(FMasterPasswordVerifier));
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::ClearMasterPassword()
{
  FMasterPasswordVerifier = L"";
  FUseMasterPassword = false;
  Shred(FPlainMasterPasswordEncrypt);
  try
  {
    RecryptPasswords();
  }
  __finally
  {
    Shred(FPlainMasterPasswordDecrypt);
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::AskForMasterPasswordIfNotSet()
{
  if (FPlainMasterPasswordEncrypt.IsEmpty())
  {
    assert(FOnMasterPasswordPrompt != NULL);
    FOnMasterPasswordPrompt();
    assert(!FPlainMasterPasswordDecrypt.IsEmpty());
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetLogWindowOnStartup(bool value)
{
  SET_CONFIG_PROPERTY(LogWindowOnStartup);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetLogWindowParams(UnicodeString value)
{
  SET_CONFIG_PROPERTY(LogWindowParams);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDDAllowMove(bool value)
{
  SET_CONFIG_PROPERTY(DDAllowMove);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDDAllowMoveInit(bool value)
{
  SET_CONFIG_PROPERTY(DDAllowMoveInit);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDDTransferConfirmation(bool value)
{
  SET_CONFIG_PROPERTY(DDTransferConfirmation);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDDTemporaryDirectory(UnicodeString value)
{
  SET_CONFIG_PROPERTY(DDTemporaryDirectory);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDDExtEnabled(bool value)
{
  SET_CONFIG_PROPERTY(DDExtEnabled);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDDExtTimeout(int value)
{
  SET_CONFIG_PROPERTY(DDExtTimeout);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDDWarnLackOfTempSpace(bool value)
{
  SET_CONFIG_PROPERTY(DDWarnLackOfTempSpace);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDDWarnLackOfTempSpaceRatio(double value)
{
  SET_CONFIG_PROPERTY(DDWarnLackOfTempSpaceRatio);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetScpExplorer(TScpExplorerConfiguration value)
{
  SET_CONFIG_PROPERTY(ScpExplorer);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetScpCommander(TScpCommanderConfiguration value)
{
  SET_CONFIG_PROPERTY(ScpCommander);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetEditor(TEditorConfiguration value)
{
  SET_CONFIG_PROPERTY(Editor);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetQueueView(TQueueViewConfiguration value)
{
  SET_CONFIG_PROPERTY(QueueView);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetEnableQueueByDefault(bool value)
{
  SET_CONFIG_PROPERTY(EnableQueueByDefault);
}
//---------------------------------------------------------------------------
TUpdatesConfiguration __fastcall TWinConfiguration::GetUpdates()
{
  TUpdatesConfiguration Result;
  {
    TGuard Guard(FCriticalSection);
    Result = FUpdates;
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetUpdates(TUpdatesConfiguration value)
{
  TGuard Guard(FCriticalSection);
  // do not use SET_CONFIG_PROPERTY to avoid OnChange handler call (not synchronized)
  FUpdates = value;
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetVersionHistory(UnicodeString value)
{
  SET_CONFIG_PROPERTY(VersionHistory);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDeleteToRecycleBin(bool value)
{
  SET_CONFIG_PROPERTY(DeleteToRecycleBin);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetSelectDirectories(bool value)
{
  SET_CONFIG_PROPERTY(SelectDirectories);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetShowHiddenFiles(bool value)
{
  SET_CONFIG_PROPERTY(ShowHiddenFiles);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetFormatSizeBytes(bool value)
{
  SET_CONFIG_PROPERTY(FormatSizeBytes);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetShowInaccesibleDirectories(bool value)
{
  SET_CONFIG_PROPERTY(ShowInaccesibleDirectories);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetConfirmTransferring(bool value)
{
  SET_CONFIG_PROPERTY(ConfirmTransferring);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetConfirmDeleting(bool value)
{
  SET_CONFIG_PROPERTY(ConfirmDeleting);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetConfirmRecycling(bool value)
{
  SET_CONFIG_PROPERTY(ConfirmRecycling);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetUseLocationProfiles(bool value)
{
  SET_CONFIG_PROPERTY(UseLocationProfiles);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetUseSharedBookmarks(bool value)
{
  SET_CONFIG_PROPERTY(UseSharedBookmarks);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetConfirmClosingSession(bool value)
{
  SET_CONFIG_PROPERTY(ConfirmClosingSession);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDoubleClickAction(TDoubleClickAction value)
{
  SET_CONFIG_PROPERTY(DoubleClickAction);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetCopyOnDoubleClickConfirmation(bool value)
{
  SET_CONFIG_PROPERTY(CopyOnDoubleClickConfirmation);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDimmHiddenFiles(bool value)
{
  SET_CONFIG_PROPERTY(DimmHiddenFiles);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetRenameWholeName(bool value)
{
  SET_CONFIG_PROPERTY(RenameWholeName);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetAutoStartSession(UnicodeString value)
{
  SET_CONFIG_PROPERTY(AutoStartSession);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetExpertMode(bool value)
{
  SET_CONFIG_PROPERTY(ExpertMode);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetDefaultDirIsHome(bool value)
{
  SET_CONFIG_PROPERTY(DefaultDirIsHome);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetTemporaryDirectoryAppendSession(bool value)
{
  SET_CONFIG_PROPERTY(TemporaryDirectoryAppendSession);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetTemporaryDirectoryAppendPath(bool value)
{
  SET_CONFIG_PROPERTY(TemporaryDirectoryAppendPath);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetTemporaryDirectoryCleanup(bool value)
{
  SET_CONFIG_PROPERTY(TemporaryDirectoryCleanup);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetConfirmTemporaryDirectoryCleanup(bool value)
{
  SET_CONFIG_PROPERTY(ConfirmTemporaryDirectoryCleanup);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetPreservePanelState(bool value)
{
  SET_CONFIG_PROPERTY(PreservePanelState);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetTheme(UnicodeString value)
{
  SET_CONFIG_PROPERTY_EX(Theme, ConfigureInterface());
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetLastStoredSession(UnicodeString value)
{
  SET_CONFIG_PROPERTY(LastStoredSession);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetPathInCaption(TPathInCaption value)
{
  SET_CONFIG_PROPERTY(PathInCaption);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetMinimizeToTray(bool value)
{
  SET_CONFIG_PROPERTY(MinimizeToTray);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetBalloonNotifications(bool value)
{
  SET_CONFIG_PROPERTY(BalloonNotifications);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetNotificationsTimeout(unsigned int value)
{
  SET_CONFIG_PROPERTY(NotificationsTimeout);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetNotificationsStickTime(unsigned int value)
{
  SET_CONFIG_PROPERTY(NotificationsStickTime);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetCopyParamAutoSelectNotice(bool value)
{
  SET_CONFIG_PROPERTY(CopyParamAutoSelectNotice);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetLockToolbars(bool value)
{
  SET_CONFIG_PROPERTY(LockToolbars);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetAutoOpenInPutty(bool value)
{
  SET_CONFIG_PROPERTY(AutoOpenInPutty);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetRefreshRemotePanel(bool value)
{
  SET_CONFIG_PROPERTY(RefreshRemotePanel);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetRefreshRemotePanelInterval(TDateTime value)
{
  SET_CONFIG_PROPERTY(RefreshRemotePanelInterval);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetFullRowSelect(bool value)
{
  SET_CONFIG_PROPERTY(FullRowSelect);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetOpenedStoredSessionFolders(UnicodeString value)
{
  SET_CONFIG_PROPERTY(OpenedStoredSessionFolders);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetCustomCommandList(TCustomCommandList * value)
{
  assert(FCustomCommandList);
  if (!FCustomCommandList->Equals(value))
  {
    FCustomCommandList->Assign(value);
    FCustomCommandsDefaults = false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetBookmarks(UnicodeString Key,
  TBookmarkList * value)
{
  FBookmarks->Bookmarks[Key] = value;
  Changed();
}
//---------------------------------------------------------------------------
TBookmarkList * __fastcall TWinConfiguration::GetBookmarks(UnicodeString Key)
{
  return FBookmarks->Bookmarks[Key];
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetSharedBookmarks(TBookmarkList * value)
{
  FBookmarks->SharedBookmarks = value;
  Changed();
}
//---------------------------------------------------------------------------
TBookmarkList * __fastcall TWinConfiguration::GetSharedBookmarks()
{
  return FBookmarks->SharedBookmarks;
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TWinConfiguration::GetDefaultKeyFile()
{
  return (!FDefaultKeyFile.IsEmpty() ? FDefaultKeyFile : FTemporaryKeyFile);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetLastMonitor(int value)
{
  ::SetLastMonitor(value);
}
//---------------------------------------------------------------------------
int __fastcall TWinConfiguration::GetLastMonitor()
{
  return ::GetLastMonitor();
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TWinConfiguration::TemporaryDir(bool Mask)
{
  return UniqTempDir(ExpandFileName(ExpandEnvironmentVariables(DDTemporaryDirectory)),
    L"scp", Mask);
}
//---------------------------------------------------------------------------
TStrings * __fastcall TWinConfiguration::FindTemporaryFolders()
{
  TStrings * Result = new TStringList();
  try
  {
    TSearchRec SRec;
    UnicodeString Mask = TemporaryDir(true);
    UnicodeString Directory = ExtractFilePath(Mask);
    if (FindFirst(Mask, faDirectory, SRec) == 0)
    {
      do
      {
        if (FLAGSET(SRec.Attr, faDirectory))
        {
          Result->Add(Directory + SRec.Name);
        }
      }
      while (FindNext(SRec) == 0);
    }

    if (Result->Count == 0)
    {
      delete Result;
      Result = NULL;
    }
  }
  catch(...)
  {
    delete Result;
    throw;
  }

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::CleanupTemporaryFolders(TStrings * Folders)
{
  UnicodeString ErrorList;
  TStrings * F;
  if (Folders == NULL)
  {
    F = FindTemporaryFolders();
  }
  else
  {
    F = Folders;
  }

  if (F != NULL)
  {
    try
    {
      for (int i = 0; i < F->Count; i++)
      {
        if (!DeleteDirectory(F->Strings[i]))
        {
          if (!ErrorList.IsEmpty())
          {
            ErrorList += L"\n";
          }
          ErrorList += F->Strings[i];
        }
      }
    }
    __finally
    {
      if (Folders == NULL)
      {
        delete F;
      }
    }

    if (!ErrorList.IsEmpty())
    {
      throw ExtException(LoadStr(CLEANUP_TEMP_ERROR), ErrorList);
    }
  }
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
#pragma warn -inl
//---------------------------------------------------------------------------
class TAsInheritedReader : public TReader
{
public:
  __fastcall TAsInheritedReader(TStream * Stream, int BufSize) :
    TReader(Stream, BufSize)
  {
    OnAncestorNotFound = AncestorNotFound;
  }

  virtual void __fastcall ReadPrefix(TFilerFlags & Flags, int & AChildPos)
  {
    TReader::ReadPrefix(Flags, AChildPos);
    Flags << ffInherited;
  }

  void __fastcall AncestorNotFound(TReader * Reader,
    const UnicodeString ComponentName, TMetaClass * ComponentClass,
    TComponent *& Component)
  {
    assert(!Component);
    if (ComponentName.IsEmpty())
    {
      for (int Index = 0; Index < LookupRoot->ComponentCount; Index++)
      {
        Component = LookupRoot->Components[Index];
        if (Component->Name.IsEmpty())
        {
          return;
        }
      }
      Component = NULL;
    }
  }
};
//---------------------------------------------------------------------------
#pragma warn .inl
//---------------------------------------------------------------------------
bool __fastcall TWinConfiguration::InternalReloadComponentRes(const UnicodeString ResName,
  HINSTANCE HInst, TComponent * Instance)
{
  HANDLE HRsrc;
  bool Result;

  if (!HInst)
  {
    HInst = HInstance;
  }
  HRsrc = FindResource(HInst, ResName.c_str(), RT_RCDATA);
  Result = (HRsrc != 0);
  if (Result)
  {
    TResourceStream * ResStream = new TResourceStream(
      reinterpret_cast<int>(HInst), ResName, RT_RCDATA);
    try
    {
      TReader * Reader;
      Reader = new TAsInheritedReader(ResStream, 4096);

      try
      {
        /*Instance =*/ Reader->ReadRootComponent(Instance);
      }
      __finally
      {
        delete Reader;
      }
    }
    __finally
    {
      delete ResStream;
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall TWinConfiguration::InitComponent(TComponent * Instance,
  TClass RootAncestor, TClass ClassType)
{
  bool Result = false;
  if ((ClassType != __classid(TComponent)) && (ClassType != RootAncestor))
  {
    if (InitComponent(Instance, RootAncestor, ClassType->ClassParent()))
    {
      Result = true;
    }
    if (InternalReloadComponentRes(ClassType->ClassName(),
          reinterpret_cast<HINSTANCE>(FindResourceHInstance(FindClassHInstance(ClassType))),
          Instance))
    {
      Result = true;
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
LCID __fastcall TWinConfiguration::GetLocale()
{
  if (!FLocale)
  {
    UnicodeString ResourceModule =
      GetResourceModuleName(Application->Name, ModuleFileName().c_str());
    if (!ResourceModule.IsEmpty())
    {
      UnicodeString ResourceExt = ExtractFileExt(ResourceModule).UpperCase();
      ResourceExt.Delete(1, 1);

      TLanguages * Langs = LanguagesDEPF();
      int Index, Count;

      Count = Langs->Count;
      Index = 0;
      while ((Index < Count) && !FLocale)
      {
        if (Langs->Ext[Index] == ResourceExt)
        {
          FLocale = Langs->LocaleID[Index];
        }
        else if (Langs->Ext[Index].SubString(1, 2) == ResourceExt)
        {
          FLocale = MAKELANGID(PRIMARYLANGID(Langs->LocaleID[Index]),
            SUBLANG_DEFAULT);
        }
        Index++;
      }
    }
  }

  return TCustomWinConfiguration::GetLocale();
}
//---------------------------------------------------------------------------
HINSTANCE __fastcall TWinConfiguration::LoadNewResourceModule(LCID ALocale,
  UnicodeString * FileName)
{
  UnicodeString FileNameStorage;
  if (FileName == NULL)
  {
    FileName = &FileNameStorage;
  }

  HINSTANCE Instance = TCustomWinConfiguration::LoadNewResourceModule(ALocale, FileName);
  if (Instance != NULL)
  {
    try
    {
      CheckTranslationVersion(*FileName, false);
    }
    catch(...)
    {
      FreeResourceModule(Instance);
      throw;
    }
  }
  return Instance;
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetResourceModule(HINSTANCE Instance)
{
  TCustomWinConfiguration::SetResourceModule(Instance);

  Busy(true);
  try
  {
    int Count;
    UnicodeString OrigName;
    int OrigLeft;
    int OrigTop;

    TForm * Form;
    Count = Screen->FormCount;

    for (int Index = 0; Index < Count; Index++)
    {
      Form = Screen->Forms[Index];
      SendMessage(Form->Handle, WM_LOCALE_CHANGE, 0, 1);
    }

    ConfigureInterface();

    for (int Index = 0; Index < Count; Index++)
    {
      Form = Screen->Forms[Index];
      TComponent * Component;
      for (int Index = 0; Index < Form->ComponentCount; Index++)
      {
        Component = Form->Components[Index];
        if (dynamic_cast<TFrame*>(Component))
        {
          OrigName = Component->Name;
          InitComponent(Component, __classid(TFrame), Component->ClassType());
          Component->Name = OrigName;
        }
      }

      OrigLeft = Form->Left;
      OrigTop = Form->Top;
      OrigName = Form->Name;
      InitComponent(Form, __classid(TForm), Form->ClassType());
      Form->Name = OrigName;

      Form->Position = poDesigned;
      Form->Left = OrigLeft;
      Form->Top = OrigTop;
      SendMessage(Form->Handle, WM_LOCALE_CHANGE, 1, 1);
    }

    TDataModule * DataModule;
    Count = Screen->DataModuleCount;
    for (int Index = 0; Index < Count; Index++)
    {
      DataModule = Screen->DataModules[Index];
      OrigName = DataModule->Name;
      InitComponent(DataModule, __classid(TDataModule), DataModule->ClassType());
      DataModule->Name = OrigName;
    }
  }
  __finally
  {
    Busy(false);
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::CheckTranslationVersion(const UnicodeString FileName,
  bool InternalLocaleOnError)
{
  UnicodeString TranslationProductVersion = GetFileProductVersion(FileName);
  UnicodeString TranslationProductName = GetFileProductName(FileName);
  if ((ProductName != TranslationProductName) ||
      (ProductVersion != TranslationProductVersion))
  {
    if (InternalLocaleOnError)
    {
      LocaleSafe = InternalLocale();
    }

    if (TranslationProductName.IsEmpty() || TranslationProductVersion.IsEmpty())
    {
      throw Exception(FMTLOAD(UNKNOWN_TRANSLATION, (FileName)));
    }
    else
    {
      throw Exception(FMTLOAD(INCOMPATIBLE_TRANSLATION,
        (FileName, TranslationProductName, TranslationProductVersion)));
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::CheckDefaultTranslation()
{
  if (!FInvalidDefaultTranslationMessage.IsEmpty())
  {
    MoreMessageDialog(FMTLOAD(INVALID_DEFAULT_TRANSLATION,
      (FInvalidDefaultTranslationMessage)), NULL, qtWarning, qaOK, HELP_NONE);
  }
}
//---------------------------------------------------------------------------
const TEditorPreferences * __fastcall TWinConfiguration::DefaultEditorForFile(
  const UnicodeString FileName, bool Local, const TFileMasks::TParams & MaskParams)
{
  return FEditorList->Find(FileName, Local, MaskParams);
}
//---------------------------------------------------------------------------
const TEditorList * __fastcall TWinConfiguration::GetEditorList()
{
  return FEditorList;
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::SetEditorList(const TEditorList * value)
{
  if (!(*FEditorList == *value))
  {
    *FEditorList = *value;
    Changed();
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::UpdateSessionInJumpList(UnicodeString SessionName, bool Add)
{
  THierarchicalStorage * Storage = CreateScpStorage(false);
  try
  {
    FDontDecryptPasswords++;
    Storage->AccessMode = smReadWrite;

    TStringList * List = new TStringList();
    try
    {
      List->CaseSensitive = false;

      if (Storage->OpenSubKey(ConfigurationSubKey, true))
      {
        UnicodeString JumpList = Storage->ReadString(L"JumpList", L"");
        List->CommaText = JumpList;

        if (!SessionName.IsEmpty())
        {
          int Index = List->IndexOf(SessionName);
          if (Index >= 0)
          {
            List->Delete(Index);
          }

          if (Add)
          {
            List->Insert(0, SessionName);
          }
        }

        while (List->Count > 30)
        {
          List->Delete(List->Count - 1);
        }

        ::UpdateJumpList(List);

        UnicodeString NewJumpList = List->CommaText;
        if (NewJumpList != JumpList)
        {
          Storage->WriteString(L"JumpList", NewJumpList);
        }
      }
    }
    __finally
    {
      delete List;
    }
  }
  __finally
  {
    FDontDecryptPasswords--;
    delete Storage;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::AddSessionToJumpList(UnicodeString SessionName)
{
  UpdateSessionInJumpList(SessionName, true);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::DeleteSessionFromJumpList(UnicodeString SessionName)
{
  UpdateSessionInJumpList(SessionName, false);
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::UpdateJumpList()
{
  AddSessionToJumpList(L"");
}
//---------------------------------------------------------------------------
void __fastcall TWinConfiguration::UpdateStaticUsage()
{
  TCustomWinConfiguration::UpdateStaticUsage();
  Usage->Set(L"Interface", Interface);
  Usage->Set(L"CustomCommandsCount", (FCustomCommandsDefaults ? 0 : FCustomCommandList->Count));
  Usage->Set(L"UsingLocationProfiles", UseLocationProfiles);
  Usage->Set(L"UsingMasterPassword", UseMasterPassword);

  UnicodeString ExternalEditors;
  for (int Index = 0; Index < EditorList->Count; Index++)
  {
    const TEditorPreferences * Editor = EditorList->Editors[Index];
    if (Editor->Data->Editor == edExternal)
    {
      UnicodeString ExternalEditor = Editor->Data->ExternalEditor;
      ReformatFileNameCommand(ExternalEditor);
      UnicodeString Name = ExtractFileName(ExtractProgram(ExternalEditor));
      int Dot = Name.LastDelimiter(L".");
      if (Dot > 0)
      {
        Name = Name.SubString(1, Dot - 1);
      }
      AddToList(ExternalEditors, Name, ",");
    }
  }
  Usage->Set(L"ExternalEditors", ExternalEditors);
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
__fastcall TCustomCommandType::TCustomCommandType() :
  FParams(0), FShortCut(0)
{
}
//---------------------------------------------------------------------------
__fastcall TCustomCommandType::TCustomCommandType(const TCustomCommandType & Other) :
  FName(Other.FName),
  FCommand(Other.FCommand),
  FParams(Other.FParams),
  FShortCut(Other.FShortCut)
{
}
//---------------------------------------------------------------------------
TCustomCommandType & TCustomCommandType::operator=(const TCustomCommandType & Other)
{
  FName = Other.FName;
  FCommand = Other.FCommand;
  FParams = Other.FParams;
  FShortCut = Other.FShortCut;
  return *this;
}
//---------------------------------------------------------------------------
bool __fastcall TCustomCommandType::Equals(const TCustomCommandType * Other) const
{
  return
    (FName == Other->FName) &&
    (FCommand == Other->FCommand) &&
    (FParams == Other->FParams) &&
    (FShortCut == Other->FShortCut);
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
__fastcall TCustomCommandList::TCustomCommandList()
{
  FCommands = new TList();
  FModified = false;
}
//---------------------------------------------------------------------------
__fastcall TCustomCommandList::~TCustomCommandList()
{
  Clear();
  delete FCommands;
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Reset()
{
  FModified = false;
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Modify()
{
  FModified = true;
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Load(THierarchicalStorage * Storage)
{
  Clear();

  if (Storage->OpenSubKey(L"CustomCommands", false))
  {
    TStrings * Names = new TStringList();
    try
    {
      Storage->ReadValues(Names, true);
      for (int Index = 0; Index < Names->Count; Index++)
      {
        TCustomCommandType * Command = new TCustomCommandType();
        Command->Name = Names->Names[Index];
        Command->Command = Names->Values[Names->Names[Index]];
        FCommands->Add(Command);
      }
      Storage->CloseSubKey();
    }
    __finally
    {
      delete Names;
    }
  }

  if (Storage->OpenSubKey(L"CustomCommandsParams", false))
  {
    for (int Index = 0; Index < FCommands->Count; Index++)
    {
      TCustomCommandType * Command = GetCommand(Index);
      Command->Params = Storage->ReadInteger(Command->Name, Command->Params);
    }
    Storage->CloseSubKey();
  }

  if (Storage->OpenSubKey(L"CustomCommandsShortCuts", false))
  {
    for (int Index = 0; Index < FCommands->Count; Index++)
    {
      TCustomCommandType * Command = GetCommand(Index);
      Command->ShortCut = (Word)Storage->ReadInteger(Command->Name, Command->ShortCut);
    }
    Storage->CloseSubKey();
  }
  Reset();
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Save(THierarchicalStorage * Storage)
{
  if (Storage->OpenSubKey(L"CustomCommands", true))
  {
    Storage->ClearValues();
    for (int Index = 0; Index < FCommands->Count; Index++)
    {
      const TCustomCommandType * Command = Commands[Index];
      Storage->WriteString(Command->Name, Command->Command);
    }
    Storage->CloseSubKey();
  }
  if (Storage->OpenSubKey(L"CustomCommandsParams", true))
  {
    Storage->ClearValues();
    for (int Index = 0; Index < FCommands->Count; Index++)
    {
      const TCustomCommandType * Command = Commands[Index];
      Storage->WriteInteger(Command->Name, Command->Params);
    }
    Storage->CloseSubKey();
  }
  if (Storage->OpenSubKey(L"CustomCommandsShortCuts", true))
  {
    Storage->ClearValues();
    for (int Index = 0; Index < FCommands->Count; Index++)
    {
      const TCustomCommandType * Command = Commands[Index];
      if (Command->ShortCut != 0)
      {
        Storage->WriteInteger(Command->Name, Command->ShortCut);
      }
    }
    Storage->CloseSubKey();
  }
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Clear()
{
  for (int Index = 0; Index < FCommands->Count; Index++)
  {
    delete Commands[Index];
  }
  FCommands->Clear();
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Add(const UnicodeString Name,
  const UnicodeString ACommand, int Params)
{
  TCustomCommandType * Command = new TCustomCommandType();
  Command->Name = Name;
  Command->Command = ACommand;
  Command->Params = Params;
  Add(Command);
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Add(TCustomCommandType * Command)
{
  Insert(Count, Command);
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Insert(int Index, TCustomCommandType * Command)
{
  FCommands->Insert(Index, Command);
  Modify();
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Change(int Index, TCustomCommandType * ACommand)
{
  TCustomCommandType * Command = GetCommand(Index);
  if (!Command->Equals(ACommand))
  {
    delete Command;
    FCommands->Items[Index] = ACommand;
    Modify();
  }
  else
  {
    delete ACommand;
  }
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Move(int CurIndex, int NewIndex)
{
  if (CurIndex != NewIndex)
  {
    FCommands->Move(CurIndex, NewIndex);
    Modify();
  }
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Delete(int Index)
{
  assert((Index >= 0) && (Index < Count));
  delete GetCommand(Index);
  FCommands->Delete(Index);
  Modify();
}
//---------------------------------------------------------------------------
int __fastcall TCustomCommandList::GetCount() const
{
  return FCommands->Count;
}
//---------------------------------------------------------------------------
const TCustomCommandType * __fastcall TCustomCommandList::GetConstCommand(int Index) const
{
  return static_cast<TCustomCommandType *>(FCommands->Items[Index]);
}
//---------------------------------------------------------------------------
TCustomCommandType * __fastcall TCustomCommandList::GetCommand(int Index)
{
  return static_cast<TCustomCommandType *>(FCommands->Items[Index]);
}
//---------------------------------------------------------------------------
bool __fastcall TCustomCommandList::Equals(const TCustomCommandList * Other) const
{
  bool Result = (Count == Other->Count);
  for (int Index = 0; Result && (Index < Count); Index++)
  {
    Result = Commands[Index]->Equals(Other->Commands[Index]);
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::Assign(const TCustomCommandList * Other)
{
  Clear();
  for (int Index = 0; Index < Other->Count; Index++)
  {
    Add(new TCustomCommandType(*Other->Commands[Index]));
  }
  // there should be comparison of with the assigned list, be we rely on caller
  // to do it instead (TGUIConfiguration::SetCopyParamList)
  Modify();
}
//---------------------------------------------------------------------------
const TCustomCommandType * TCustomCommandList::Find(const UnicodeString Name) const
{
  for (int Index = 0; Index < FCommands->Count; Index++)
  {
    if (Commands[Index]->Name == Name)
    {
      return Commands[Index];
    }
  }
  return NULL;
}
//---------------------------------------------------------------------------
const TCustomCommandType * TCustomCommandList::Find(TShortCut ShortCut) const
{
  for (int Index = 0; Index < FCommands->Count; Index++)
  {
    if (Commands[Index]->ShortCut == ShortCut)
    {
      return Commands[Index];
    }
  }
  return NULL;
}
//---------------------------------------------------------------------------
void __fastcall TCustomCommandList::ShortCuts(TShortCuts & ShortCuts) const
{
  for (int Index = 0; Index < FCommands->Count; Index++)
  {
    const TCustomCommandType * Command = Commands[Index];
    if (Command->ShortCut != 0)
    {
      ShortCuts.Add(Command->ShortCut);
    }
  }
}
