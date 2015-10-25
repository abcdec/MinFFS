///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Jun  5 2014)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.13        *
// * Modifications made by abcdec @GitHub. https://github.com/abcdec/MinFFS *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
// **************************************************************************

#include "folder_history_box.h"
#include "on_completion_box.h"
#include "triple_splitter.h"
#include "wx+/bitmap_button.h"
#include "wx+/graph.h"
#include "wx+/grid.h"
#include "wx+/toggle_button.h"

#include "gui_generated.h"

///////////////////////////////////////////////////////////////////////////

MainDialogGenerated::MainDialogGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 640,400 ), wxDefaultSize );
	
	m_menubar1 = new wxMenuBar( 0 );
	m_menuFile = new wxMenu();
	m_menuItemNew = new wxMenuItem( m_menuFile, wxID_NEW, wxString( _("&New") ) + wxT('\t') + wxT("Ctrl+N"), wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItemNew );
	
	m_menuItemLoad = new wxMenuItem( m_menuFile, wxID_OPEN, wxString( _("&Open...") ) + wxT('\t') + wxT("Ctrl+O"), wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItemLoad );
	
	m_menuFile->AppendSeparator();
	
	m_menuItemSave = new wxMenuItem( m_menuFile, wxID_SAVE, wxString( _("&Save") ) + wxT('\t') + wxT("Ctrl+S"), wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItemSave );
	
	m_menuItemSaveAs = new wxMenuItem( m_menuFile, wxID_SAVEAS, wxString( _("Save &as...") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItemSaveAs );
	
	m_menuItemSaveAsBatch = new wxMenuItem( m_menuFile, wxID_ANY, wxString( _("Save as &batch job...") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItemSaveAsBatch );
	
	m_menuFile->AppendSeparator();
	
	wxMenuItem* m_menuItem4;
	m_menuItem4 = new wxMenuItem( m_menuFile, wxID_EXIT, wxString( _("E&xit") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuItem4 );
	
	m_menubar1->Append( m_menuFile, _("&File") ); 
	
	m_menu4 = new wxMenu();
	m_menuItemCompare = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("Start &comparison") ) + wxT('\t') + wxT("F5"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemCompare );
	
	m_menu4->AppendSeparator();
	
	m_menuItemCompSettings = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("C&omparison settings") ) + wxT('\t') + wxT("F6"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemCompSettings );
	
	m_menuItemFilter = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("&Filter settings") ) + wxT('\t') + wxT("F7"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemFilter );
	
	m_menuItemSyncSettings = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("S&ynchronization settings") ) + wxT('\t') + wxT("F8"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemSyncSettings );
	
	m_menu4->AppendSeparator();
	
	m_menuItemSynchronize = new wxMenuItem( m_menu4, wxID_ANY, wxString( _("Start &synchronization") ) + wxT('\t') + wxT("F9"), wxEmptyString, wxITEM_NORMAL );
	m_menu4->Append( m_menuItemSynchronize );
	
	m_menubar1->Append( m_menu4, _("&Actions") ); 
	
	m_menuTools = new wxMenu();
	m_menuItemOptions = new wxMenuItem( m_menuTools, wxID_PREFERENCES, wxString( _("&Options") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItemOptions );
	
	m_menuLanguages = new wxMenu();
	wxMenuItem* m_menuLanguagesItem = new wxMenuItem( m_menuTools, wxID_ANY, _("&Language"), wxEmptyString, wxITEM_NORMAL, m_menuLanguages );
	m_menuTools->Append( m_menuLanguagesItem );
	
	m_menuTools->AppendSeparator();
	
	wxMenuItem* m_menuItem15;
	m_menuItem15 = new wxMenuItem( m_menuTools, wxID_FIND, wxString( _("&Find...") ) + wxT('\t') + wxT("Ctrl+F"), wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItem15 );
	
	wxMenuItem* m_menuItem51;
	m_menuItem51 = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("&Reset layout") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItem51 );
	
	wxMenuItem* m_menuItem5;
	m_menuItem5 = new wxMenuItem( m_menuTools, wxID_ANY, wxString( _("&Export file list...") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuTools->Append( m_menuItem5 );
	
	m_menubar1->Append( m_menuTools, _("&Tools") ); 
	
	m_menuHelp = new wxMenu();
	m_menuItemHelp = new wxMenuItem( m_menuHelp, wxID_HELP, wxString( _("&View help") ) + wxT('\t') + wxT("F1"), wxEmptyString, wxITEM_NORMAL );
	m_menuHelp->Append( m_menuItemHelp );
	
	m_menuCheckVersion = new wxMenu();
	wxMenuItem* m_menuCheckVersionItem = new wxMenuItem( m_menuHelp, wxID_ANY, _("&Check for new version"), wxEmptyString, wxITEM_NORMAL, m_menuCheckVersion );
	m_menuItemCheckVersionNow = new wxMenuItem( m_menuCheckVersion, wxID_ANY, wxString( _("&Check now") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuCheckVersion->Append( m_menuItemCheckVersionNow );
	
	m_menuItemCheckVersionAuto = new wxMenuItem( m_menuCheckVersion, wxID_ANY, wxString( _("Check &automatically once a week") ) , wxEmptyString, wxITEM_CHECK );
	m_menuCheckVersion->Append( m_menuItemCheckVersionAuto );
	m_menuItemCheckVersionAuto->Check( true );
	
	m_menuHelp->Append( m_menuCheckVersionItem );
	
	m_menuHelp->AppendSeparator();
	
	m_menuItemAbout = new wxMenuItem( m_menuHelp, wxID_ABOUT, wxString( _("&About") ) + wxT('\t') + wxT("Shift+F1"), wxEmptyString, wxITEM_NORMAL );
	m_menuHelp->Append( m_menuItemAbout );
	
	m_menubar1->Append( m_menuHelp, _("&Help") ); 
	
	this->SetMenuBar( m_menubar1 );
	
	bSizerPanelHolder = new wxBoxSizer( wxVERTICAL );
	
	m_panelTopButtons = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxRAISED_BORDER|wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1791;
	bSizer1791 = new wxBoxSizer( wxVERTICAL );
	
	bSizerTopButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonCancel = new zen::BitmapTextButton( m_panelTopButtons, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonCancel->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	m_buttonCancel->Enable( false );
	m_buttonCancel->Hide();
	
	bSizerTopButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	m_buttonCompare = new zen::BitmapTextButton( m_panelTopButtons, wxID_ANY, _("Compare"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonCompare->SetDefault(); 
	m_buttonCompare->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	m_buttonCompare->SetToolTip( _("dummy") );
	
	bSizerTopButtons->Add( m_buttonCompare, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizerTopButtons->Add( 3, 3, 0, 0, 5 );
	
	m_bpButtonCmpConfig = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonCmpConfig->SetToolTip( _("dummy") );
	
	bSizerTopButtons->Add( m_bpButtonCmpConfig, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizerTopButtons->Add( 0, 0, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerTopButtons->Add( 5, 5, 0, 0, 5 );
	
	m_bpButtonFilter = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 60,-1 ), wxBU_AUTODRAW|wxFULL_REPAINT_ON_RESIZE );
	bSizerTopButtons->Add( m_bpButtonFilter, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizerTopButtons->Add( 5, 5, 0, 0, 5 );
	
	
	bSizerTopButtons->Add( 0, 0, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonSyncConfig = new wxBitmapButton( m_panelTopButtons, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSyncConfig->SetToolTip( _("dummy") );
	
	bSizerTopButtons->Add( m_bpButtonSyncConfig, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizerTopButtons->Add( 3, 3, 0, 0, 5 );
	
	m_buttonSync = new zen::BitmapTextButton( m_panelTopButtons, wxID_ANY, _("Synchronize"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonSync->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	m_buttonSync->SetToolTip( _("dummy") );
	
	bSizerTopButtons->Add( m_buttonSync, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer1791->Add( bSizerTopButtons, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	m_panelTopButtons->SetSizer( bSizer1791 );
	m_panelTopButtons->Layout();
	bSizer1791->Fit( m_panelTopButtons );
	bSizerPanelHolder->Add( m_panelTopButtons, 0, wxALIGN_CENTER_HORIZONTAL|wxEXPAND, 5 );
	
	m_panelDirectoryPairs = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSTATIC_BORDER|wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1601;
	bSizer1601 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer91;
	bSizer91 = new wxBoxSizer( wxHORIZONTAL );
	
	m_panelTopLeft = new wxPanel( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelTopLeft->SetMinSize( wxSize( 1,-1 ) );
	
	wxFlexGridSizer* fgSizer8;
	fgSizer8 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer8->AddGrowableCol( 1 );
	fgSizer8->SetFlexibleDirection( wxBOTH );
	fgSizer8->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_ALL );
	
	
	fgSizer8->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextResolvedPathL = new wxStaticText( m_panelTopLeft, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextResolvedPathL->Wrap( -1 );
	fgSizer8->Add( m_staticTextResolvedPathL, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
	
	wxBoxSizer* bSizer159;
	bSizer159 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonAddPair = new wxBitmapButton( m_panelTopLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	m_bpButtonAddPair->SetToolTip( _("Add folder pair") );
	
	bSizer159->Add( m_bpButtonAddPair, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonRemovePair = new wxBitmapButton( m_panelTopLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	m_bpButtonRemovePair->SetToolTip( _("Remove folder pair") );
	
	bSizer159->Add( m_bpButtonRemovePair, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	fgSizer8->Add( bSizer159, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	wxBoxSizer* bSizer182;
	bSizer182 = new wxBoxSizer( wxHORIZONTAL );
	
	m_directoryLeft = new FolderHistoryBox( m_panelTopLeft, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer182->Add( m_directoryLeft, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectDirLeft = new wxButton( m_panelTopLeft, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectDirLeft->SetToolTip( _("Select a folder") );
	
	bSizer182->Add( m_buttonSelectDirLeft, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	fgSizer8->Add( bSizer182, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	m_panelTopLeft->SetSizer( fgSizer8 );
	m_panelTopLeft->Layout();
	fgSizer8->Fit( m_panelTopLeft );
	bSizer91->Add( m_panelTopLeft, 1, wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_panelTopMiddle = new wxPanel( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1771;
	bSizer1771 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer1771->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_bpButtonSwapSides = new wxBitmapButton( m_panelTopMiddle, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), wxBU_AUTODRAW );
	m_bpButtonSwapSides->SetToolTip( _("Swap sides") );
	
	bSizer1771->Add( m_bpButtonSwapSides, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxEXPAND, 5 );
	
	wxBoxSizer* bSizer160;
	bSizer160 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonAltCompCfg = new wxBitmapButton( m_panelTopMiddle, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	bSizer160->Add( m_bpButtonAltCompCfg, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonLocalFilter = new wxBitmapButton( m_panelTopMiddle, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	bSizer160->Add( m_bpButtonLocalFilter, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 2 );
	
	m_bpButtonAltSyncCfg = new wxBitmapButton( m_panelTopMiddle, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	bSizer160->Add( m_bpButtonAltSyncCfg, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer1771->Add( bSizer160, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer1771->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	m_panelTopMiddle->SetSizer( bSizer1771 );
	m_panelTopMiddle->Layout();
	bSizer1771->Fit( m_panelTopMiddle );
	bSizer91->Add( m_panelTopMiddle, 0, wxRIGHT|wxLEFT|wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	m_panelTopRight = new wxPanel( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelTopRight->SetMinSize( wxSize( 1,-1 ) );
	
	wxBoxSizer* bSizer183;
	bSizer183 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextResolvedPathR = new wxStaticText( m_panelTopRight, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextResolvedPathR->Wrap( -1 );
	bSizer183->Add( m_staticTextResolvedPathR, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
	
	wxBoxSizer* bSizer179;
	bSizer179 = new wxBoxSizer( wxHORIZONTAL );
	
	m_directoryRight = new FolderHistoryBox( m_panelTopRight, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer179->Add( m_directoryRight, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectDirRight = new wxButton( m_panelTopRight, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectDirRight->SetToolTip( _("Select a folder") );
	
	bSizer179->Add( m_buttonSelectDirRight, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer183->Add( bSizer179, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	m_panelTopRight->SetSizer( bSizer183 );
	m_panelTopRight->Layout();
	bSizer183->Fit( m_panelTopRight );
	bSizer91->Add( m_panelTopRight, 1, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	
	bSizer1601->Add( bSizer91, 0, wxEXPAND, 5 );
	
	m_scrolledWindowFolderPairs = new wxScrolledWindow( m_panelDirectoryPairs, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), wxHSCROLL|wxVSCROLL );
	m_scrolledWindowFolderPairs->SetScrollRate( 10, 10 );
	m_scrolledWindowFolderPairs->SetMinSize( wxSize( -1,0 ) );
	
	bSizerAddFolderPairs = new wxBoxSizer( wxVERTICAL );
	
	
	m_scrolledWindowFolderPairs->SetSizer( bSizerAddFolderPairs );
	m_scrolledWindowFolderPairs->Layout();
	bSizerAddFolderPairs->Fit( m_scrolledWindowFolderPairs );
	bSizer1601->Add( m_scrolledWindowFolderPairs, 1, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	m_panelDirectoryPairs->SetSizer( bSizer1601 );
	m_panelDirectoryPairs->Layout();
	bSizer1601->Fit( m_panelDirectoryPairs );
	bSizerPanelHolder->Add( m_panelDirectoryPairs, 0, wxALIGN_CENTER_HORIZONTAL|wxEXPAND, 5 );
	
	m_gridNavi = new zen::Grid( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridNavi->SetScrollRate( 5, 5 );
	bSizerPanelHolder->Add( m_gridNavi, 1, wxEXPAND, 5 );
	
	m_panelCenter = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1711;
	bSizer1711 = new wxBoxSizer( wxVERTICAL );
	
	m_splitterMain = new zen::TripleSplitter( m_panelCenter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1781;
	bSizer1781 = new wxBoxSizer( wxHORIZONTAL );
	
	m_gridMainL = new zen::Grid( m_splitterMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridMainL->SetScrollRate( 5, 5 );
	bSizer1781->Add( m_gridMainL, 1, wxEXPAND, 5 );
	
	m_gridMainC = new zen::Grid( m_splitterMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridMainC->SetScrollRate( 5, 5 );
	bSizer1781->Add( m_gridMainC, 0, wxEXPAND, 5 );
	
	m_gridMainR = new zen::Grid( m_splitterMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridMainR->SetScrollRate( 5, 5 );
	bSizer1781->Add( m_gridMainR, 1, wxEXPAND, 5 );
	
	
	m_splitterMain->SetSizer( bSizer1781 );
	m_splitterMain->Layout();
	bSizer1781->Fit( m_splitterMain );
	bSizer1711->Add( m_splitterMain, 1, wxEXPAND, 5 );
	
	m_panelStatusBar = new wxPanel( m_panelCenter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSTATIC_BORDER|wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer451;
	bSizer451 = new wxBoxSizer( wxHORIZONTAL );
	
	bSizer451->SetMinSize( wxSize( -1,22 ) ); 
	bSizerFileStatus = new wxBoxSizer( wxHORIZONTAL );
	
	bSizerStatusLeft = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer53;
	bSizer53 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer53->Add( 0, 0, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	bSizerStatusLeftDirectories = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapSmallDirectoryLeft = new wxStaticBitmap( m_panelStatusBar, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStatusLeftDirectories->Add( m_bitmapSmallDirectoryLeft, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusLeftDirectories->Add( 2, 0, 0, 0, 5 );
	
	m_staticTextStatusLeftDirs = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusLeftDirs->Wrap( -1 );
	bSizerStatusLeftDirectories->Add( m_staticTextStatusLeftDirs, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer53->Add( bSizerStatusLeftDirectories, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	bSizerStatusLeftFiles = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizerStatusLeftFiles->Add( 10, 0, 0, 0, 5 );
	
	m_bitmapSmallFileLeft = new wxStaticBitmap( m_panelStatusBar, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStatusLeftFiles->Add( m_bitmapSmallFileLeft, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusLeftFiles->Add( 2, 0, 0, 0, 5 );
	
	m_staticTextStatusLeftFiles = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusLeftFiles->Wrap( -1 );
	bSizerStatusLeftFiles->Add( m_staticTextStatusLeftFiles, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusLeftFiles->Add( 4, 0, 0, 0, 5 );
	
	m_staticTextStatusLeftBytes = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusLeftBytes->Wrap( -1 );
	bSizerStatusLeftFiles->Add( m_staticTextStatusLeftBytes, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer53->Add( bSizerStatusLeftFiles, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer53->Add( 0, 0, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusLeft->Add( bSizer53, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticline9 = new wxStaticLine( m_panelStatusBar, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizerStatusLeft->Add( m_staticline9, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxTOP, 2 );
	
	
	bSizerFileStatus->Add( bSizerStatusLeft, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerFileStatus->Add( 26, 0, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextStatusMiddle = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusMiddle->Wrap( -1 );
	bSizerFileStatus->Add( m_staticTextStatusMiddle, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerFileStatus->Add( 26, 0, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	bSizerStatusRight = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticline10 = new wxStaticLine( m_panelStatusBar, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizerStatusRight->Add( m_staticline10, 0, wxEXPAND|wxALIGN_CENTER_VERTICAL|wxTOP, 2 );
	
	wxBoxSizer* bSizer52;
	bSizer52 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer52->Add( 0, 0, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	bSizerStatusRightDirectories = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapSmallDirectoryRight = new wxStaticBitmap( m_panelStatusBar, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStatusRightDirectories->Add( m_bitmapSmallDirectoryRight, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusRightDirectories->Add( 2, 0, 0, 0, 5 );
	
	m_staticTextStatusRightDirs = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusRightDirs->Wrap( -1 );
	bSizerStatusRightDirectories->Add( m_staticTextStatusRightDirs, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer52->Add( bSizerStatusRightDirectories, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	bSizerStatusRightFiles = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizerStatusRightFiles->Add( 10, 0, 0, 0, 5 );
	
	m_bitmapSmallFileRight = new wxStaticBitmap( m_panelStatusBar, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStatusRightFiles->Add( m_bitmapSmallFileRight, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusRightFiles->Add( 2, 0, 0, 0, 5 );
	
	m_staticTextStatusRightFiles = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusRightFiles->Wrap( -1 );
	bSizerStatusRightFiles->Add( m_staticTextStatusRightFiles, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusRightFiles->Add( 4, 0, 0, 0, 5 );
	
	m_staticTextStatusRightBytes = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatusRightBytes->Wrap( -1 );
	bSizerStatusRightFiles->Add( m_staticTextStatusRightBytes, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer52->Add( bSizerStatusRightFiles, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer52->Add( 0, 0, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatusRight->Add( bSizer52, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerFileStatus->Add( bSizerStatusRight, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer451->Add( bSizerFileStatus, 1, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	m_staticTextFullStatus = new wxStaticText( m_panelStatusBar, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextFullStatus->Wrap( -1 );
	m_staticTextFullStatus->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer451->Add( m_staticTextFullStatus, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	m_panelStatusBar->SetSizer( bSizer451 );
	m_panelStatusBar->Layout();
	bSizer451->Fit( m_panelStatusBar );
	bSizer1711->Add( m_panelStatusBar, 0, wxALIGN_CENTER_HORIZONTAL|wxEXPAND, 5 );
	
	
	m_panelCenter->SetSizer( bSizer1711 );
	m_panelCenter->Layout();
	bSizer1711->Fit( m_panelCenter );
	bSizerPanelHolder->Add( m_panelCenter, 1, wxEXPAND, 5 );
	
	m_panelSearch = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer1713;
	bSizer1713 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonHideSearch = new wxBitmapButton( m_panelSearch, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	m_bpButtonHideSearch->SetToolTip( _("Close search bar") );
	
	bSizer1713->Add( m_bpButtonHideSearch, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_staticText101 = new wxStaticText( m_panelSearch, wxID_ANY, _("Find:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText101->Wrap( -1 );
	bSizer1713->Add( m_staticText101, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_textCtrlSearchTxt = new wxTextCtrl( m_panelSearch, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 220,-1 ), 0|wxWANTS_CHARS );
	m_textCtrlSearchTxt->SetMaxLength( 0 ); 
	bSizer1713->Add( m_textCtrlSearchTxt, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	m_checkBoxMatchCase = new wxCheckBox( m_panelSearch, wxID_ANY, _("Match case"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer1713->Add( m_checkBoxMatchCase, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	m_panelSearch->SetSizer( bSizer1713 );
	m_panelSearch->Layout();
	bSizer1713->Fit( m_panelSearch );
	bSizerPanelHolder->Add( m_panelSearch, 0, 0, 5 );
	
	m_panelConfig = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	bSizerConfig = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer151;
	bSizer151 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer17611;
	bSizer17611 = new wxBoxSizer( wxVERTICAL );
	
	m_bpButtonNew = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	m_bpButtonNew->SetToolTip( _("dummy") );
	
	bSizer17611->Add( m_bpButtonNew, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_staticText951 = new wxStaticText( m_panelConfig, wxID_ANY, _("New"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText951->Wrap( -1 );
	bSizer17611->Add( m_staticText951, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );
	
	
	bSizer151->Add( bSizer17611, 0, 0, 5 );
	
	wxBoxSizer* bSizer1761;
	bSizer1761 = new wxBoxSizer( wxVERTICAL );
	
	m_bpButtonOpen = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	m_bpButtonOpen->SetToolTip( _("dummy") );
	
	bSizer1761->Add( m_bpButtonOpen, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_staticText95 = new wxStaticText( m_panelConfig, wxID_ANY, _("Open..."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText95->Wrap( -1 );
	bSizer1761->Add( m_staticText95, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );
	
	
	bSizer151->Add( bSizer1761, 0, 0, 5 );
	
	wxBoxSizer* bSizer175;
	bSizer175 = new wxBoxSizer( wxVERTICAL );
	
	m_bpButtonSave = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	m_bpButtonSave->SetToolTip( _("dummy") );
	
	bSizer175->Add( m_bpButtonSave, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_staticText961 = new wxStaticText( m_panelConfig, wxID_ANY, _("Save"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText961->Wrap( -1 );
	bSizer175->Add( m_staticText961, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );
	
	
	bSizer151->Add( bSizer175, 0, 0, 5 );
	
	wxBoxSizer* bSizer174;
	bSizer174 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer1772;
	bSizer1772 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonSaveAs = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	m_bpButtonSaveAs->SetToolTip( _("dummy") );
	
	bSizer1772->Add( m_bpButtonSaveAs, 0, 0, 5 );
	
	m_bpButtonSaveAsBatch = new wxBitmapButton( m_panelConfig, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	m_bpButtonSaveAsBatch->SetToolTip( _("dummy") );
	
	bSizer1772->Add( m_bpButtonSaveAsBatch, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer174->Add( bSizer1772, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_staticText97 = new wxStaticText( m_panelConfig, wxID_ANY, _("Save as..."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText97->Wrap( -1 );
	bSizer174->Add( m_staticText97, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT, 2 );
	
	
	bSizer151->Add( bSizer174, 0, 0, 5 );
	
	
	bSizerConfig->Add( bSizer151, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_listBoxHistory = new wxListBox( m_panelConfig, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_EXTENDED|wxLB_NEEDED_SB ); 
	m_listBoxHistory->SetMinSize( wxSize( -1,40 ) );
	
	bSizerConfig->Add( m_listBoxHistory, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	m_panelConfig->SetSizer( bSizerConfig );
	m_panelConfig->Layout();
	bSizerConfig->Fit( m_panelConfig );
	bSizerPanelHolder->Add( m_panelConfig, 0, 0, 5 );
	
	m_panelViewFilter = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	bSizerViewFilter = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextViewType = new wxStaticText( m_panelViewFilter, wxID_ANY, _("View type:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextViewType->Wrap( -1 );
	bSizerViewFilter->Add( m_staticTextViewType, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonViewTypeSyncAction = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 82,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonViewTypeSyncAction, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxRIGHT, 5 );
	
	m_bpButtonShowExcluded = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowExcluded, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerViewFilter->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextSelectView = new wxStaticText( m_panelViewFilter, wxID_ANY, _("Select view:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextSelectView->Wrap( -1 );
	bSizerViewFilter->Add( m_staticTextSelectView, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowDeleteLeft = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowDeleteLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowUpdateLeft = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowUpdateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowCreateLeft = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowCreateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowLeftOnly = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowLeftOnly, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowLeftNewer = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowLeftNewer, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowEqual = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowEqual, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowDoNothing = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowDoNothing, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowDifferent = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowDifferent, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowRightNewer = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowRightNewer, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowRightOnly = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowRightOnly, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowCreateRight = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowCreateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowUpdateRight = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowUpdateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowDeleteRight = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowDeleteRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonShowConflict = new ToggleButton( m_panelViewFilter, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 42,42 ), wxBU_AUTODRAW );
	bSizerViewFilter->Add( m_bpButtonShowConflict, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerViewFilter->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticText96 = new wxStaticText( m_panelViewFilter, wxID_ANY, _("Statistics:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText96->Wrap( -1 );
	bSizerViewFilter->Add( m_staticText96, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_panelStatistics = new wxPanel( m_panelViewFilter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
	m_panelStatistics->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer1801 = new wxBoxSizer( wxVERTICAL );
	
	bSizerStatistics = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer173;
	bSizer173 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapDeleteLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapDeleteLeft->SetToolTip( _("Number of files and folders that will be deleted") );
	
	bSizer173->Add( m_bitmapDeleteLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer173->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer173->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextDeleteLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDeleteLeft->Wrap( -1 );
	m_staticTextDeleteLeft->SetToolTip( _("Number of files and folders that will be deleted") );
	
	bSizer173->Add( m_staticTextDeleteLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatistics->Add( bSizer173, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer172;
	bSizer172 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapUpdateLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapUpdateLeft->SetToolTip( _("Number of files that will be updated") );
	
	bSizer172->Add( m_bitmapUpdateLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer172->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer172->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextUpdateLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextUpdateLeft->Wrap( -1 );
	m_staticTextUpdateLeft->SetToolTip( _("Number of files that will be updated") );
	
	bSizer172->Add( m_staticTextUpdateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerStatistics->Add( bSizer172, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer1712;
	bSizer1712 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapCreateLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapCreateLeft->SetToolTip( _("Number of files and folders that will be created") );
	
	bSizer1712->Add( m_bitmapCreateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer1712->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer1712->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextCreateLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCreateLeft->Wrap( -1 );
	m_staticTextCreateLeft->SetToolTip( _("Number of files and folders that will be created") );
	
	bSizer1712->Add( m_staticTextCreateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerStatistics->Add( bSizer1712, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	bSizerData = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapData = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapData->SetToolTip( _("Total bytes to copy") );
	
	bSizerData->Add( m_bitmapData, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizerData->Add( 5, 2, 0, 0, 5 );
	
	
	bSizerData->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextData = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextData->Wrap( -1 );
	m_staticTextData->SetToolTip( _("Total bytes to copy") );
	
	bSizerData->Add( m_staticTextData, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatistics->Add( bSizerData, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer178;
	bSizer178 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapCreateRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapCreateRight->SetToolTip( _("Number of files and folders that will be created") );
	
	bSizer178->Add( m_bitmapCreateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer178->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer178->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextCreateRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCreateRight->Wrap( -1 );
	m_staticTextCreateRight->SetToolTip( _("Number of files and folders that will be created") );
	
	bSizer178->Add( m_staticTextCreateRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatistics->Add( bSizer178, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer177;
	bSizer177 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapUpdateRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapUpdateRight->SetToolTip( _("Number of files that will be updated") );
	
	bSizer177->Add( m_bitmapUpdateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer177->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer177->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextUpdateRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextUpdateRight->Wrap( -1 );
	m_staticTextUpdateRight->SetToolTip( _("Number of files that will be updated") );
	
	bSizer177->Add( m_staticTextUpdateRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatistics->Add( bSizer177, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizerStatistics->Add( 5, 5, 0, 0, 5 );
	
	wxBoxSizer* bSizer176;
	bSizer176 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapDeleteRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapDeleteRight->SetToolTip( _("Number of files and folders that will be deleted") );
	
	bSizer176->Add( m_bitmapDeleteRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer176->Add( 5, 2, 0, 0, 5 );
	
	
	bSizer176->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextDeleteRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDeleteRight->Wrap( -1 );
	m_staticTextDeleteRight->SetToolTip( _("Number of files and folders that will be deleted") );
	
	bSizer176->Add( m_staticTextDeleteRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStatistics->Add( bSizer176, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer1801->Add( bSizerStatistics, 0, wxALIGN_CENTER_VERTICAL|wxALL, 4 );
	
	
	m_panelStatistics->SetSizer( bSizer1801 );
	m_panelStatistics->Layout();
	bSizer1801->Fit( m_panelStatistics );
	bSizerViewFilter->Add( m_panelStatistics, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	m_panelViewFilter->SetSizer( bSizerViewFilter );
	m_panelViewFilter->Layout();
	bSizerViewFilter->Fit( m_panelViewFilter );
	bSizerPanelHolder->Add( m_panelViewFilter, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	this->SetSizer( bSizerPanelHolder );
	this->Layout();
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( MainDialogGenerated::OnClose ) );
	this->Connect( m_menuItemNew->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnConfigNew ) );
	this->Connect( m_menuItemLoad->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnConfigLoad ) );
	this->Connect( m_menuItemSave->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnConfigSave ) );
	this->Connect( m_menuItemSaveAs->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnConfigSaveAs ) );
	this->Connect( m_menuItemSaveAsBatch->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnSaveAsBatchJob ) );
	this->Connect( m_menuItem4->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuQuit ) );
	this->Connect( m_menuItemCompare->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnCompare ) );
	this->Connect( m_menuItemCompSettings->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnCmpSettings ) );
	this->Connect( m_menuItemFilter->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnConfigureFilter ) );
	this->Connect( m_menuItemSyncSettings->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnSyncSettings ) );
	this->Connect( m_menuItemSynchronize->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnStartSync ) );
	this->Connect( m_menuItemOptions->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuOptions ) );
	this->Connect( m_menuItem15->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuFindItem ) );
	this->Connect( m_menuItem51->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuResetLayout ) );
	this->Connect( m_menuItem5->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuExportFileList ) );
	this->Connect( m_menuItemHelp->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnShowHelp ) );
	this->Connect( m_menuItemCheckVersionNow->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuCheckVersion ) );
	this->Connect( m_menuItemCheckVersionAuto->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuCheckVersionAutomatically ) );
	this->Connect( m_menuItemAbout->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnMenuAbout ) );
	m_buttonCompare->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnCompare ), NULL, this );
	m_bpButtonCmpConfig->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnCmpSettings ), NULL, this );
	m_bpButtonCmpConfig->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnCompSettingsContext ), NULL, this );
	m_bpButtonFilter->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnConfigureFilter ), NULL, this );
	m_bpButtonFilter->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnGlobalFilterContext ), NULL, this );
	m_bpButtonSyncConfig->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnSyncSettings ), NULL, this );
	m_bpButtonSyncConfig->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnSyncSettingsContext ), NULL, this );
	m_buttonSync->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnStartSync ), NULL, this );
	m_bpButtonAddPair->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnTopFolderPairAdd ), NULL, this );
	m_bpButtonRemovePair->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnTopFolderPairRemove ), NULL, this );
	m_bpButtonSwapSides->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnSwapSides ), NULL, this );
	m_bpButtonHideSearch->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnHideSearchPanel ), NULL, this );
	m_textCtrlSearchTxt->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainDialogGenerated::OnSearchGridEnter ), NULL, this );
	m_bpButtonNew->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnConfigNew ), NULL, this );
	m_bpButtonOpen->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnConfigLoad ), NULL, this );
	m_bpButtonSave->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnConfigSave ), NULL, this );
	m_bpButtonSaveAs->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnConfigSaveAs ), NULL, this );
	m_bpButtonSaveAsBatch->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnSaveAsBatchJob ), NULL, this );
	m_listBoxHistory->Connect( wxEVT_KEY_DOWN, wxKeyEventHandler( MainDialogGenerated::OnCfgHistoryKeyEvent ), NULL, this );
	m_listBoxHistory->Connect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( MainDialogGenerated::OnLoadFromHistory ), NULL, this );
	m_listBoxHistory->Connect( wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, wxCommandEventHandler( MainDialogGenerated::OnLoadFromHistoryDoubleClick ), NULL, this );
	m_listBoxHistory->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnCfgHistoryRightClick ), NULL, this );
	m_bpButtonViewTypeSyncAction->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewType ), NULL, this );
	m_bpButtonShowExcluded->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowExcluded->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowDeleteLeft->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowDeleteLeft->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowUpdateLeft->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowUpdateLeft->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowCreateLeft->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowCreateLeft->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowLeftOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowLeftOnly->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowLeftNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowLeftNewer->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowEqual->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowEqual->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowDoNothing->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowDoNothing->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowDifferent->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowDifferent->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowRightNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowRightNewer->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowRightOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowRightOnly->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowCreateRight->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowCreateRight->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowUpdateRight->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowUpdateRight->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowDeleteRight->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowDeleteRight->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
	m_bpButtonShowConflict->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainDialogGenerated::OnToggleViewButton ), NULL, this );
	m_bpButtonShowConflict->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( MainDialogGenerated::OnViewButtonRightClick ), NULL, this );
}

MainDialogGenerated::~MainDialogGenerated()
{
}

ConfigDlgGenerated::ConfigDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer7;
	bSizer7 = new wxBoxSizer( wxVERTICAL );
	
	m_notebook = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelCompSettingsHolder = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelCompSettingsHolder->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer275;
	bSizer275 = new wxBoxSizer( wxVERTICAL );
	
	bSizerLocalCompSettings = new wxBoxSizer( wxVERTICAL );
	
	m_checkBoxUseLocalCmpOptions = new wxCheckBox( m_panelCompSettingsHolder, wxID_ANY, _("Use local settings:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxUseLocalCmpOptions->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizerLocalCompSettings->Add( m_checkBoxUseLocalCmpOptions, 0, wxALL|wxEXPAND, 10 );
	
	m_staticline59 = new wxStaticLine( m_panelCompSettingsHolder, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerLocalCompSettings->Add( m_staticline59, 0, wxEXPAND, 5 );
	
	
	bSizer275->Add( bSizerLocalCompSettings, 0, wxEXPAND, 5 );
	
	m_panelComparisonSettings = new wxPanel( m_panelCompSettingsHolder, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelComparisonSettings->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer159;
	bSizer159 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer178;
	bSizer178 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer182;
	bSizer182 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText91 = new wxStaticText( m_panelComparisonSettings, wxID_ANY, _("Select a variant:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText91->Wrap( -1 );
	bSizer182->Add( m_staticText91, 0, wxALL, 5 );
	
	wxFlexGridSizer* fgSizer16;
	fgSizer16 = new wxFlexGridSizer( 2, 2, 5, 5 );
	fgSizer16->SetFlexibleDirection( wxBOTH );
	fgSizer16->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_bitmapByTime = new wxStaticBitmap( m_panelComparisonSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapByTime->SetToolTip( _("Identify equal files by comparing modification time and size.") );
	
	fgSizer16->Add( m_bitmapByTime, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_toggleBtnTimeSize = new wxToggleButton( m_panelComparisonSettings, wxID_ANY, _("File time and size"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnTimeSize->SetValue( true ); 
	m_toggleBtnTimeSize->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	fgSizer16->Add( m_toggleBtnTimeSize, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	m_bitmapByContent = new wxStaticBitmap( m_panelComparisonSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapByContent->SetToolTip( _("Identify equal files by comparing the file content.") );
	
	fgSizer16->Add( m_bitmapByContent, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_toggleBtnContent = new wxToggleButton( m_panelComparisonSettings, wxID_ANY, _("File content"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_toggleBtnContent->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	fgSizer16->Add( m_toggleBtnContent, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer182->Add( fgSizer16, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM|wxRIGHT|wxLEFT|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer178->Add( bSizer182, 0, wxALL, 5 );
	
	m_staticline42 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer178->Add( m_staticline42, 0, wxEXPAND, 5 );
	
	m_textCtrlCompVarDescription = new wxTextCtrl( m_panelComparisonSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY|wxNO_BORDER );
	bSizer178->Add( m_textCtrlCompVarDescription, 1, wxEXPAND|wxLEFT, 5 );
	
	
	bSizer159->Add( bSizer178, 0, wxEXPAND, 5 );
	
	m_staticline33 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer159->Add( m_staticline33, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer1734;
	bSizer1734 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer1733;
	bSizer1733 = new wxBoxSizer( wxVERTICAL );
	
	m_checkBoxTimeShift = new wxCheckBox( m_panelComparisonSettings, wxID_ANY, _("Ignore time shift (in hours)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxTimeShift->SetToolTip( _("Consider file times with specified offset as equal") );
	
	bSizer1733->Add( m_checkBoxTimeShift, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_spinCtrlTimeShift = new wxSpinCtrl( m_panelComparisonSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 70,-1 ), wxSP_ARROW_KEYS|wxSP_WRAP, 1, 26, 0 );
	m_spinCtrlTimeShift->SetToolTip( _("Consider file times with specified offset as equal") );
	
	bSizer1733->Add( m_spinCtrlTimeShift, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	m_hyperlink241 = new wxHyperlinkCtrl( m_panelComparisonSettings, wxID_ANY, _("Handle daylight saving time"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer1733->Add( m_hyperlink241, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer1734->Add( bSizer1733, 0, wxALL, 5 );
	
	m_staticline44 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer1734->Add( m_staticline44, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer1721;
	bSizer1721 = new wxBoxSizer( wxVERTICAL );
	
	m_checkBoxSymlinksInclude = new wxCheckBox( m_panelComparisonSettings, wxID_ANY, _("Include symbolic links:"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer1721->Add( m_checkBoxSymlinksInclude, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer176;
	bSizer176 = new wxBoxSizer( wxVERTICAL );
	
	m_radioBtnSymlinksFollow = new wxRadioButton( m_panelComparisonSettings, wxID_ANY, _("Follow"), wxDefaultPosition, wxDefaultSize, 0 );
	m_radioBtnSymlinksFollow->SetValue( true ); 
	bSizer176->Add( m_radioBtnSymlinksFollow, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	m_radioBtnSymlinksDirect = new wxRadioButton( m_panelComparisonSettings, wxID_ANY, _("Direct"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer176->Add( m_radioBtnSymlinksDirect, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer1721->Add( bSizer176, 0, wxEXPAND|wxLEFT, 18 );
	
	m_hyperlink24 = new wxHyperlinkCtrl( m_panelComparisonSettings, wxID_ANY, _("More information"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer1721->Add( m_hyperlink24, 0, wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer1734->Add( bSizer1721, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_staticline441 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer1734->Add( m_staticline441, 0, wxEXPAND, 5 );
	
	
	bSizer159->Add( bSizer1734, 0, 0, 5 );
	
	m_staticline331 = new wxStaticLine( m_panelComparisonSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer159->Add( m_staticline331, 0, wxEXPAND, 5 );
	
	
	m_panelComparisonSettings->SetSizer( bSizer159 );
	m_panelComparisonSettings->Layout();
	bSizer159->Fit( m_panelComparisonSettings );
	bSizer275->Add( m_panelComparisonSettings, 0, wxEXPAND, 5 );
	
	
	m_panelCompSettingsHolder->SetSizer( bSizer275 );
	m_panelCompSettingsHolder->Layout();
	bSizer275->Fit( m_panelCompSettingsHolder );
	m_notebook->AddPage( m_panelCompSettingsHolder, _("dummy"), true );
	m_panelFilterSettingsHolder = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelFilterSettingsHolder->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer278;
	bSizer278 = new wxBoxSizer( wxVERTICAL );
	
	bSizerLocalFilterSettings = new wxBoxSizer( wxVERTICAL );
	
	m_staticText144 = new wxStaticText( m_panelFilterSettingsHolder, wxID_ANY, _("Local settings:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText144->Wrap( -1 );
	bSizerLocalFilterSettings->Add( m_staticText144, 0, wxALL, 10 );
	
	m_staticline61 = new wxStaticLine( m_panelFilterSettingsHolder, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerLocalFilterSettings->Add( m_staticline61, 0, wxEXPAND, 5 );
	
	
	bSizer278->Add( bSizerLocalFilterSettings, 0, wxEXPAND, 5 );
	
	m_panelFilterSettings = new wxPanel( m_panelFilterSettingsHolder, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelFilterSettings->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer1591;
	bSizer1591 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer166;
	bSizer166 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer166->Add( 0, 10, 0, 0, 5 );
	
	wxBoxSizer* bSizer1661;
	bSizer1661 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapInclude = new wxStaticBitmap( m_panelFilterSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 30,30 ), 0 );
	bSizer1661->Add( m_bitmapInclude, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );
	
	wxBoxSizer* bSizer1731;
	bSizer1731 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText78 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Include:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText78->Wrap( -1 );
	bSizer1731->Add( m_staticText78, 0, 0, 5 );
	
	m_textCtrlInclude = new wxTextCtrl( m_panelFilterSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_MULTILINE );
	m_textCtrlInclude->SetMinSize( wxSize( 280,-1 ) );
	
	bSizer1731->Add( m_textCtrlInclude, 1, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxTOP, 5 );
	
	
	bSizer1661->Add( bSizer1731, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer166->Add( bSizer1661, 1, wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxLEFT, 5 );
	
	m_staticline22 = new wxStaticLine( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer166->Add( m_staticline22, 0, wxEXPAND, 5 );
	
	
	bSizer166->Add( 0, 10, 0, 0, 5 );
	
	wxBoxSizer* bSizer1651;
	bSizer1651 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapExclude = new wxStaticBitmap( m_panelFilterSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 30,30 ), 0 );
	bSizer1651->Add( m_bitmapExclude, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	wxBoxSizer* bSizer1742;
	bSizer1742 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer189;
	bSizer189 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText77 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Exclude:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText77->Wrap( -1 );
	bSizer189->Add( m_staticText77, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer189->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_hyperlink171 = new wxHyperlinkCtrl( m_panelFilterSettings, wxID_ANY, _("Show examples"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer189->Add( m_hyperlink171, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer1742->Add( bSizer189, 0, wxEXPAND, 5 );
	
	m_textCtrlExclude = new wxTextCtrl( m_panelFilterSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_MULTILINE );
	bSizer1742->Add( m_textCtrlExclude, 1, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxTOP, 5 );
	
	
	bSizer1651->Add( bSizer1742, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer166->Add( bSizer1651, 2, wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxLEFT, 5 );
	
	
	bSizer1591->Add( bSizer166, 1, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	m_staticline24 = new wxStaticLine( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer1591->Add( m_staticline24, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	wxBoxSizer* bSizer160;
	bSizer160 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer167;
	bSizer167 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapFilterDate = new wxStaticBitmap( m_panelFilterSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 34,34 ), 0 );
	bSizer167->Add( m_bitmapFilterDate, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	wxBoxSizer* bSizer165;
	bSizer165 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText79 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Time span:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText79->Wrap( -1 );
	bSizer165->Add( m_staticText79, 0, wxBOTTOM, 5 );
	
	m_spinCtrlTimespan = new wxSpinCtrl( m_panelFilterSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 2000000000, 0 );
	bSizer165->Add( m_spinCtrlTimespan, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	wxArrayString m_choiceUnitTimespanChoices;
	m_choiceUnitTimespan = new wxChoice( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceUnitTimespanChoices, 0 );
	m_choiceUnitTimespan->SetSelection( 0 );
	bSizer165->Add( m_choiceUnitTimespan, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer167->Add( bSizer165, 1, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer160->Add( bSizer167, 0, wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxALL, 5 );
	
	m_staticline23 = new wxStaticLine( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer160->Add( m_staticline23, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer168;
	bSizer168 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapFilterSize = new wxStaticBitmap( m_panelFilterSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 32,32 ), 0 );
	bSizer168->Add( m_bitmapFilterSize, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );
	
	wxBoxSizer* bSizer158;
	bSizer158 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText80 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("File size:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText80->Wrap( -1 );
	bSizer158->Add( m_staticText80, 0, wxBOTTOM, 5 );
	
	wxBoxSizer* bSizer162;
	bSizer162 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText101 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Minimum:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText101->Wrap( -1 );
	bSizer162->Add( m_staticText101, 0, wxBOTTOM, 2 );
	
	m_spinCtrlMinSize = new wxSpinCtrl( m_panelFilterSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 2000000000, 0 );
	bSizer162->Add( m_spinCtrlMinSize, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	wxArrayString m_choiceUnitMinSizeChoices;
	m_choiceUnitMinSize = new wxChoice( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceUnitMinSizeChoices, 0 );
	m_choiceUnitMinSize->SetSelection( 0 );
	bSizer162->Add( m_choiceUnitMinSize, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer158->Add( bSizer162, 0, wxBOTTOM|wxEXPAND, 5 );
	
	wxBoxSizer* bSizer163;
	bSizer163 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText102 = new wxStaticText( m_panelFilterSettings, wxID_ANY, _("Maximum:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText102->Wrap( -1 );
	bSizer163->Add( m_staticText102, 0, wxBOTTOM, 2 );
	
	m_spinCtrlMaxSize = new wxSpinCtrl( m_panelFilterSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 2000000000, 0 );
	bSizer163->Add( m_spinCtrlMaxSize, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	wxArrayString m_choiceUnitMaxSizeChoices;
	m_choiceUnitMaxSize = new wxChoice( m_panelFilterSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceUnitMaxSizeChoices, 0 );
	m_choiceUnitMaxSize->SetSelection( 0 );
	bSizer163->Add( m_choiceUnitMaxSize, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer158->Add( bSizer163, 0, wxEXPAND, 5 );
	
	
	bSizer168->Add( bSizer158, 1, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer160->Add( bSizer168, 1, wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxALL, 5 );
	
	
	bSizer1591->Add( bSizer160, 0, wxEXPAND, 5 );
	
	
	m_panelFilterSettings->SetSizer( bSizer1591 );
	m_panelFilterSettings->Layout();
	bSizer1591->Fit( m_panelFilterSettings );
	bSizer278->Add( m_panelFilterSettings, 1, wxEXPAND, 5 );
	
	m_staticline62 = new wxStaticLine( m_panelFilterSettingsHolder, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer278->Add( m_staticline62, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer280;
	bSizer280 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText44 = new wxStaticText( m_panelFilterSettingsHolder, wxID_ANY, _("Select filter rules to exclude certain files from synchronization. Enter file paths relative to their corresponding folder pair."), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticText44->Wrap( 590 );
	bSizer280->Add( m_staticText44, 0, wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer280->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticline46 = new wxStaticLine( m_panelFilterSettingsHolder, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer280->Add( m_staticline46, 0, wxEXPAND, 5 );
	
	m_buttonClear = new wxButton( m_panelFilterSettingsHolder, wxID_DEFAULT, _("C&lear"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer280->Add( m_buttonClear, 0, wxALL|wxALIGN_CENTER_VERTICAL, 10 );
	
	
	bSizer278->Add( bSizer280, 0, wxEXPAND, 5 );
	
	
	m_panelFilterSettingsHolder->SetSizer( bSizer278 );
	m_panelFilterSettingsHolder->Layout();
	bSizer278->Fit( m_panelFilterSettingsHolder );
	m_notebook->AddPage( m_panelFilterSettingsHolder, _("dummy"), false );
	m_panelSyncSettingsHolder = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelSyncSettingsHolder->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer276;
	bSizer276 = new wxBoxSizer( wxVERTICAL );
	
	bSizerLocalSyncSettings = new wxBoxSizer( wxVERTICAL );
	
	m_checkBoxUseLocalSyncOptions = new wxCheckBox( m_panelSyncSettingsHolder, wxID_ANY, _("Use local settings:"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizerLocalSyncSettings->Add( m_checkBoxUseLocalSyncOptions, 0, wxALL|wxEXPAND, 10 );
	
	m_staticline60 = new wxStaticLine( m_panelSyncSettingsHolder, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerLocalSyncSettings->Add( m_staticline60, 0, wxEXPAND, 5 );
	
	
	bSizer276->Add( bSizerLocalSyncSettings, 0, wxEXPAND, 5 );
	
	m_panelSyncSettings = new wxPanel( m_panelSyncSettingsHolder, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelSyncSettings->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer232;
	bSizer232 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer237;
	bSizer237 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer235;
	bSizer235 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText86 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Select a variant:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText86->Wrap( -1 );
	bSizer235->Add( m_staticText86, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer236;
	bSizer236 = new wxBoxSizer( wxVERTICAL );
	
	m_toggleBtnTwoWay = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,30 ), 0 );
	m_toggleBtnTwoWay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer236->Add( m_toggleBtnTwoWay, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxBOTTOM, 5 );
	
	m_toggleBtnMirror = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,30 ), 0 );
	m_toggleBtnMirror->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer236->Add( m_toggleBtnMirror, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxBOTTOM, 5 );
	
	m_toggleBtnUpdate = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,30 ), 0 );
	m_toggleBtnUpdate->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer236->Add( m_toggleBtnUpdate, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxBOTTOM, 5 );
	
	m_toggleBtnCustom = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,30 ), 0 );
	m_toggleBtnCustom->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer236->Add( m_toggleBtnCustom, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer235->Add( bSizer236, 0, wxRIGHT|wxLEFT, 5 );
	
	m_checkBoxDetectMove = new wxCheckBox( m_panelSyncSettings, wxID_ANY, _("Detect moved files"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxDetectMove->SetValue(true); 
	m_checkBoxDetectMove->SetToolTip( _("- Detection active after initial sync\n- Requires and creates database files\n- Not supported by all file systems") );
	
	bSizer235->Add( m_checkBoxDetectMove, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxALL, 5 );
	
	
	bSizer237->Add( bSizer235, 0, wxALL, 5 );
	
	m_staticline53 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer237->Add( m_staticline53, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer238;
	bSizer238 = new wxBoxSizer( wxVERTICAL );
	
	m_textCtrlSyncVarDescription = new wxTextCtrl( m_panelSyncSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY|wxNO_BORDER );
	bSizer238->Add( m_textCtrlSyncVarDescription, 1, wxEXPAND|wxLEFT, 5 );
	
	m_staticline43 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer238->Add( m_staticline43, 0, wxEXPAND, 5 );
	
	bSizerSyncConfig = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer173;
	bSizer173 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer173->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticText119 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Category"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText119->Wrap( -1 );
	bSizer173->Add( m_staticText119, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer173->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	bSizer173->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticText120 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Action"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText120->Wrap( -1 );
	bSizer173->Add( m_staticText120, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer173->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	bSizerSyncConfig->Add( bSizer173, 0, wxEXPAND|wxRIGHT, 5 );
	
	fgSizerSyncDirections = new wxFlexGridSizer( 2, 0, 5, 5 );
	fgSizerSyncDirections->SetFlexibleDirection( wxBOTH );
	fgSizerSyncDirections->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_bitmapLeftOnly = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 45,45 ), 0 );
	m_bitmapLeftOnly->SetToolTip( _("Item exists on left side only") );
	
	fgSizerSyncDirections->Add( m_bitmapLeftOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapLeftNewer = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 45,45 ), 0 );
	m_bitmapLeftNewer->SetToolTip( _("Left side is newer") );
	
	fgSizerSyncDirections->Add( m_bitmapLeftNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapDifferent = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 45,45 ), 0 );
	m_bitmapDifferent->SetToolTip( _("Items have different content") );
	
	fgSizerSyncDirections->Add( m_bitmapDifferent, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapConflict = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 45,45 ), 0 );
	m_bitmapConflict->SetToolTip( _("Conflict/item cannot be categorized") );
	
	fgSizerSyncDirections->Add( m_bitmapConflict, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapRightNewer = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 45,45 ), 0 );
	m_bitmapRightNewer->SetToolTip( _("Right side is newer") );
	
	fgSizerSyncDirections->Add( m_bitmapRightNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapRightOnly = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 45,45 ), 0 );
	m_bitmapRightOnly->SetToolTip( _("Item exists on right side only") );
	
	fgSizerSyncDirections->Add( m_bitmapRightOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonLeftOnly = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 46,46 ), wxBU_AUTODRAW );
	fgSizerSyncDirections->Add( m_bpButtonLeftOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonLeftNewer = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 46,46 ), wxBU_AUTODRAW );
	fgSizerSyncDirections->Add( m_bpButtonLeftNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonDifferent = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 46,46 ), wxBU_AUTODRAW );
	fgSizerSyncDirections->Add( m_bpButtonDifferent, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonConflict = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 46,46 ), wxBU_AUTODRAW );
	fgSizerSyncDirections->Add( m_bpButtonConflict, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonRightNewer = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 46,46 ), wxBU_AUTODRAW );
	fgSizerSyncDirections->Add( m_bpButtonRightNewer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonRightOnly = new wxBitmapButton( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 46,46 ), wxBU_AUTODRAW );
	fgSizerSyncDirections->Add( m_bpButtonRightOnly, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerSyncConfig->Add( fgSizerSyncDirections, 0, 0, 5 );
	
	m_bitmapDatabase = new wxStaticBitmap( m_panelSyncSettings, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmapDatabase->SetToolTip( _("Detect synchronization directions with the help of database files") );
	
	bSizerSyncConfig->Add( m_bitmapDatabase, 0, wxALIGN_CENTER_HORIZONTAL|wxLEFT|wxALIGN_CENTER_VERTICAL, 10 );
	
	wxBoxSizer* bSizerKeepVerticalHeightWhenSyncDirsNotShown;
	bSizerKeepVerticalHeightWhenSyncDirsNotShown = new wxBoxSizer( wxVERTICAL );
	
	
	bSizerKeepVerticalHeightWhenSyncDirsNotShown->Add( 0, 45, 0, 0, 5 );
	
	
	bSizerKeepVerticalHeightWhenSyncDirsNotShown->Add( 0, 5, 1, 0, 5 );
	
	
	bSizerKeepVerticalHeightWhenSyncDirsNotShown->Add( 0, 46, 0, 0, 5 );
	
	
	bSizerSyncConfig->Add( bSizerKeepVerticalHeightWhenSyncDirsNotShown, 0, 0, 5 );
	
	
	bSizer238->Add( bSizerSyncConfig, 0, wxALL, 10 );
	
	
	bSizer237->Add( bSizer238, 1, wxEXPAND, 5 );
	
	
	bSizer232->Add( bSizer237, 0, wxEXPAND, 5 );
	
	m_staticline54 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer232->Add( m_staticline54, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer184;
	bSizer184 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText87 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Delete files:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText87->Wrap( -1 );
	bSizer184->Add( m_staticText87, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM, 5 );
	
	wxBoxSizer* bSizer180;
	bSizer180 = new wxBoxSizer( wxHORIZONTAL );
	
	m_toggleBtnPermanent = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("&Permanent"), wxDefaultPosition, wxDefaultSize, 0 );
	m_toggleBtnPermanent->SetToolTip( _("Delete or overwrite files permanently") );
	
	bSizer180->Add( m_toggleBtnPermanent, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxRIGHT, 5 );
	
	m_toggleBtnRecycler = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("&Recycle bin"), wxDefaultPosition, wxDefaultSize, 0 );
	m_toggleBtnRecycler->SetToolTip( _("Back up deleted and overwritten files in the recycle bin") );
	
	bSizer180->Add( m_toggleBtnRecycler, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxRIGHT, 5 );
	
	m_toggleBtnVersioning = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("&Versioning"), wxDefaultPosition, wxDefaultSize, 0 );
	m_toggleBtnVersioning->SetToolTip( _("Move files to a user-defined folder") );
	
	bSizer180->Add( m_toggleBtnVersioning, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer184->Add( bSizer180, 0, 0, 5 );
	
	bSizerVersioning = new wxBoxSizer( wxHORIZONTAL );
	
	m_panelVersioning = new wxPanel( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelVersioning->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer191;
	bSizer191 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer156;
	bSizer156 = new wxBoxSizer( wxHORIZONTAL );
	
	m_versioningFolder = new FolderHistoryBox( m_panelVersioning, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer156->Add( m_versioningFolder, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectDirVersioning = new wxButton( m_panelVersioning, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectDirVersioning->SetToolTip( _("Select a folder") );
	
	bSizer156->Add( m_buttonSelectDirVersioning, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer191->Add( bSizer156, 0, wxEXPAND|wxBOTTOM, 5 );
	
	bSizer192 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText93 = new wxStaticText( m_panelVersioning, wxID_ANY, _("Naming convention:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText93->Wrap( -1 );
	bSizer192->Add( m_staticText93, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	wxArrayString m_choiceVersioningStyleChoices;
	m_choiceVersioningStyle = new wxChoice( m_panelVersioning, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_choiceVersioningStyleChoices, 0 );
	m_choiceVersioningStyle->SetSelection( 0 );
	bSizer192->Add( m_choiceVersioningStyle, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_staticTextNamingCvtPart1 = new wxStaticText( m_panelVersioning, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextNamingCvtPart1->Wrap( -1 );
	m_staticTextNamingCvtPart1->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer192->Add( m_staticTextNamingCvtPart1, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextNamingCvtPart2Bold = new wxStaticText( m_panelVersioning, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextNamingCvtPart2Bold->Wrap( -1 );
	m_staticTextNamingCvtPart2Bold->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	m_staticTextNamingCvtPart2Bold->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer192->Add( m_staticTextNamingCvtPart2Bold, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextNamingCvtPart3 = new wxStaticText( m_panelVersioning, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextNamingCvtPart3->Wrap( -1 );
	m_staticTextNamingCvtPart3->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer192->Add( m_staticTextNamingCvtPart3, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer192->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_hyperlink17 = new wxHyperlinkCtrl( m_panelVersioning, wxID_ANY, _("Show examples"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer192->Add( m_hyperlink17, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );
	
	
	bSizer191->Add( bSizer192, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	m_panelVersioning->SetSizer( bSizer191 );
	m_panelVersioning->Layout();
	bSizer191->Fit( m_panelVersioning );
	bSizerVersioning->Add( m_panelVersioning, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer184->Add( bSizerVersioning, 0, wxTOP|wxEXPAND, 5 );
	
	
	bSizer232->Add( bSizer184, 0, wxALL|wxEXPAND, 10 );
	
	bSizerMiscConfig = new wxBoxSizer( wxVERTICAL );
	
	m_staticline582 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerMiscConfig->Add( m_staticline582, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer1732;
	bSizer1732 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer174;
	bSizer174 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText88 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("Handle errors:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText88->Wrap( -1 );
	bSizer174->Add( m_staticText88, 0, wxBOTTOM, 5 );
	
	wxBoxSizer* bSizer175;
	bSizer175 = new wxBoxSizer( wxHORIZONTAL );
	
	m_toggleBtnErrorIgnore = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("&Ignore"), wxDefaultPosition, wxDefaultSize, 0 );
	m_toggleBtnErrorIgnore->SetToolTip( _("Hide all error and warning messages") );
	
	bSizer175->Add( m_toggleBtnErrorIgnore, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_toggleBtnErrorPopup = new wxToggleButton( m_panelSyncSettings, wxID_ANY, _("&Pop-up"), wxDefaultPosition, wxDefaultSize, 0 );
	m_toggleBtnErrorPopup->SetToolTip( _("Show pop-up on errors or warnings") );
	
	bSizer175->Add( m_toggleBtnErrorPopup, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer174->Add( bSizer175, 0, 0, 5 );
	
	
	bSizer1732->Add( bSizer174, 0, wxALL, 10 );
	
	m_staticline57 = new wxStaticLine( m_panelSyncSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer1732->Add( m_staticline57, 0, wxEXPAND, 5 );
	
	bSizerOnCompletion = new wxBoxSizer( wxVERTICAL );
	
	m_staticText89 = new wxStaticText( m_panelSyncSettings, wxID_ANY, _("On completion:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText89->Wrap( -1 );
	bSizerOnCompletion->Add( m_staticText89, 0, wxBOTTOM, 5 );
	
	m_comboBoxOnCompletion = new OnCompletionBox( m_panelSyncSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizerOnCompletion->Add( m_comboBoxOnCompletion, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );
	
	
	bSizer1732->Add( bSizerOnCompletion, 1, wxALL, 10 );
	
	
	bSizerMiscConfig->Add( bSizer1732, 1, wxEXPAND, 5 );
	
	
	bSizer232->Add( bSizerMiscConfig, 1, wxEXPAND, 5 );
	
	
	m_panelSyncSettings->SetSizer( bSizer232 );
	m_panelSyncSettings->Layout();
	bSizer232->Fit( m_panelSyncSettings );
	bSizer276->Add( m_panelSyncSettings, 1, wxEXPAND, 5 );
	
	
	m_panelSyncSettingsHolder->SetSizer( bSizer276 );
	m_panelSyncSettingsHolder->Layout();
	bSizer276->Fit( m_panelSyncSettingsHolder );
	m_notebook->AddPage( m_panelSyncSettingsHolder, _("dummy"), false );
	
	bSizer7->Add( m_notebook, 1, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOkay->SetDefault(); 
	m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer7->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer7 );
	this->Layout();
	bSizer7->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( ConfigDlgGenerated::OnClose ) );
	m_checkBoxUseLocalCmpOptions->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleLocalCompSettings ), NULL, this );
	m_toggleBtnTimeSize->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnTimeSizeDouble ), NULL, this );
	m_toggleBtnTimeSize->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnTimeSize ), NULL, this );
	m_toggleBtnContent->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnContentDouble ), NULL, this );
	m_toggleBtnContent->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnContent ), NULL, this );
	m_checkBoxTimeShift->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeCompOption ), NULL, this );
	m_hyperlink241->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( ConfigDlgGenerated::OnHelpTimeShift ), NULL, this );
	m_checkBoxSymlinksInclude->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeCompOption ), NULL, this );
	m_hyperlink24->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( ConfigDlgGenerated::OnHelpComparisonSettings ), NULL, this );
	m_textCtrlInclude->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeFilterOption ), NULL, this );
	m_hyperlink171->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( ConfigDlgGenerated::OnHelpShowExamples ), NULL, this );
	m_textCtrlExclude->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeFilterOption ), NULL, this );
	m_choiceUnitTimespan->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeFilterOption ), NULL, this );
	m_choiceUnitMinSize->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeFilterOption ), NULL, this );
	m_choiceUnitMaxSize->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeFilterOption ), NULL, this );
	m_buttonClear->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnFilterReset ), NULL, this );
	m_checkBoxUseLocalSyncOptions->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleLocalSyncSettings ), NULL, this );
	m_toggleBtnTwoWay->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnSyncTwoWayDouble ), NULL, this );
	m_toggleBtnTwoWay->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnSyncTwoWay ), NULL, this );
	m_toggleBtnMirror->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnSyncMirrorDouble ), NULL, this );
	m_toggleBtnMirror->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnSyncMirror ), NULL, this );
	m_toggleBtnUpdate->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnSyncUpdateDouble ), NULL, this );
	m_toggleBtnUpdate->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnSyncUpdate ), NULL, this );
	m_toggleBtnCustom->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( ConfigDlgGenerated::OnSyncCustomDouble ), NULL, this );
	m_toggleBtnCustom->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnSyncCustom ), NULL, this );
	m_checkBoxDetectMove->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnToggleDetectMovedFiles ), NULL, this );
	m_bpButtonLeftOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnExLeftSideOnly ), NULL, this );
	m_bpButtonLeftNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnLeftNewer ), NULL, this );
	m_bpButtonDifferent->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnDifferent ), NULL, this );
	m_bpButtonConflict->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnConflict ), NULL, this );
	m_bpButtonRightNewer->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnRightNewer ), NULL, this );
	m_bpButtonRightOnly->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnExRightSideOnly ), NULL, this );
	m_toggleBtnPermanent->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnDeletionPermanent ), NULL, this );
	m_toggleBtnRecycler->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnDeletionRecycler ), NULL, this );
	m_toggleBtnVersioning->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnDeletionVersioning ), NULL, this );
	m_choiceVersioningStyle->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( ConfigDlgGenerated::OnChangeSyncOption ), NULL, this );
	m_hyperlink17->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( ConfigDlgGenerated::OnHelpVersioning ), NULL, this );
	m_toggleBtnErrorIgnore->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnErrorIgnore ), NULL, this );
	m_toggleBtnErrorPopup->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnErrorPopup ), NULL, this );
	m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnOkay ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDlgGenerated::OnCancel ), NULL, this );
}

ConfigDlgGenerated::~ConfigDlgGenerated()
{
}

SyncConfirmationDlgGenerated::SyncConfirmationDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer134;
	bSizer134 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapSync = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer72->Add( m_bitmapSync, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticTextHeader = new wxStaticText( this, wxID_ANY, _("Start synchronization now?"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextHeader->Wrap( -1 );
	bSizer72->Add( m_staticTextHeader, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer134->Add( bSizer72, 0, 0, 5 );
	
	m_staticline371 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer134->Add( m_staticline371, 0, wxEXPAND, 5 );
	
	m_panelStatistics = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelStatistics->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer185;
	bSizer185 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer185->Add( 40, 0, 0, 0, 5 );
	
	
	bSizer185->Add( 0, 0, 1, 0, 5 );
	
	m_staticline38 = new wxStaticLine( m_panelStatistics, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer185->Add( m_staticline38, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer162;
	bSizer162 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer182;
	bSizer182 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText84 = new wxStaticText( m_panelStatistics, wxID_ANY, _("Variant:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText84->Wrap( -1 );
	bSizer182->Add( m_staticText84, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );
	
	
	bSizer182->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_staticTextVariant = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextVariant->Wrap( -1 );
	m_staticTextVariant->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer182->Add( m_staticTextVariant, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	bSizer182->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	bSizer162->Add( bSizer182, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxALL, 5 );
	
	m_staticline14 = new wxStaticLine( m_panelStatistics, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer162->Add( m_staticline14, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer181;
	bSizer181 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText83 = new wxStaticText( m_panelStatistics, wxID_ANY, _("Statistics:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText83->Wrap( -1 );
	bSizer181->Add( m_staticText83, 0, wxALL, 5 );
	
	wxFlexGridSizer* fgSizer11;
	fgSizer11 = new wxFlexGridSizer( 2, 7, 2, 5 );
	fgSizer11->SetFlexibleDirection( wxBOTH );
	fgSizer11->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_bitmapDeleteLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapDeleteLeft->SetToolTip( _("Number of files and folders that will be deleted") );
	
	fgSizer11->Add( m_bitmapDeleteLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapUpdateLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapUpdateLeft->SetToolTip( _("Number of files that will be updated") );
	
	fgSizer11->Add( m_bitmapUpdateLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bitmapCreateLeft = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapCreateLeft->SetToolTip( _("Number of files and folders that will be created") );
	
	fgSizer11->Add( m_bitmapCreateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bitmapData = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapData->SetToolTip( _("Total bytes to copy") );
	
	fgSizer11->Add( m_bitmapData, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bitmapCreateRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapCreateRight->SetToolTip( _("Number of files and folders that will be created") );
	
	fgSizer11->Add( m_bitmapCreateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bitmapUpdateRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapUpdateRight->SetToolTip( _("Number of files that will be updated") );
	
	fgSizer11->Add( m_bitmapUpdateRight, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bitmapDeleteRight = new wxStaticBitmap( m_panelStatistics, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	m_bitmapDeleteRight->SetToolTip( _("Number of files and folders that will be deleted") );
	
	fgSizer11->Add( m_bitmapDeleteRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextDeleteLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDeleteLeft->Wrap( -1 );
	m_staticTextDeleteLeft->SetToolTip( _("Number of files and folders that will be deleted") );
	
	fgSizer11->Add( m_staticTextDeleteLeft, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextUpdateLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextUpdateLeft->Wrap( -1 );
	m_staticTextUpdateLeft->SetToolTip( _("Number of files that will be updated") );
	
	fgSizer11->Add( m_staticTextUpdateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_staticTextCreateLeft = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCreateLeft->Wrap( -1 );
	m_staticTextCreateLeft->SetToolTip( _("Number of files and folders that will be created") );
	
	fgSizer11->Add( m_staticTextCreateLeft, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_staticTextData = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextData->Wrap( -1 );
	m_staticTextData->SetToolTip( _("Total bytes to copy") );
	
	fgSizer11->Add( m_staticTextData, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextCreateRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextCreateRight->Wrap( -1 );
	m_staticTextCreateRight->SetToolTip( _("Number of files and folders that will be created") );
	
	fgSizer11->Add( m_staticTextCreateRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextUpdateRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextUpdateRight->Wrap( -1 );
	m_staticTextUpdateRight->SetToolTip( _("Number of files that will be updated") );
	
	fgSizer11->Add( m_staticTextUpdateRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextDeleteRight = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDeleteRight->Wrap( -1 );
	m_staticTextDeleteRight->SetToolTip( _("Number of files and folders that will be deleted") );
	
	fgSizer11->Add( m_staticTextDeleteRight, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer181->Add( fgSizer11, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer162->Add( bSizer181, 0, wxEXPAND|wxALL, 5 );
	
	
	bSizer185->Add( bSizer162, 0, 0, 5 );
	
	m_staticline381 = new wxStaticLine( m_panelStatistics, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer185->Add( m_staticline381, 0, wxEXPAND, 5 );
	
	
	bSizer185->Add( 0, 0, 1, 0, 5 );
	
	
	bSizer185->Add( 40, 0, 0, 0, 5 );
	
	
	m_panelStatistics->SetSizer( bSizer185 );
	m_panelStatistics->Layout();
	bSizer185->Fit( m_panelStatistics );
	bSizer134->Add( m_panelStatistics, 0, wxEXPAND, 5 );
	
	m_staticline12 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer134->Add( m_staticline12, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer164;
	bSizer164 = new wxBoxSizer( wxVERTICAL );
	
	m_checkBoxDontShowAgain = new wxCheckBox( this, wxID_ANY, _("&Don't show this dialog again"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer164->Add( m_checkBoxDontShowAgain, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonStartSync = new wxButton( this, wxID_OK, _("&Start"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonStartSync->SetDefault(); 
	m_buttonStartSync->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonStartSync, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer164->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	bSizer134->Add( bSizer164, 1, wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer134 );
	this->Layout();
	bSizer134->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( SyncConfirmationDlgGenerated::OnClose ) );
	m_buttonStartSync->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SyncConfirmationDlgGenerated::OnStartSync ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SyncConfirmationDlgGenerated::OnCancel ), NULL, this );
}

SyncConfirmationDlgGenerated::~SyncConfirmationDlgGenerated()
{
}

FolderPairPanelGenerated::FolderPairPanelGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	wxBoxSizer* bSizer74;
	bSizer74 = new wxBoxSizer( wxHORIZONTAL );
	
	m_panelLeft = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelLeft->SetMinSize( wxSize( 1,-1 ) );
	
	wxBoxSizer* bSizer134;
	bSizer134 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonFolderPairOptions = new wxBitmapButton( m_panelLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	m_bpButtonFolderPairOptions->SetToolTip( _("Arrange folder pair") );
	
	bSizer134->Add( m_bpButtonFolderPairOptions, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonRemovePair = new wxBitmapButton( m_panelLeft, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	m_bpButtonRemovePair->SetToolTip( _("Remove folder pair") );
	
	bSizer134->Add( m_bpButtonRemovePair, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_directoryLeft = new FolderHistoryBox( m_panelLeft, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer134->Add( m_directoryLeft, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectDirLeft = new wxButton( m_panelLeft, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectDirLeft->SetToolTip( _("Select a folder") );
	
	bSizer134->Add( m_buttonSelectDirLeft, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	m_panelLeft->SetSizer( bSizer134 );
	m_panelLeft->Layout();
	bSizer134->Fit( m_panelLeft );
	bSizer74->Add( m_panelLeft, 0, wxALIGN_CENTER_VERTICAL|wxLEFT|wxEXPAND, 5 );
	
	m_panel20 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer95;
	bSizer95 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer95->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_bpButtonAltCompCfg = new wxBitmapButton( m_panel20, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	bSizer95->Add( m_bpButtonAltCompCfg, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_bpButtonLocalFilter = new wxBitmapButton( m_panel20, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	bSizer95->Add( m_bpButtonLocalFilter, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 2 );
	
	m_bpButtonAltSyncCfg = new wxBitmapButton( m_panel20, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	bSizer95->Add( m_bpButtonAltSyncCfg, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer95->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	m_panel20->SetSizer( bSizer95 );
	m_panel20->Layout();
	bSizer95->Fit( m_panel20 );
	bSizer74->Add( m_panel20, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT|wxEXPAND, 5 );
	
	m_panelRight = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelRight->SetMinSize( wxSize( 1,-1 ) );
	
	wxBoxSizer* bSizer135;
	bSizer135 = new wxBoxSizer( wxHORIZONTAL );
	
	m_directoryRight = new FolderHistoryBox( m_panelRight, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer135->Add( m_directoryRight, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectDirRight = new wxButton( m_panelRight, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectDirRight->SetToolTip( _("Select a folder") );
	
	bSizer135->Add( m_buttonSelectDirRight, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	m_panelRight->SetSizer( bSizer135 );
	m_panelRight->Layout();
	bSizer135->Fit( m_panelRight );
	bSizer74->Add( m_panelRight, 1, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer74 );
	this->Layout();
	bSizer74->Fit( this );
}

FolderPairPanelGenerated::~FolderPairPanelGenerated()
{
}

CompareProgressDlgGenerated::CompareProgressDlgGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer40;
	bSizer40 = new wxBoxSizer( wxHORIZONTAL );
	
	m_panelStatistics = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelStatistics->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer187;
	bSizer187 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer187->Add( 0, 5, 0, 0, 5 );
	
	wxFlexGridSizer* fgSizer7;
	fgSizer7 = new wxFlexGridSizer( 0, 2, 5, 5 );
	fgSizer7->SetFlexibleDirection( wxHORIZONTAL );
	fgSizer7->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_staticTextItemsFoundLabel = new wxStaticText( m_panelStatistics, wxID_ANY, _("Items found:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextItemsFoundLabel->Wrap( -1 );
	fgSizer7->Add( m_staticTextItemsFoundLabel, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextItemsFound = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextItemsFound->Wrap( -1 );
	m_staticTextItemsFound->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	fgSizer7->Add( m_staticTextItemsFound, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextItemsRemainingLabel = new wxStaticText( m_panelStatistics, wxID_ANY, _("Items remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextItemsRemainingLabel->Wrap( -1 );
	fgSizer7->Add( m_staticTextItemsRemainingLabel, 0, wxALIGN_BOTTOM, 5 );
	
	bSizerItemsRemaining = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextItemsRemaining = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextItemsRemaining->Wrap( -1 );
	m_staticTextItemsRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizerItemsRemaining->Add( m_staticTextItemsRemaining, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextDataRemaining = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDataRemaining->Wrap( -1 );
	bSizerItemsRemaining->Add( m_staticTextDataRemaining, 0, wxLEFT|wxALIGN_BOTTOM, 5 );
	
	
	fgSizer7->Add( bSizerItemsRemaining, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextTimeRemainingLabel = new wxStaticText( m_panelStatistics, wxID_ANY, _("Time remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTimeRemainingLabel->Wrap( -1 );
	fgSizer7->Add( m_staticTextTimeRemainingLabel, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextTimeRemaining = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTimeRemaining->Wrap( -1 );
	m_staticTextTimeRemaining->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	fgSizer7->Add( m_staticTextTimeRemaining, 0, wxALIGN_BOTTOM, 5 );
	
	wxStaticText* m_staticText37;
	m_staticText37 = new wxStaticText( m_panelStatistics, wxID_ANY, _("Time elapsed:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText37->Wrap( -1 );
	fgSizer7->Add( m_staticText37, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextTimeElapsed = new wxStaticText( m_panelStatistics, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTimeElapsed->Wrap( -1 );
	m_staticTextTimeElapsed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	fgSizer7->Add( m_staticTextTimeElapsed, 0, wxALIGN_BOTTOM, 5 );
	
	
	bSizer187->Add( fgSizer7, 0, wxRIGHT|wxLEFT, 5 );
	
	
	bSizer187->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelStatistics->SetSizer( bSizer187 );
	m_panelStatistics->Layout();
	bSizer187->Fit( m_panelStatistics );
	bSizer40->Add( m_panelStatistics, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	wxBoxSizer* bSizer181;
	bSizer181 = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextStatus = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatus->Wrap( -1 );
	bSizer181->Add( m_staticTextStatus, 0, 0, 5 );
	
	m_gauge2 = new wxGauge( this, wxID_ANY, 100, wxDefaultPosition, wxSize( -1,14 ), wxGA_HORIZONTAL|wxGA_SMOOTH );
	bSizer181->Add( m_gauge2, 0, wxEXPAND|wxTOP, 5 );
	
	m_staticTextSpeed = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextSpeed->Wrap( -1 );
	m_staticTextSpeed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer181->Add( m_staticTextSpeed, 0, wxALIGN_BOTTOM|wxTOP, 5 );
	
	
	bSizer40->Add( bSizer181, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	
	this->SetSizer( bSizer40 );
	this->Layout();
	bSizer40->Fit( this );
}

CompareProgressDlgGenerated::~CompareProgressDlgGenerated()
{
}

SyncProgressPanelGenerated::SyncProgressPanelGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	bSizerRoot = new wxBoxSizer( wxVERTICAL );
	
	bSizer42 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer42->Add( 32, 0, 0, 0, 5 );
	
	
	bSizer42->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_bitmapStatus = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 32,32 ), 0 );
	bSizer42->Add( m_bitmapStatus, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
	
	m_staticTextPhase = new wxStaticText( this, wxID_ANY, _("Synchronizing..."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextPhase->Wrap( -1 );
	m_staticTextPhase->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer42->Add( m_staticTextPhase, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );
	
	m_animCtrlSyncing = new wxAnimationCtrl( this, wxID_ANY, wxNullAnimation, wxDefaultPosition, wxSize( 32,32 ), wxAC_DEFAULT_STYLE ); 
	bSizer42->Add( m_animCtrlSyncing, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
	
	
	bSizer42->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_bpButtonMinimizeToTray = new wxBitmapButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 32,32 ), wxBU_AUTODRAW );
	m_bpButtonMinimizeToTray->SetToolTip( _("Minimize to notification area") );
	
	bSizer42->Add( m_bpButtonMinimizeToTray, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerRoot->Add( bSizer42, 0, wxALIGN_CENTER_HORIZONTAL|wxRIGHT|wxLEFT|wxEXPAND, 5 );
	
	bSizerStatusText = new wxBoxSizer( wxVERTICAL );
	
	m_staticTextStatus = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextStatus->Wrap( -1 );
	bSizerStatusText->Add( m_staticTextStatus, 0, wxEXPAND|wxLEFT, 10 );
	
	
	bSizerStatusText->Add( 0, 5, 0, 0, 5 );
	
	
	bSizerRoot->Add( bSizerStatusText, 0, wxEXPAND, 5 );
	
	wxStaticLine* m_staticlineHeader;
	m_staticlineHeader = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerRoot->Add( m_staticlineHeader, 0, wxEXPAND, 5 );
	
	m_panelProgress = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelProgress->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer173;
	bSizer173 = new wxBoxSizer( wxVERTICAL );
	
	bSizer171 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer171->Add( 10, 0, 0, 0, 5 );
	
	wxBoxSizer* bSizer164;
	bSizer164 = new wxBoxSizer( wxVERTICAL );
	
	m_panelItemsProcessed = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelItemsProcessed->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer165;
	bSizer165 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer165->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText96;
	m_staticText96 = new wxStaticText( m_panelItemsProcessed, wxID_ANY, _("Items processed:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText96->Wrap( -1 );
	bSizer165->Add( m_staticText96, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer169;
	bSizer169 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextProcessedObj = new wxStaticText( m_panelItemsProcessed, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticTextProcessedObj->Wrap( -1 );
	m_staticTextProcessedObj->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer169->Add( m_staticTextProcessedObj, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextDataProcessed = new wxStaticText( m_panelItemsProcessed, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDataProcessed->Wrap( -1 );
	bSizer169->Add( m_staticTextDataProcessed, 0, wxLEFT|wxALIGN_BOTTOM, 5 );
	
	
	bSizer165->Add( bSizer169, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer165->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelItemsProcessed->SetSizer( bSizer165 );
	m_panelItemsProcessed->Layout();
	bSizer165->Fit( m_panelItemsProcessed );
	bSizer164->Add( m_panelItemsProcessed, 0, wxEXPAND|wxTOP, 7 );
	
	m_panelItemsRemaining = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelItemsRemaining->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer166;
	bSizer166 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer166->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText97;
	m_staticText97 = new wxStaticText( m_panelItemsRemaining, wxID_ANY, _("Items remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText97->Wrap( -1 );
	bSizer166->Add( m_staticText97, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer170;
	bSizer170 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticTextRemainingObj = new wxStaticText( m_panelItemsRemaining, wxID_ANY, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticTextRemainingObj->Wrap( -1 );
	m_staticTextRemainingObj->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer170->Add( m_staticTextRemainingObj, 0, wxALIGN_BOTTOM, 5 );
	
	m_staticTextDataRemaining = new wxStaticText( m_panelItemsRemaining, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDataRemaining->Wrap( -1 );
	bSizer170->Add( m_staticTextDataRemaining, 0, wxLEFT|wxALIGN_BOTTOM, 5 );
	
	
	bSizer166->Add( bSizer170, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer166->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelItemsRemaining->SetSizer( bSizer166 );
	m_panelItemsRemaining->Layout();
	bSizer166->Fit( m_panelItemsRemaining );
	bSizer164->Add( m_panelItemsRemaining, 0, wxTOP|wxEXPAND, 7 );
	
	m_panelTimeRemaining = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelTimeRemaining->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer167;
	bSizer167 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer167->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText98;
	m_staticText98 = new wxStaticText( m_panelTimeRemaining, wxID_ANY, _("Time remaining:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText98->Wrap( -1 );
	bSizer167->Add( m_staticText98, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	m_staticTextRemTime = new wxStaticText( m_panelTimeRemaining, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextRemTime->Wrap( -1 );
	m_staticTextRemTime->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer167->Add( m_staticTextRemTime, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer167->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelTimeRemaining->SetSizer( bSizer167 );
	m_panelTimeRemaining->Layout();
	bSizer167->Fit( m_panelTimeRemaining );
	bSizer164->Add( m_panelTimeRemaining, 0, wxTOP|wxEXPAND, 7 );
	
	wxPanel* m_panelTimeElapsed;
	m_panelTimeElapsed = new wxPanel( m_panelProgress, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panelTimeElapsed->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer168;
	bSizer168 = new wxBoxSizer( wxVERTICAL );
	
	
	bSizer168->Add( 0, 5, 0, 0, 5 );
	
	wxStaticText* m_staticText961;
	m_staticText961 = new wxStaticText( m_panelTimeElapsed, wxID_ANY, _("Time elapsed:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText961->Wrap( -1 );
	bSizer168->Add( m_staticText961, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	m_staticTextTimeElapsed = new wxStaticText( m_panelTimeElapsed, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextTimeElapsed->Wrap( -1 );
	m_staticTextTimeElapsed->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer168->Add( m_staticTextTimeElapsed, 0, wxALIGN_BOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer168->Add( 0, 5, 0, 0, 5 );
	
	
	m_panelTimeElapsed->SetSizer( bSizer168 );
	m_panelTimeElapsed->Layout();
	bSizer168->Fit( m_panelTimeElapsed );
	bSizer164->Add( m_panelTimeElapsed, 0, wxTOP|wxEXPAND, 7 );
	
	
	bSizer171->Add( bSizer164, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer171->Add( 10, 0, 0, 0, 5 );
	
	wxBoxSizer* bSizer161;
	bSizer161 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer175;
	bSizer175 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapGraphKeyBytes = new wxStaticBitmap( m_panelProgress, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer175->Add( m_bitmapGraphKeyBytes, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );
	
	wxStaticText* m_staticText99;
	m_staticText99 = new wxStaticText( m_panelProgress, wxID_ANY, _("Bytes copied:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText99->Wrap( -1 );
	bSizer175->Add( m_staticText99, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer161->Add( bSizer175, 0, 0, 5 );
	
	m_panelGraphBytes = new zen::Graph2D( m_panelProgress, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_panelGraphBytes->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer161->Add( m_panelGraphBytes, 1, wxALIGN_CENTER_VERTICAL|wxEXPAND, 15 );
	
	wxBoxSizer* bSizer174;
	bSizer174 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapGraphKeyItems = new wxStaticBitmap( m_panelProgress, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer174->Add( m_bitmapGraphKeyItems, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM, 5 );
	
	wxStaticText* m_staticText100;
	m_staticText100 = new wxStaticText( m_panelProgress, wxID_ANY, _("Items processed:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText100->Wrap( -1 );
	bSizer174->Add( m_staticText100, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer161->Add( bSizer174, 0, 0, 5 );
	
	m_panelGraphItems = new zen::Graph2D( m_panelProgress, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_panelGraphItems->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer161->Add( m_panelGraphItems, 1, wxEXPAND, 15 );
	
	
	bSizer161->Add( 450, 0, 0, 0, 5 );
	
	
	bSizer171->Add( bSizer161, 1, wxEXPAND, 5 );
	
	
	bSizer171->Add( 0, 310, 0, 0, 5 );
	
	
	bSizer173->Add( bSizer171, 1, wxEXPAND, 5 );
	
	
	m_panelProgress->SetSizer( bSizer173 );
	m_panelProgress->Layout();
	bSizer173->Fit( m_panelProgress );
	bSizerRoot->Add( m_panelProgress, 1, wxEXPAND, 5 );
	
	m_notebookResult = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_FIXEDWIDTH );
	
	bSizerRoot->Add( m_notebookResult, 1, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 5 );
	
	m_staticlineFooter = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizerRoot->Add( m_staticlineFooter, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer160;
	bSizer160 = new wxBoxSizer( wxHORIZONTAL );
	
	bSizerOnCompletion = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText87 = new wxStaticText( this, wxID_ANY, _("On completion:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText87->Wrap( -1 );
	bSizerOnCompletion->Add( m_staticText87, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_comboBoxOnCompletion = new OnCompletionBox( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizerOnCompletion->Add( m_comboBoxOnCompletion, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer160->Add( bSizerOnCompletion, 1, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer160->Add( 0, 0, 0, 0, 5 );
	
	
	bSizerStdButtons->Add( bSizer160, 1, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );
	
	m_buttonClose = new wxButton( this, wxID_OK, _("Close"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonClose->SetDefault(); 
	m_buttonClose->Enable( false );
	
	bSizerStdButtons->Add( m_buttonClose, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonPause = new wxButton( this, wxID_ANY, _("&Pause"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonPause, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	m_buttonStop = new wxButton( this, wxID_CANCEL, _("Stop"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonStop, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizerRoot->Add( bSizerStdButtons, 0, wxALIGN_RIGHT|wxEXPAND, 5 );
	
	
	this->SetSizer( bSizerRoot );
	this->Layout();
	bSizerRoot->Fit( this );
}

SyncProgressPanelGenerated::~SyncProgressPanelGenerated()
{
}

LogPanelGenerated::LogPanelGenerated( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer179;
	bSizer179 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer153;
	bSizer153 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer154;
	bSizer154 = new wxBoxSizer( wxVERTICAL );
	
	m_bpButtonErrors = new ToggleButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 49,49 ), wxBU_AUTODRAW );
	bSizer154->Add( m_bpButtonErrors, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonWarnings = new ToggleButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 49,49 ), wxBU_AUTODRAW );
	bSizer154->Add( m_bpButtonWarnings, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_bpButtonInfo = new ToggleButton( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 49,49 ), wxBU_AUTODRAW );
	bSizer154->Add( m_bpButtonInfo, 0, wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer153->Add( bSizer154, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	m_staticline13 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer153->Add( m_staticline13, 0, wxEXPAND, 5 );
	
	m_gridMessages = new zen::Grid( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_gridMessages->SetScrollRate( 5, 5 );
	bSizer153->Add( m_gridMessages, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer179->Add( bSizer153, 1, wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer179 );
	this->Layout();
	bSizer179->Fit( this );
	
	// Connect Events
	m_bpButtonErrors->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( LogPanelGenerated::OnErrors ), NULL, this );
	m_bpButtonWarnings->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( LogPanelGenerated::OnWarnings ), NULL, this );
	m_bpButtonInfo->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( LogPanelGenerated::OnInfo ), NULL, this );
}

LogPanelGenerated::~LogPanelGenerated()
{
}

BatchDlgGenerated::BatchDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( -1,-1 ), wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer54;
	bSizer54 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapBatchJob = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer72->Add( m_bitmapBatchJob, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticTextDescr = new wxStaticText( this, wxID_ANY, _("Create a batch file for unattended synchronization. To start, double-click this file or schedule in a task planner: %x"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextDescr->Wrap( 520 );
	bSizer72->Add( m_staticTextDescr, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer54->Add( bSizer72, 0, 0, 5 );
	
	m_staticline18 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer54->Add( m_staticline18, 0, wxEXPAND, 5 );
	
	m_panel35 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel35->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer172;
	bSizer172 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer180;
	bSizer180 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer171;
	bSizer171 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText82 = new wxStaticText( m_panel35, wxID_ANY, _("Handle errors:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText82->Wrap( -1 );
	bSizer171->Add( m_staticText82, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer169;
	bSizer169 = new wxBoxSizer( wxHORIZONTAL );
	
	m_toggleBtnErrorIgnore = new wxToggleButton( m_panel35, wxID_ANY, _("&Ignore"), wxDefaultPosition, wxDefaultSize, 0 );
	m_toggleBtnErrorIgnore->SetToolTip( _("Hide all error and warning messages") );
	
	bSizer169->Add( m_toggleBtnErrorIgnore, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_toggleBtnErrorPopup = new wxToggleButton( m_panel35, wxID_ANY, _("&Pop-up"), wxDefaultPosition, wxDefaultSize, 0 );
	m_toggleBtnErrorPopup->SetToolTip( _("Show pop-up on errors or warnings") );
	
	bSizer169->Add( m_toggleBtnErrorPopup, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_toggleBtnErrorStop = new wxToggleButton( m_panel35, wxID_ANY, _("&Stop"), wxDefaultPosition, wxDefaultSize, 0 );
	m_toggleBtnErrorStop->SetToolTip( _("Stop synchronization at first error") );
	
	bSizer169->Add( m_toggleBtnErrorStop, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer171->Add( bSizer169, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer180->Add( bSizer171, 0, wxALL, 5 );
	
	m_staticline26 = new wxStaticLine( m_panel35, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer180->Add( m_staticline26, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer170;
	bSizer170 = new wxBoxSizer( wxVERTICAL );
	
	m_checkBoxRunMinimized = new wxCheckBox( m_panel35, wxID_ANY, _("Run minimized"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer170->Add( m_checkBoxRunMinimized, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxALL, 5 );
	
	wxBoxSizer* bSizer179;
	bSizer179 = new wxBoxSizer( wxHORIZONTAL );
	
	m_staticText81 = new wxStaticText( m_panel35, wxID_ANY, _("On completion:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText81->Wrap( -1 );
	bSizer179->Add( m_staticText81, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_comboBoxOnCompletion = new OnCompletionBox( m_panel35, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer179->Add( m_comboBoxOnCompletion, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer170->Add( bSizer179, 0, wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer180->Add( bSizer170, 1, wxALL, 5 );
	
	
	bSizer172->Add( bSizer180, 0, wxEXPAND, 5 );
	
	m_staticline25 = new wxStaticLine( m_panel35, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer172->Add( m_staticline25, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer191;
	bSizer191 = new wxBoxSizer( wxVERTICAL );
	
	m_checkBoxGenerateLogfile = new wxCheckBox( m_panel35, wxID_ANY, _("Save log:"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer191->Add( m_checkBoxGenerateLogfile, 0, wxEXPAND|wxALL, 5 );
	
	m_panelLogfile = new wxPanel( m_panel35, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelLogfile->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer1721;
	bSizer1721 = new wxBoxSizer( wxHORIZONTAL );
	
	m_logfileDir = new FolderHistoryBox( m_panelLogfile, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	bSizer1721->Add( m_logfileDir, 1, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_buttonSelectLogfileDir = new wxButton( m_panelLogfile, wxID_ANY, _("Browse"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonSelectLogfileDir->SetToolTip( _("Select a folder") );
	
	bSizer1721->Add( m_buttonSelectLogfileDir, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_checkBoxLogfilesLimit = new wxCheckBox( m_panelLogfile, wxID_ANY, _("Limit:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxLogfilesLimit->SetToolTip( _("Limit maximum number of log files") );
	
	bSizer1721->Add( m_checkBoxLogfilesLimit, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_spinCtrlLogfileLimit = new wxSpinCtrl( m_panelLogfile, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 70,-1 ), wxSP_ARROW_KEYS, 1, 2000000000, 1 );
	m_spinCtrlLogfileLimit->SetToolTip( _("Limit maximum number of log files") );
	
	bSizer1721->Add( m_spinCtrlLogfileLimit, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	m_panelLogfile->SetSizer( bSizer1721 );
	m_panelLogfile->Layout();
	bSizer1721->Fit( m_panelLogfile );
	bSizer191->Add( m_panelLogfile, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer172->Add( bSizer191, 0, wxEXPAND|wxALL, 5 );
	
	m_hyperlink17 = new wxHyperlinkCtrl( m_panel35, wxID_ANY, _("How can I schedule a batch job?"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer172->Add( m_hyperlink17, 0, wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	m_panel35->SetSizer( bSizer172 );
	m_panel35->Layout();
	bSizer172->Fit( m_panel35 );
	bSizer54->Add( m_panel35, 1, wxALIGN_CENTER_HORIZONTAL|wxEXPAND, 5 );
	
	m_staticline13 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer54->Add( m_staticline13, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizerStdButtons->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_buttonSaveAs = new wxButton( this, wxID_SAVE, _("Save &as..."), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonSaveAs->SetDefault(); 
	m_buttonSaveAs->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonSaveAs, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer54->Add( bSizerStdButtons, 0, wxALIGN_RIGHT|wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer54 );
	this->Layout();
	bSizer54->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( BatchDlgGenerated::OnClose ) );
	m_toggleBtnErrorIgnore->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnErrorIgnore ), NULL, this );
	m_toggleBtnErrorPopup->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnErrorPopup ), NULL, this );
	m_toggleBtnErrorStop->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnErrorStop ), NULL, this );
	m_checkBoxGenerateLogfile->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnToggleGenerateLogfile ), NULL, this );
	m_checkBoxLogfilesLimit->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnToggleLogfilesLimit ), NULL, this );
	m_hyperlink17->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( BatchDlgGenerated::OnHelpScheduleBatch ), NULL, this );
	m_buttonSaveAs->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnSaveBatchJob ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( BatchDlgGenerated::OnCancel ), NULL, this );
}

BatchDlgGenerated::~BatchDlgGenerated()
{
}

DeleteDlgGenerated::DeleteDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( -1,-1 ), wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer24;
	bSizer24 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapDeleteType = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer72->Add( m_bitmapDeleteType, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticTextHeader = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0|wxNO_BORDER );
	m_staticTextHeader->Wrap( -1 );
	bSizer72->Add( m_staticTextHeader, 0, wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer24->Add( bSizer72, 0, 0, 5 );
	
	m_staticline91 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer24->Add( m_staticline91, 0, wxEXPAND, 5 );
	
	m_panel31 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel31->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer185;
	bSizer185 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer185->Add( 60, 0, 0, 0, 5 );
	
	m_staticline42 = new wxStaticLine( m_panel31, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer185->Add( m_staticline42, 0, wxEXPAND, 5 );
	
	m_textCtrlFileList = new wxTextCtrl( m_panel31, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 500,200 ), wxTE_DONTWRAP|wxTE_MULTILINE|wxTE_READONLY|wxNO_BORDER );
	bSizer185->Add( m_textCtrlFileList, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	m_panel31->SetSizer( bSizer185 );
	m_panel31->Layout();
	bSizer185->Fit( m_panel31 );
	bSizer24->Add( m_panel31, 1, wxEXPAND|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_staticline9 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer24->Add( m_staticline9, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxUseRecycler = new wxCheckBox( this, wxID_ANY, _("&Recycle bin"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizerStdButtons->Add( m_checkBoxUseRecycler, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonOK = new wxButton( this, wxID_OK, _("dummy"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOK->SetDefault(); 
	m_buttonOK->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOK, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer24->Add( bSizerStdButtons, 0, wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer24 );
	this->Layout();
	bSizer24->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( DeleteDlgGenerated::OnClose ) );
	m_checkBoxUseRecycler->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( DeleteDlgGenerated::OnUseRecycler ), NULL, this );
	m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DeleteDlgGenerated::OnOK ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DeleteDlgGenerated::OnCancel ), NULL, this );
}

DeleteDlgGenerated::~DeleteDlgGenerated()
{
}

OptionsDlgGenerated::OptionsDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( -1,-1 ), wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer95;
	bSizer95 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer72;
	bSizer72 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapSettings = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer72->Add( m_bitmapSettings, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_staticText44 = new wxStaticText( this, wxID_ANY, _("The following settings are used for all synchronization jobs."), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticText44->Wrap( 500 );
	bSizer72->Add( m_staticText44, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 10 );
	
	
	bSizer95->Add( bSizer72, 0, 0, 5 );
	
	m_staticline20 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer95->Add( m_staticline20, 0, wxEXPAND, 5 );
	
	m_panel39 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel39->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer166;
	bSizer166 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer186;
	bSizer186 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer160;
	bSizer160 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer176;
	bSizer176 = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxFailSafe = new wxCheckBox( m_panel39, wxID_ANY, _("Fail-safe file copy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxFailSafe->SetValue(true); 
	m_checkBoxFailSafe->SetToolTip( _("Copy to a temporary file (*.ffs_tmp) before overwriting target.\nThis guarantees a consistent state even in case of a serious error.") );
	
	bSizer176->Add( m_checkBoxFailSafe, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText91 = new wxStaticText( m_panel39, wxID_ANY, _("(recommended)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText91->Wrap( -1 );
	m_staticText91->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer176->Add( m_staticText91, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer160->Add( bSizer176, 0, wxEXPAND, 5 );
	
	bSizerLockedFiles = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxCopyLocked = new wxCheckBox( m_panel39, wxID_ANY, _("Copy locked files"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxCopyLocked->SetValue(true); 
	m_checkBoxCopyLocked->SetToolTip( _("Copy shared or locked files using the Volume Shadow Copy Service.") );
	
	bSizerLockedFiles->Add( m_checkBoxCopyLocked, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticText92 = new wxStaticText( m_panel39, wxID_ANY, _("(requires administrator rights)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText92->Wrap( -1 );
	m_staticText92->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizerLockedFiles->Add( m_staticText92, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer160->Add( bSizerLockedFiles, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer178;
	bSizer178 = new wxBoxSizer( wxHORIZONTAL );
	
	m_checkBoxCopyPermissions = new wxCheckBox( m_panel39, wxID_ANY, _("Copy file access permissions"), wxDefaultPosition, wxDefaultSize, 0 );
	m_checkBoxCopyPermissions->SetValue(true); 
	m_checkBoxCopyPermissions->SetToolTip( _("Transfer file and folder permissions.") );
	
	bSizer178->Add( m_checkBoxCopyPermissions, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_staticText93 = new wxStaticText( m_panel39, wxID_ANY, _("(requires administrator rights)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText93->Wrap( -1 );
	m_staticText93->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) );
	
	bSizer178->Add( m_staticText93, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer160->Add( bSizer178, 0, wxEXPAND, 5 );
	
	
	bSizer186->Add( bSizer160, 0, wxEXPAND|wxALL, 5 );
	
	m_staticline39 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer186->Add( m_staticline39, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer188;
	bSizer188 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText95 = new wxStaticText( m_panel39, wxID_ANY, _("Automatic retry on error:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText95->Wrap( -1 );
	bSizer188->Add( m_staticText95, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM, 5 );
	
	wxFlexGridSizer* fgSizer6;
	fgSizer6 = new wxFlexGridSizer( 0, 2, 5, 5 );
	fgSizer6->SetFlexibleDirection( wxBOTH );
	fgSizer6->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	m_staticText96 = new wxStaticText( m_panel39, wxID_ANY, _("Retry count:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText96->Wrap( -1 );
	fgSizer6->Add( m_staticText96, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_spinCtrlAutoRetryCount = new wxSpinCtrl( m_panel39, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 60,-1 ), wxSP_ARROW_KEYS, 0, 2000000000, 4 );
	fgSizer6->Add( m_spinCtrlAutoRetryCount, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextAutoRetryDelay = new wxStaticText( m_panel39, wxID_ANY, _("Delay (in seconds):"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextAutoRetryDelay->Wrap( -1 );
	fgSizer6->Add( m_staticTextAutoRetryDelay, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_spinCtrlAutoRetryDelay = new wxSpinCtrl( m_panel39, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 60,-1 ), wxSP_ARROW_KEYS, 0, 2000000000, 0 );
	fgSizer6->Add( m_spinCtrlAutoRetryDelay, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer188->Add( fgSizer6, 0, wxLEFT, 10 );
	
	
	bSizer186->Add( bSizer188, 0, wxALL, 10 );
	
	
	bSizer166->Add( bSizer186, 0, wxEXPAND, 5 );
	
	m_staticline191 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer166->Add( m_staticline191, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer181;
	bSizer181 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText85 = new wxStaticText( m_panel39, wxID_ANY, _("Customize context menu:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText85->Wrap( -1 );
	bSizer181->Add( m_staticText85, 0, wxALIGN_CENTER_VERTICAL|wxBOTTOM, 5 );
	
	m_gridCustomCommand = new wxGrid( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	
	// Grid
	m_gridCustomCommand->CreateGrid( 5, 2 );
	m_gridCustomCommand->EnableEditing( true );
	m_gridCustomCommand->EnableGridLines( true );
	m_gridCustomCommand->EnableDragGridSize( false );
	m_gridCustomCommand->SetMargins( 0, 0 );
	
	// Columns
	m_gridCustomCommand->SetColSize( 0, 165 );
	m_gridCustomCommand->SetColSize( 1, 196 );
	m_gridCustomCommand->EnableDragColMove( false );
	m_gridCustomCommand->EnableDragColSize( true );
	m_gridCustomCommand->SetColLabelSize( 20 );
	m_gridCustomCommand->SetColLabelValue( 0, _("Description") );
	m_gridCustomCommand->SetColLabelValue( 1, _("Command line") );
	m_gridCustomCommand->SetColLabelAlignment( wxALIGN_CENTRE, wxALIGN_CENTRE );
	
	// Rows
	m_gridCustomCommand->EnableDragRowSize( false );
	m_gridCustomCommand->SetRowLabelSize( 1 );
	m_gridCustomCommand->SetRowLabelAlignment( wxALIGN_LEFT, wxALIGN_CENTRE );
	
	// Label Appearance
	
	// Cell Defaults
	m_gridCustomCommand->SetDefaultCellAlignment( wxALIGN_LEFT, wxALIGN_TOP );
	bSizer181->Add( m_gridCustomCommand, 1, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxEXPAND, 5 );
	
	wxBoxSizer* bSizer193;
	bSizer193 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bpButtonAddRow = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	bSizer193->Add( m_bpButtonAddRow, 0, 0, 5 );
	
	m_bpButtonRemoveRow = new wxBitmapButton( m_panel39, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( 25,25 ), wxBU_AUTODRAW );
	bSizer193->Add( m_bpButtonRemoveRow, 0, 0, 5 );
	
	
	bSizer193->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_hyperlink17 = new wxHyperlinkCtrl( m_panel39, wxID_ANY, _("Show examples"), wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer193->Add( m_hyperlink17, 0, wxLEFT, 5 );
	
	
	bSizer181->Add( bSizer193, 0, wxTOP|wxEXPAND, 5 );
	
	
	bSizer166->Add( bSizer181, 1, wxEXPAND|wxALL, 10 );
	
	m_staticline192 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer166->Add( m_staticline192, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer1881;
	bSizer1881 = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonResetDialogs = new zen::BitmapTextButton( m_panel39, wxID_ANY, _("Show hidden dialogs again"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonResetDialogs->SetToolTip( _("Show all permanently hidden dialogs and warning messages again") );
	
	bSizer1881->Add( m_buttonResetDialogs, 0, wxALL|wxALIGN_CENTER_VERTICAL, 10 );
	
	m_staticline40 = new wxStaticLine( m_panel39, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer1881->Add( m_staticline40, 0, wxEXPAND, 5 );
	
	
	bSizer166->Add( bSizer1881, 0, 0, 5 );
	
	
	m_panel39->SetSizer( bSizer166 );
	m_panel39->Layout();
	bSizer166->Fit( m_panel39 );
	bSizer95->Add( m_panel39, 1, wxALIGN_CENTER_HORIZONTAL|wxEXPAND, 5 );
	
	m_staticline36 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer95->Add( m_staticline36, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonDefault = new wxButton( this, wxID_DEFAULT, _("&Default"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonDefault, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizerStdButtons->Add( 0, 0, 1, 0, 5 );
	
	m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOkay->SetDefault(); 
	m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer95->Add( bSizerStdButtons, 0, wxEXPAND, 5 );
	
	
	this->SetSizer( bSizer95 );
	this->Layout();
	bSizer95->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( OptionsDlgGenerated::OnClose ) );
	m_spinCtrlAutoRetryCount->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( OptionsDlgGenerated::OnToggleAutoRetryCount ), NULL, this );
	m_bpButtonAddRow->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnAddRow ), NULL, this );
	m_bpButtonRemoveRow->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnRemoveRow ), NULL, this );
	m_hyperlink17->Connect( wxEVT_COMMAND_HYPERLINK, wxHyperlinkEventHandler( OptionsDlgGenerated::OnHelpShowExamples ), NULL, this );
	m_buttonResetDialogs->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnResetDialogs ), NULL, this );
	m_buttonDefault->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnDefault ), NULL, this );
	m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnOkay ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( OptionsDlgGenerated::OnCancel ), NULL, this );
}

OptionsDlgGenerated::~OptionsDlgGenerated()
{
}

TooltipDialogGenerated::TooltipDialogGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* bSizer158;
	bSizer158 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmapLeft = new wxStaticBitmap( this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer158->Add( m_bitmapLeft, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticTextMain = new wxStaticText( this, wxID_ANY, _("dummy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextMain->Wrap( 600 );
	bSizer158->Add( m_staticTextMain, 0, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	this->SetSizer( bSizer158 );
	this->Layout();
	bSizer158->Fit( this );
}

TooltipDialogGenerated::~TooltipDialogGenerated()
{
}

SelectTimespanDlgGenerated::SelectTimespanDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer96;
	bSizer96 = new wxBoxSizer( wxVERTICAL );
	
	m_panel35 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel35->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer98;
	bSizer98 = new wxBoxSizer( wxHORIZONTAL );
	
	m_calendarFrom = new wxCalendarCtrl( m_panel35, wxID_ANY, wxDefaultDateTime, wxDefaultPosition, wxDefaultSize, wxCAL_SHOW_HOLIDAYS|wxNO_BORDER );
	bSizer98->Add( m_calendarFrom, 0, wxTOP|wxBOTTOM|wxLEFT, 10 );
	
	m_calendarTo = new wxCalendarCtrl( m_panel35, wxID_ANY, wxDefaultDateTime, wxDefaultPosition, wxDefaultSize, wxCAL_SHOW_HOLIDAYS|wxNO_BORDER );
	bSizer98->Add( m_calendarTo, 0, wxALL, 10 );
	
	
	m_panel35->SetSizer( bSizer98 );
	m_panel35->Layout();
	bSizer98->Fit( m_panel35 );
	bSizer96->Add( m_panel35, 0, wxEXPAND|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_staticline21 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer96->Add( m_staticline21, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonOkay = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonOkay->SetDefault(); 
	m_buttonOkay->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizerStdButtons->Add( m_buttonOkay, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	
	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizerStdButtons->Add( m_buttonCancel, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxRIGHT, 5 );
	
	
	bSizer96->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer96 );
	this->Layout();
	bSizer96->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( SelectTimespanDlgGenerated::OnClose ) );
	m_calendarFrom->Connect( wxEVT_CALENDAR_SEL_CHANGED, wxCalendarEventHandler( SelectTimespanDlgGenerated::OnChangeSelectionFrom ), NULL, this );
	m_calendarTo->Connect( wxEVT_CALENDAR_SEL_CHANGED, wxCalendarEventHandler( SelectTimespanDlgGenerated::OnChangeSelectionTo ), NULL, this );
	m_buttonOkay->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SelectTimespanDlgGenerated::OnOkay ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( SelectTimespanDlgGenerated::OnCancel ), NULL, this );
}

SelectTimespanDlgGenerated::~SelectTimespanDlgGenerated()
{
}

AboutDlgGenerated::AboutDlgGenerated( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	
	wxBoxSizer* bSizer31;
	bSizer31 = new wxBoxSizer( wxVERTICAL );
	
	m_panel41 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel41->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	wxBoxSizer* bSizer162;
	bSizer162 = new wxBoxSizer( wxVERTICAL );
	
	m_bitmapLogo = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer162->Add( m_bitmapLogo, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	m_staticline341 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer162->Add( m_staticline341, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer174;
	bSizer174 = new wxBoxSizer( wxHORIZONTAL );
	
	wxBoxSizer* bSizer181;
	bSizer181 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSizer187;
	bSizer187 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText96 = new wxStaticText( m_panel41, wxID_ANY, _("Source code written in C++ using:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText96->Wrap( -1 );
	bSizer187->Add( m_staticText96, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer171;
	bSizer171 = new wxBoxSizer( wxHORIZONTAL );
	
	m_hyperlink11 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("MS Visual C++"), wxT("http://www.visualstudio.com"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink11->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink11->SetToolTip( _("http://www.visualstudio.com") );
	
	bSizer171->Add( m_hyperlink11, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink9 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("MinGW"), wxT("http://www.mingw.org"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink9->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink9->SetToolTip( _("http://www.mingw.org") );
	
	bSizer171->Add( m_hyperlink9, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink10 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("Code::Blocks"), wxT("http://www.codeblocks.org"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink10->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink10->SetToolTip( _("http://www.codeblocks.org") );
	
	bSizer171->Add( m_hyperlink10, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink7 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("wxWidgets"), wxT("http://www.wxwidgets.org"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink7->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink7->SetToolTip( _("http://www.wxwidgets.org") );
	
	bSizer171->Add( m_hyperlink7, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink14 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("wxFormBuilder"), wxT("http://wxformbuilder.org"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink14->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink14->SetToolTip( _("http://wxformbuilder.org") );
	
	bSizer171->Add( m_hyperlink14, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer187->Add( bSizer171, 0, wxALIGN_CENTER_HORIZONTAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer172;
	bSizer172 = new wxBoxSizer( wxHORIZONTAL );
	
	m_hyperlink15 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("zen::Xml"), wxT("http://zenxml.sourceforge.net"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink15->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink15->SetToolTip( _("http://zenxml.sourceforge.net") );
	
	bSizer172->Add( m_hyperlink15, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink13 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("Boost"), wxT("http://www.boost.org"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink13->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink13->SetToolTip( _("http://www.boost.org") );
	
	bSizer172->Add( m_hyperlink13, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink16 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("Artistic Style"), wxT("http://astyle.sourceforge.net"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink16->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink16->SetToolTip( _("http://astyle.sourceforge.net") );
	
	bSizer172->Add( m_hyperlink16, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink12 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("Google Test"), wxT("http://code.google.com/p/googletest"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink12->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink12->SetToolTip( _("http://code.google.com/p/googletest") );
	
	bSizer172->Add( m_hyperlink12, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink18 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("Unicode NSIS"), wxT("http://www.scratchpaper.com"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink18->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_hyperlink18->SetToolTip( _("http://www.scratchpaper.com") );
	
	bSizer172->Add( m_hyperlink18, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer187->Add( bSizer172, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer181->Add( bSizer187, 0, wxALL|wxEXPAND, 5 );
	
	m_panelDonate = new wxPanel( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelDonate->SetBackgroundColour( wxColour( 153, 170, 187 ) );
	
	wxBoxSizer* bSizer183;
	bSizer183 = new wxBoxSizer( wxVERTICAL );
	
	m_panel39 = new wxPanel( m_panelDonate, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel39->SetBackgroundColour( wxColour( 221, 221, 255 ) );
	
	wxBoxSizer* bSizer184;
	bSizer184 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer184->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_animCtrlWink = new wxAnimationCtrl( m_panel39, wxID_ANY, wxNullAnimation, wxDefaultPosition, wxSize( 48,48 ), wxAC_DEFAULT_STYLE ); 
	bSizer184->Add( m_animCtrlWink, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	wxBoxSizer* bSizer178;
	bSizer178 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText83 = new wxStaticText( m_panel39, wxID_ANY, _("If you like FreeFileSync"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText83->Wrap( -1 );
	m_staticText83->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	m_staticText83->SetForegroundColour( wxColour( 0, 0, 0 ) );
	
	bSizer178->Add( m_staticText83, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	m_buttonDonate = new wxButton( m_panel39, wxID_ANY, _("Donate with PayPal"), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttonDonate->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 90, false, wxEmptyString ) );
	m_buttonDonate->SetToolTip( _("http://www.freefilesync.org/donate.php") );
	
	bSizer178->Add( m_buttonDonate, 0, wxBOTTOM|wxRIGHT|wxLEFT|wxALIGN_CENTER_HORIZONTAL, 5 );
	
	
	bSizer184->Add( bSizer178, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer184->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	m_panel39->SetSizer( bSizer184 );
	m_panel39->Layout();
	bSizer184->Fit( m_panel39 );
	bSizer183->Add( m_panel39, 0, wxEXPAND|wxALL, 5 );
	
	
	m_panelDonate->SetSizer( bSizer183 );
	m_panelDonate->Layout();
	bSizer183->Fit( m_panelDonate );
	bSizer181->Add( m_panelDonate, 0, wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxRIGHT|wxLEFT, 5 );
	
	wxBoxSizer* bSizer186;
	bSizer186 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText94 = new wxStaticText( m_panel41, wxID_ANY, _("Feedback and suggestions are welcome"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText94->Wrap( -1 );
	bSizer186->Add( m_staticText94, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer166;
	bSizer166 = new wxBoxSizer( wxHORIZONTAL );
	
	
	bSizer166->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_bitmap9 = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmap9->SetToolTip( _("Homepage") );
	
	bSizer166->Add( m_bitmap9, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	m_hyperlink1 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("MinFFS @GitHub"), wxT("https://github.com/abcdec/MinFFS"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink1->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, true, wxEmptyString ) );
	m_hyperlink1->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer166->Add( m_hyperlink1, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer166->Add( 0, 0, 1, wxEXPAND, 5 );
	
	m_bitmap10 = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_bitmap10->SetToolTip( _("Email") );
	
	bSizer166->Add( m_bitmap10, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );
	
	m_hyperlink2 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("Submit issues @GitHub"), wxT("https://github.com/abcdec/MinFFS/issues"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink2->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, true, wxEmptyString ) );
	m_hyperlink2->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer166->Add( m_hyperlink2, 0, wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer166->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	bSizer186->Add( bSizer166, 0, wxALIGN_CENTER_HORIZONTAL|wxEXPAND|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer181->Add( bSizer186, 0, wxALL|wxEXPAND, 5 );
	
	m_staticline34 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer181->Add( m_staticline34, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer185;
	bSizer185 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText93 = new wxStaticText( m_panel41, wxID_ANY, _("Published under the GNU General Public License"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText93->Wrap( -1 );
	bSizer185->Add( m_staticText93, 0, wxALL, 5 );
	
	wxBoxSizer* bSizer1671;
	bSizer1671 = new wxBoxSizer( wxHORIZONTAL );
	
	m_bitmap13 = new wxStaticBitmap( m_panel41, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer1671->Add( m_bitmap13, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 5 );
	
	m_hyperlink5 = new wxHyperlinkCtrl( m_panel41, wxID_ANY, _("http://www.gnu.org/licenses/gpl-3.0"), wxT("http://www.gnu.org/licenses/gpl-3.0"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	m_hyperlink5->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	
	bSizer1671->Add( m_hyperlink5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer185->Add( bSizer1671, 0, wxALIGN_CENTER_HORIZONTAL|wxBOTTOM|wxRIGHT|wxLEFT, 5 );
	
	
	bSizer181->Add( bSizer185, 0, wxALL|wxEXPAND, 5 );
	
	
	bSizer174->Add( bSizer181, 0, 0, 5 );
	
	m_staticline37 = new wxStaticLine( m_panel41, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL );
	bSizer174->Add( m_staticline37, 0, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer177;
	bSizer177 = new wxBoxSizer( wxVERTICAL );
	
	m_staticText54 = new wxStaticText( m_panel41, wxID_ANY, _("Many thanks for localization:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText54->Wrap( 200 );
	m_staticText54->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	
	bSizer177->Add( m_staticText54, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );
	
	
	bSizer177->Add( 0, 5, 0, 0, 5 );
	
	m_scrolledWindowTranslators = new wxScrolledWindow( m_panel41, wxID_ANY, wxDefaultPosition, wxSize( -1,-1 ), wxVSCROLL );
	m_scrolledWindowTranslators->SetScrollRate( 10, 10 );
	m_scrolledWindowTranslators->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_scrolledWindowTranslators->SetMinSize( wxSize( 220,-1 ) );
	
	fgSizerTranslators = new wxFlexGridSizer( 0, 2, 2, 10 );
	fgSizerTranslators->SetFlexibleDirection( wxBOTH );
	fgSizerTranslators->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	
	m_scrolledWindowTranslators->SetSizer( fgSizerTranslators );
	m_scrolledWindowTranslators->Layout();
	fgSizerTranslators->Fit( m_scrolledWindowTranslators );
	bSizer177->Add( m_scrolledWindowTranslators, 1, wxALIGN_CENTER_HORIZONTAL|wxLEFT|wxEXPAND, 5 );
	
	
	bSizer174->Add( bSizer177, 0, wxEXPAND|wxTOP|wxLEFT, 5 );
	
	
	bSizer162->Add( bSizer174, 0, 0, 5 );
	
	
	m_panel41->SetSizer( bSizer162 );
	m_panel41->Layout();
	bSizer162->Fit( m_panel41 );
	bSizer31->Add( m_panel41, 0, wxALIGN_CENTER_HORIZONTAL|wxEXPAND, 5 );
	
	m_staticline36 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	bSizer31->Add( m_staticline36, 0, wxEXPAND, 5 );
	
	bSizerStdButtons = new wxBoxSizer( wxHORIZONTAL );
	
	m_buttonClose = new wxButton( this, wxID_OK, _("Close"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_buttonClose->SetDefault(); 
	bSizerStdButtons->Add( m_buttonClose, 0, wxALL|wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5 );
	
	
	bSizer31->Add( bSizerStdButtons, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer31 );
	this->Layout();
	bSizer31->Fit( this );
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( AboutDlgGenerated::OnClose ) );
	m_buttonDonate->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::OnDonate ), NULL, this );
	m_buttonClose->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AboutDlgGenerated::OnOK ), NULL, this );
}

AboutDlgGenerated::~AboutDlgGenerated()
{
}
